#include "Arduino.h"
#include "string.h"

#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"

#include "wificonfig.h"
#include "index.h"
#include "favicon.h"

char* index_html = reinterpret_cast<char*>(&web_index_html[0]);
char* favicon = reinterpret_cast<char*>(&web_favicon_ico[0]);

ESP8266WebServer _server(80);

const uint8_t _FIRST_LED_INDEX = 1;
const uint8_t _NUMBER_OF_LEDS = 51;
const uint8_t _DATA_BYTE_LENGTH = _NUMBER_OF_LEDS * 4;
float _max_current = 400; // mA
uint32_t _max_led_sum = (uint32_t)(_max_current * 255.0 / 20); // S/255*20mA < I_max (mA)

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

inline void setInitialLEDData(int index, uint8_t R, uint8_t G, uint8_t B, uint8_t W = 0) {

    int i = 4 * index;
    _mode_data.initial_data[i + 0] = G;
    _mode_data.initial_data[i + 1] = R;
    _mode_data.initial_data[i + 2] = B;
    _mode_data.initial_data[i + 3] = W;

}

inline void setLEDData(int index, uint8_t R, uint8_t G, uint8_t B, uint8_t W = 0) {

    int i = 4 * index;
    _led_data[i + 0] = G;
    _led_data[i + 1] = R;
    _led_data[i + 2] = B;
    _led_data[i + 3] = W;

}

const int _GPIO_PIN = 0;

enum _MODE {

    MULTICOLOR_TWINKLE,
    REAL_WHITE_CONSTANT,
    REAL_WHITE_GLOW,
    RGB_WHITE_CONSTANT,
    SINGLE_COLOR_BOUNCE,
    SINGLE_COLOR_GLOW,
    SINGLE_COLOR_TWINKLE

};
_MODE _mode;

// Maps strings to corresponding enum.
static _MODE getModeEnum(String s) {

    if (s == "MULTICOLOR_TWINKLE") { return _MODE::MULTICOLOR_TWINKLE; };
    if (s == "REAL_WHITE_CONSTANT") { return _MODE::REAL_WHITE_CONSTANT; };
    if (s == "REAL_WHITE_GLOW") { return _MODE::REAL_WHITE_GLOW; };
    if (s == "RGB_WHITE_CONSTANT") { return _MODE::RGB_WHITE_CONSTANT; };
    if (s == "SINGLE_COLOR_BOUNCE") { return _MODE::SINGLE_COLOR_BOUNCE; };
    if (s == "SINGLE_COLOR_GLOW") { return _MODE::SINGLE_COLOR_GLOW; };
    if (s == "SINGLE_COLOR_TWINKLE") { return _MODE::SINGLE_COLOR_TWINKLE; };
    return _mode;

}

// Maps enums to correspongind strings.
static String getModeString(_MODE m) {

    switch (m) {
        case MULTICOLOR_TWINKLE: return "MULTICOLOR_TWINKLE";
        case REAL_WHITE_CONSTANT: return "REAL_WHITE_CONSTANT";
        case REAL_WHITE_GLOW: return "REAL_WHITE_GLOW";
        case RGB_WHITE_CONSTANT: return "RGB_WHITE_CONSTANT";
        case SINGLE_COLOR_BOUNCE: return "SINGLE_COLOR_BOUNCE";
        case SINGLE_COLOR_GLOW: return "SINGLE_COLOR_GLOW";
        case SINGLE_COLOR_TWINKLE: return "SINGLE_COLOR_TWINKLE";
    }
    return "ERROR";

}

// To measure how many cycles operations take.
static inline int32_t asm_ccount(void) {

    int32_t r;
    asm volatile ("rsr %0, ccount" : "=r"(r));
    return r;

}

template<int T>
inline void nop_delay() __attribute__((always_inline));

// One nop is 12.5 ns @ 80 MHz.
template<>
void nop_delay<0>()
{

    asm volatile ( "nop":: );

}

template<int T>
void nop_delay()
{

    asm volatile ( "nop":: );
    nop_delay<T - 1>();

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

void switchMode(_MODE new_mode) {

    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    uint8_t max_rgb;
    for (int i = 0; i < _NUMBER_OF_LEDS; i++ ) {
        setLEDData(i, 0, 0, 0, 0);
    }
    _mode = new_mode;
    _mode_data.led_index = 0;
    _mode_data.phase = 0;
    _mode_data.special = false;
    _mode_data.brightness = 1.0;
    Serial.print("Switching mode to: ");
    Serial.println(getModeString(_mode));

    switch (_mode) {

        case SINGLE_COLOR_BOUNCE:
            _mode_data.interval = 25;
            r = random(256);
            g = random(256);
            b = random(256);
            max_rgb = max(r, max(g, b));
            r = (uint8_t) r * 256.0 / max_rgb;
            g = (uint8_t) g * 256.0 / max_rgb;
            b = (uint8_t) b * 256.0 / max_rgb;
            // 10% change of white.
            if (random(10) < 0) {
                r = g = b = 0;
                w = 255;
            }
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitialLEDData(i, r, g, b, w);
            }
            break;

        case SINGLE_COLOR_GLOW:
            _mode_data.interval = 10;
            r = random(256);
            g = random(256);
            b = random(256);
            max_rgb = max(r, max(g, b));
            r = (uint8_t) r * 256.0 / max_rgb;
            g = (uint8_t) g * 256.0 / max_rgb;
            b = (uint8_t) b * 256.0 / max_rgb;
            w = 0;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                _mode_data.phase_offset[i] = PI * (i - _FIRST_LED_INDEX) / (float) _NUMBER_OF_LEDS;
                setInitialLEDData(i, r, g, b, w);
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
            r = random(256);
            g = random(256);
            b = random(256);
            w = 0;
            for (int i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                _mode_data.phase_offset[i] = PI * 0.01 * random(101);
                setInitialLEDData(i, r, g, b, w);
            }
            break;

        case REAL_WHITE_CONSTANT:
            _mode_data.interval = 1000;
            break;

        case RGB_WHITE_CONSTANT:
            _mode_data.interval = 1000;
            break;
    }

    gpioDown();
    delay(1);

}

inline void updateData() {

    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    int i;
    uint8_t max_br = (uint8_t)(_mode_data.brightness * 255);

    _mode_data.phase += 0.01;
    uint16_t sum = 0; // Assuming a maximum value of 255/LED * 4 LEDs/module * 60 modules = 61200.

    switch (_mode) {

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
                double br = pow((float)sin(_mode_data.phase + _mode_data.phase_offset[n]), 6) * _mode_data.brightness;
                w = (uint8_t)(br * 255);
                setLEDData(n, 0, 0, 0, w);
                sum += w;
            }
            break;

        case SINGLE_COLOR_GLOW:
        case MULTICOLOR_TWINKLE:
        case SINGLE_COLOR_TWINKLE:
            for (int n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                i = 4 * n;
                double br = pow((float)sin(_mode_data.phase + _mode_data.phase_offset[n]), 6) * _mode_data.brightness;
                g = (uint8_t)(br * _mode_data.initial_data[i + 0]);
                r = (uint8_t)(br * _mode_data.initial_data[i + 1]);
                b = (uint8_t)(br * _mode_data.initial_data[i + 2]);
                w = (uint8_t)(br * _mode_data.initial_data[i + 3]);
                setLEDData(n, r, g, b, w);
                sum += r + g + b + w;
            }
            break;

        case REAL_WHITE_CONSTANT:
            for (int n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                setLEDData(n, 0, 0, 0, max_br);
            }
            sum += (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * max_br;
            break;

        case RGB_WHITE_CONSTANT:
            for (int n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                setLEDData(n, max_br, max_br, max_br, 0);
            }
            sum += (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 3 * max_br;
            break;
    }

    if (sum > _max_led_sum) {
        float new_brightness = _max_led_sum / (float)sum;
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

void setupWifi() {

    IPAddress subnet(255, 255, 255, 0);
    WiFi.mode(WIFI_STA);

    Serial.println("");
    if (ip == INADDR_NONE) {
        Serial.println("Using dynamic IP address.");
    } else {
        Serial.print("Using static IP address: ");
        Serial.println(ip.toString());
        WiFi.config(ip, gateway, subnet);
    }

    WiFi.begin(ssid, password);
    int timeout = 0;
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED && timeout < 10) {
        delay(1000);
        Serial.print(".");
        timeout++;
    }
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected using IP address: ");
        Serial.println(WiFi.localIP());
    }

    _server.on("/", HTTP_GET, [](){
        Serial.println("GET: /");
        _server.send(200, "text/html", index_html);
    });
    _server.on("/favicon.ico", HTTP_GET, [](){
        Serial.println("GET: /favicon.ico");
        _server.send_P(200, "image/x-icon", favicon, web_favicon_ico_len);
    });
    _server.on("/", HTTP_POST, [](){
        Serial.println("POST: /");
        _server.send(200, "text/plain", "");
        String mode = _server.arg("mode");
        Serial.print("Got request for new mode: ");
        Serial.println(mode);
        switchMode(getModeEnum(mode));
    });

    _server.begin();
    Serial.println("Server is running.");

}

void setup() {

    Serial.begin(115200);
    pinMode(_GPIO_PIN, OUTPUT);

    setupWifi();

    switchMode(REAL_WHITE_CONSTANT);

}

void loop() {

    _server.handleClient();
    updateData();

    noInterrupts();
    for (uint8_t* cursor = _led_data; cursor < _led_data + _DATA_BYTE_LENGTH; cursor++ ) {
        for (int8_t bit = 7; bit >= 0; bit-- ) {
            if ((*cursor & (1 << bit)) == 0) {
                // Write zero; "manually inlined".
                gpioUp();
                nop_delay<15>();
                gpioDown();
                nop_delay<60>();
            } else {
                // Write one; "manually inlined".
                gpioUp();
                nop_delay<39>();
                gpioDown();
                nop_delay<36>();
            }
        }
    }
    interrupts();

    delay(_mode_data.interval);

}
