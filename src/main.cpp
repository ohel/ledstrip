#include "Arduino.h"
#include "string.h"

#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "ESP8266mDNS.h"

#include "wificonfig.h"
#include "index.h"
#include "favicon.h"

#include "ledsetup.h"
#ifndef _LED_SETUP
// For different kinds of LED strip setups, put the next three lines in ledsetup.h.
// The first LED index is normally 0, but if using the "sacrificial LED" technique to boost the IO voltage, it's 1.
#define _LED_SETUP
const uint8_t _FIRST_LED_INDEX = 0;
const uint8_t _NUMBER_OF_LEDS = 60;
#endif

extern "C" void write(uint8_t* led_data, uint32_t length);

const char* _INDEX_HTML = reinterpret_cast<char*>(&web_index_html[0]);
const char* _FAVICON = reinterpret_cast<char*>(&web_favicon_ico[0]);
const unsigned int _WEB_FAVICON_LEN = web_favicon_ico_len;

ESP8266WebServer _SERVER(80);

const uint8_t _DATA_BYTE_LENGTH = _NUMBER_OF_LEDS * 4;
const float _MAX_CURRENT = 400; // mA
const uint32_t _MAX_LED_SUM = (uint32_t)(_MAX_CURRENT * 255.0 / 20); // S/255*20mA < I_max (mA)

uint8_t _led_data[_DATA_BYTE_LENGTH] = { 0 };

struct _ModeData {

    uint8_t initial_data[_DATA_BYTE_LENGTH] = { 0 };
    int led_index = _FIRST_LED_INDEX;
    int interval = 100;
    float phase_offset[_NUMBER_OF_LEDS] = { 0 };
    float phase = 0.0;
    float brightness = 1.0;
    bool special = false;

};
_ModeData _mode_data;

inline void setInitialLEDData(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {

    int i = 4 * index;
    _mode_data.initial_data[i + 0] = g;
    _mode_data.initial_data[i + 1] = r;
    _mode_data.initial_data[i + 2] = b;
    _mode_data.initial_data[i + 3] = w;
    _led_data[i + 0] = g;
    _led_data[i + 1] = r;
    _led_data[i + 2] = b;
    _led_data[i + 3] = w;

}

inline void setLEDData(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {

    int i = 4 * index;
    _led_data[i + 0] = g;
    _led_data[i + 1] = r;
    _led_data[i + 2] = b;
    _led_data[i + 3] = w;

}

const int _GPIO_PIN = 0;

enum _MODE {

    MULTICOLOR_TWINKLE,
    MULTICOLOR_SINGLE_TWINKLE,
    REAL_WHITE_CONSTANT,
    REAL_WHITE_CONSTANT_50, // Sometimes limiting brightness with PWM makes the LEDs flicker.
    REAL_WHITE_GLOW,
    RGB_WHITE_CONSTANT,
    SINGLE_COLOR_CONSTANT,
    SINGLE_COLOR_BOUNCE,
    SINGLE_COLOR_GLOW,
    SINGLE_COLOR_TWINKLE,
    SINGLE_COLOR_SINGLE_TWINKLE,
    OFF

};
_MODE _mode;

// Maps strings to corresponding enum.
static _MODE getModeEnum(String s) {

    if (s == "MULTICOLOR_TWINKLE") { return _MODE::MULTICOLOR_TWINKLE; };
    if (s == "MULTICOLOR_SINGLE_TWINKLE") { return _MODE::MULTICOLOR_SINGLE_TWINKLE; };
    if (s == "REAL_WHITE_CONSTANT") { return _MODE::REAL_WHITE_CONSTANT; };
    if (s == "REAL_WHITE_CONSTANT_50") { return _MODE::REAL_WHITE_CONSTANT_50; };
    if (s == "REAL_WHITE_GLOW") { return _MODE::REAL_WHITE_GLOW; };
    if (s == "RGB_WHITE_CONSTANT") { return _MODE::RGB_WHITE_CONSTANT; };
    if (s == "SINGLE_COLOR_CONSTANT") { return _MODE::SINGLE_COLOR_CONSTANT; };
    if (s == "SINGLE_COLOR_BOUNCE") { return _MODE::SINGLE_COLOR_BOUNCE; };
    if (s == "SINGLE_COLOR_GLOW") { return _MODE::SINGLE_COLOR_GLOW; };
    if (s == "SINGLE_COLOR_TWINKLE") { return _MODE::SINGLE_COLOR_TWINKLE; };
    if (s == "SINGLE_COLOR_SINGLE_TWINKLE") { return _MODE::SINGLE_COLOR_SINGLE_TWINKLE; };
    if (s == "OFF") { return _MODE::OFF; };
    return _mode;

}

// Maps enums to corresponding strings.
static String getModeString(_MODE m) {

    switch (m) {
        case MULTICOLOR_TWINKLE: return "MULTICOLOR_TWINKLE";
        case MULTICOLOR_SINGLE_TWINKLE: return "MULTICOLOR_SINGLE_TWINKLE";
        case REAL_WHITE_CONSTANT: return "REAL_WHITE_CONSTANT";
        case REAL_WHITE_CONSTANT_50: return "REAL_WHITE_CONSTANT_50";
        case REAL_WHITE_GLOW: return "REAL_WHITE_GLOW";
        case RGB_WHITE_CONSTANT: return "RGB_WHITE_CONSTANT";
        case SINGLE_COLOR_CONSTANT: return "SINGLE_COLOR_CONSTANT";
        case SINGLE_COLOR_BOUNCE: return "SINGLE_COLOR_BOUNCE";
        case SINGLE_COLOR_GLOW: return "SINGLE_COLOR_GLOW";
        case SINGLE_COLOR_TWINKLE: return "SINGLE_COLOR_TWINKLE";
        case SINGLE_COLOR_SINGLE_TWINKLE: return "SINGLE_COLOR_SINGLE_TWINKLE";
        case OFF: return "OFF";
    }
    return "ERROR";

}

// Writing the PIN register takes 9 cycles = 112.5 ns @ 80 MHz.
inline void gpioUp() __attribute__((always_inline));
void gpioUp() {

    GPOS = (1 << _GPIO_PIN);

}

inline void gpioDown() __attribute__((always_inline));
void gpioDown() {

    GPOC = (1 << _GPIO_PIN);

}

// Assuming a maximum value of 255/LED * 4 LEDs/module * 60 modules = 61200,
// scale brightness based on LED brightness sum so that current stays within limits.
inline void scaleBrightness(uint16_t sum) {

    if (sum > _MAX_LED_SUM) {
        float new_brightness = _MAX_LED_SUM / (float)sum;
        if (new_brightness < _mode_data.brightness) {
            Serial.print("Scaling brightness to: ");
            Serial.println(new_brightness);
            _mode_data.brightness = new_brightness;
            for (int i = 0; i < _NUMBER_OF_LEDS * 4; i += 4 ) {
                _led_data[i + 0] = (uint8_t)(_led_data[i + 0] * new_brightness);
                _led_data[i + 1] = (uint8_t)(_led_data[i + 1] * new_brightness);
                _led_data[i + 2] = (uint8_t)(_led_data[i + 2] * new_brightness);
                _led_data[i + 3] = (uint8_t)(_led_data[i + 3] * new_brightness);
            }
        }
    }

}

void randomColor(uint8_t &r, uint8_t &g, uint8_t &b) {
    uint8_t max_rgb;
    r = random(256);
    g = random(256);
    b = random(256);
    max_rgb = max(r, max(g, b));
    r = (uint8_t) r * 256.0 / max_rgb;
    g = (uint8_t) g * 256.0 / max_rgb;
    b = (uint8_t) b * 256.0 / max_rgb;
}

void switchMode(_MODE new_mode, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t w = 0) {

    Serial.print("Switching mode to: ");
    Serial.println(getModeString(new_mode));

    for (int i = 0; i < _NUMBER_OF_LEDS; i++ ) {
        setLEDData(i, 0, 0, 0, 0);
        _mode_data.phase_offset[i] = 0;
    }
    _mode = new_mode;
    _mode_data.led_index = 0;
    _mode_data.phase = 0;
    _mode_data.special = false;
    _mode_data.brightness = 1.0;
    uint16_t sum = 0;

    if (r != 0 || g != 0 || b != 0 || w != 0) {
        Serial.println("Preferring given color: ");
    } else {
        Serial.println("Using random color: ");
        randomColor(r, g, b);
    }
    Serial.print("   R: "); Serial.println(r);
    Serial.print("   G: "); Serial.println(g);
    Serial.print("   B: "); Serial.println(b);
    Serial.print("   W: "); Serial.println(w);

    switch (_mode) {

        case OFF:
            _mode_data.interval = 1000;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitialLEDData(i, 0, 0, 0, 0);
            }
            break;

        case SINGLE_COLOR_CONSTANT:
            _mode_data.interval = 1000;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitialLEDData(i, r, g, b, 0);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * (r + g + b);
            break;

        case SINGLE_COLOR_BOUNCE:
            _mode_data.interval = 25;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitialLEDData(i, r, g, b, 0);
            }
            break;

        case SINGLE_COLOR_GLOW:
            _mode_data.interval = 10;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                _mode_data.phase_offset[i] = PI * (i - _FIRST_LED_INDEX) / (float) _NUMBER_OF_LEDS;
                setInitialLEDData(i, r, g, b, 0);
            }
            break;

        case REAL_WHITE_GLOW:
            _mode_data.interval = 10;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                _mode_data.phase_offset[i] = PI * (i - _FIRST_LED_INDEX) / (float) _NUMBER_OF_LEDS;
                setInitialLEDData(i, 0, 0, 0, 0);
            }
            break;

        case MULTICOLOR_TWINKLE:
            _mode_data.interval = 10;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                _mode_data.phase_offset[i] = PI * 0.01 * random(101);
                setInitialLEDData(i, random(256), random(256), random(256), random(256));
            }
            break;

        case SINGLE_COLOR_TWINKLE:
            _mode_data.interval = 10;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                _mode_data.phase_offset[i] = PI * 0.01 * random(101);
                setInitialLEDData(i, r, g, b, 0);
            }
            break;

        case MULTICOLOR_SINGLE_TWINKLE:
        case SINGLE_COLOR_SINGLE_TWINKLE:
            _mode_data.interval = 100;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                if (_mode == MULTICOLOR_SINGLE_TWINKLE) {
                    randomColor(r, g, b);
                }
                setInitialLEDData(i, r, g, b, 0);
                setLEDData(i, 0, 0, 0, 0);
                _mode_data.phase_offset[i] = PI * 0.01 * random(1001);
            }
            break;

        case REAL_WHITE_CONSTANT:
            _mode_data.interval = 1000;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitialLEDData(i, 0, 0, 0, 255);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 255;
            break;

        case REAL_WHITE_CONSTANT_50:
            _mode_data.interval = 1000;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i += 2) {
                setInitialLEDData(i, 0, 0, 0, 255);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 255 / 2;
            break;

        case RGB_WHITE_CONSTANT:
            _mode_data.interval = 1000;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitialLEDData(i, 255, 255, 255, 0);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 3 * 255;
            break;
    }

    // If sum is constant, set it in the switch case above.
    scaleBrightness(sum);
    gpioDown();
    delay(1);

}

inline void updateData() {

    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    int i;
    double br;
    uint16_t sum = 0;
    _mode_data.phase += 0.01;

    // Null the phase after 1000*2*Pi to keep things predictable.
    if (_mode_data.phase > 6283.1) {
        _mode_data.phase = 0.0;
    }

    switch (_mode) {

        case OFF:
            break;

        case SINGLE_COLOR_BOUNCE:
            setLEDData(_mode_data.led_index, 0, 0, 0);
            // Special here reverses the direction.
            _mode_data.special ? _mode_data.led_index-- : _mode_data.led_index++;
            _mode_data.special |= _mode_data.led_index == _NUMBER_OF_LEDS - 1;
            _mode_data.special &= _mode_data.led_index != _FIRST_LED_INDEX;
            i = 4 * _mode_data.led_index;
            g = (uint8_t)(_mode_data.initial_data[i + 0]);
            r = (uint8_t)(_mode_data.initial_data[i + 1]);
            b = (uint8_t)(_mode_data.initial_data[i + 2]);
            w = (uint8_t)(_mode_data.initial_data[i + 3]);
            setLEDData(_mode_data.led_index, r, g, b, w);
            sum = r + g + b + w;
            break;

        case REAL_WHITE_GLOW:
            for (int n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                br = pow((float)sin(_mode_data.phase + _mode_data.phase_offset[n]), 6) * _mode_data.brightness;
                w = (uint8_t)(br * 255);
                setLEDData(n, 0, 0, 0, w);
                sum += w;
            }
            break;

        case SINGLE_COLOR_GLOW:
        case MULTICOLOR_TWINKLE:
        case SINGLE_COLOR_TWINKLE:
            for (int n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                br = pow((float)sin(_mode_data.phase + _mode_data.phase_offset[n]), 6) * _mode_data.brightness;
                i = 4 * n;
                g = (uint8_t)(br * _mode_data.initial_data[i + 0]);
                r = (uint8_t)(br * _mode_data.initial_data[i + 1]);
                b = (uint8_t)(br * _mode_data.initial_data[i + 2]);
                w = (uint8_t)(br * _mode_data.initial_data[i + 3]);
                setLEDData(n, r, g, b, w);
                sum += r + g + b + w;
            }
            break;

        case MULTICOLOR_SINGLE_TWINKLE:
        case SINGLE_COLOR_SINGLE_TWINKLE:
            for (int n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                _mode_data.phase_offset[n] += 0.01;
                if (_mode_data.phase_offset[n] > 10 * PI) {
                    if (_mode == MULTICOLOR_SINGLE_TWINKLE) {
                        randomColor(r, g, b);
                        setInitialLEDData(n, r, g, b, 0);
                    }
                    _mode_data.phase_offset[n] = PI * 0.01 * random(501);
                    setLEDData(n, 0, 0, 0, 0);
                } else if (_mode_data.phase_offset[n] > 9 * PI) {
                    br = pow((float)sin(_mode_data.phase_offset[n]), 4) * _mode_data.brightness;
                    i = 4 * n;
                    g = (uint8_t)(br * _mode_data.initial_data[i + 0]);
                    r = (uint8_t)(br * _mode_data.initial_data[i + 1]);
                    b = (uint8_t)(br * _mode_data.initial_data[i + 2]);
                    w = (uint8_t)(br * _mode_data.initial_data[i + 3]);
                    setLEDData(n, r, g, b, w);
                    sum += r + g + b + w;
                }
            }
            break;

        case SINGLE_COLOR_CONSTANT:
        case REAL_WHITE_CONSTANT:
        case REAL_WHITE_CONSTANT_50:
        case RGB_WHITE_CONSTANT:
            // LED data has been already set and brightness scaled on init.
            break;
    }

    scaleBrightness(sum);

}

void setupWifi() {

    IPAddress subnet(255, 255, 255, 0);
    WiFi.mode(WIFI_STA);

    Serial.println("");
    if (_WIFI_IP == INADDR_NONE) {
        Serial.println("Using dynamic IP address.");
    } else {
        Serial.print("Using static IP address: ");
        Serial.println(_WIFI_IP.toString());
        WiFi.config(_WIFI_IP, _WIFI_GATEWAY, subnet);
    }
    WiFi.hostname(_WIFI_HOSTNAME);

    WiFi.begin(_WIFI_SSID, _WIFI_PASSWORD);
    int timeout = 0;
    Serial.print("Connecting to: ");
    Serial.println(_WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED && timeout < 10) {
        delay(1000);
        Serial.print(".");
        timeout++;
    }
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected using IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Hostname: ");
        Serial.println(WiFi.hostname());
    }

    _SERVER.on("/", HTTP_GET, [](){
        Serial.println("GET: /");
        _SERVER.send(200, "text/html", _INDEX_HTML);
    });
    _SERVER.on("/_FAVICON.ico", HTTP_GET, [](){
        Serial.println("GET: /_FAVICON.ico");
        _SERVER.send_P(200, "image/x-icon", _FAVICON, _WEB_FAVICON_LEN);
    });
    _SERVER.on("/", HTTP_POST, [](){
        Serial.println("POST: /");
        _SERVER.send(200, "text/plain", "");
        String mode = _SERVER.arg("mode");
        uint8_t r = (uint8_t) _SERVER.arg("r").toInt();
        uint8_t g = (uint8_t) _SERVER.arg("g").toInt();
        uint8_t b = (uint8_t) _SERVER.arg("b").toInt();
        Serial.print("Request mode: ");
        Serial.println(mode);
        switchMode(getModeEnum(mode), r, g, b);
    });

    _SERVER.begin();
    Serial.println("Server is running.");

    if (!MDNS.begin(_WIFI_HOSTNAME)) {
        Serial.println("Failed to start mDNS responder.");
    } else {
        Serial.println("Started mDNS responder.");
    }

}

void setup() {

    Serial.begin(115200);
    pinMode(_GPIO_PIN, OUTPUT);

    setupWifi();

    switchMode(REAL_WHITE_CONSTANT_50);

}

void loop() {

    _SERVER.handleClient();
    updateData();
    write(_led_data, _DATA_BYTE_LENGTH);
    delay(_mode_data.interval);

}
