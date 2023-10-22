#include <Arduino.h>
#include <string.h>
#include "webconfig.h"

// The first LED index is normally 0, but if using the "sacrificial LED" technique to boost the IO voltage, it's 1.
// A current limit of 400mA is usually tolerable by D1 mini powered via USB. For higher values, power the LEDs directly from the 5V line.
#include "led_cfg.h"

extern "C" void write(byte* led_val, unsigned long int length);

const byte _DATA_BYTE_LENGTH = _NUMBER_OF_LEDS * 4;
// LED_SUM is the sum of all LED values (on a scale 0-255), so e.g. for 4 LEDs/module, 60 modules/strip the maximum sum is 255*4*60=61200.
// Assuming for each LED the current is 20mA, calculate the safe limit for LED sum based on current limit.
const unsigned long int _LED_SUM_LIMIT = (unsigned long int)(_CURRENT_LIMIT_MA * 255.0 / 20); // LED_SUM/255*20mA < I_limit (mA)

struct LEDStripData {

    byte led_val[_DATA_BYTE_LENGTH] = { 0 };
    byte init_val[_DATA_BYTE_LENGTH] = { 0 };
    unsigned int led_index = _FIRST_LED_INDEX;
    unsigned int interval = 100;
    float phase_offset[_NUMBER_OF_LEDS] = { 0 };
    float phase = 0.0;
    float brightness = 1.0;
    bool special = false;

};
LEDStripData _data;

inline void setInitValues(byte index, byte r = 0, byte g = 0, byte b = 0, byte w = 0, float phase_offset = 0) {

    unsigned int i = 4 * index;
    _data.init_val[i + 0] = g;
    _data.init_val[i + 1] = r;
    _data.init_val[i + 2] = b;
    _data.init_val[i + 3] = w;
    _data.led_val[i + 0] = g;
    _data.led_val[i + 1] = r;
    _data.led_val[i + 2] = b;
    _data.led_val[i + 3] = w;

    _data.phase_offset[index] = phase_offset;

}

inline void setValues(byte index, byte r = 0, byte g = 0, byte b = 0, byte w = 0) {

    unsigned int i = 4 * index;
    _data.led_val[i + 0] = g;
    _data.led_val[i + 1] = r;
    _data.led_val[i + 2] = b;
    _data.led_val[i + 3] = w;

}

const byte _GPIO_PIN = D3; // = 0 on D1 mini

enum _MODE {

    HUE_ROTATE_CONSTANT,
    HUE_ROTATE_VARIABLE,
    MULTICOLOR_TWINKLE,
    MULTICOLOR_SINGLE_TWINKLE,
    REAL_WHITE_CONSTANT,
    REAL_WHITE_CONSTANT_50,
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
_MODE getModeEnum(String s) {

    if (s == "HUE_ROTATE_CONSTANT") { return _MODE::HUE_ROTATE_CONSTANT; };
    if (s == "HUE_ROTATE_VARIABLE") { return _MODE::HUE_ROTATE_VARIABLE; };
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
String getModeString(_MODE m) {

    switch (m) {
        case HUE_ROTATE_CONSTANT: return "HUE_ROTATE_CONSTANT";
        case HUE_ROTATE_VARIABLE: return "HUE_ROTATE_VARIABLE";
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

// Return info on whether a mode uses a specific color value or not.
bool modeUsesSpecificColor(_MODE m) {

    switch (m) {
        case HUE_ROTATE_CONSTANT:
        case HUE_ROTATE_VARIABLE:
        case MULTICOLOR_TWINKLE:
        case MULTICOLOR_SINGLE_TWINKLE:
        case REAL_WHITE_CONSTANT:
        case REAL_WHITE_CONSTANT_50:
        case REAL_WHITE_GLOW:
        case RGB_WHITE_CONSTANT:
        case OFF:
            return false;
        case SINGLE_COLOR_CONSTANT:
        case SINGLE_COLOR_BOUNCE:
        case SINGLE_COLOR_GLOW:
        case SINGLE_COLOR_TWINKLE:
        case SINGLE_COLOR_SINGLE_TWINKLE:
            return true;
    }
    return false;

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

// Scale brightness based on LED brightness sum so that current stays within limits.
inline void scaleBrightness(unsigned int sum) {
    if (sum > _LED_SUM_LIMIT) {
        float new_brightness = _LED_SUM_LIMIT / (float)sum;
        if (new_brightness < _data.brightness) {
            Serial.print("Current limit scaling brightness to: ");
            Serial.println(new_brightness);
            _data.brightness = new_brightness;
            for (byte i = 0; i < _NUMBER_OF_LEDS * 4; i += 4 ) {
                _data.led_val[i + 0] = (byte)(_data.led_val[i + 0] * new_brightness);
                _data.led_val[i + 1] = (byte)(_data.led_val[i + 1] * new_brightness);
                _data.led_val[i + 2] = (byte)(_data.led_val[i + 2] * new_brightness);
                _data.led_val[i + 3] = (byte)(_data.led_val[i + 3] * new_brightness);
            }
        }
    }

}

void randomColor(byte &r, byte &g, byte &b) {
    byte max_rgb;
    r = random(256);
    g = random(256);
    b = random(256);
    max_rgb = max(r, max(g, b));
    r = (byte) r * 255.0 / max_rgb;
    g = (byte) g * 255.0 / max_rgb;
    b = (byte) b * 255.0 / max_rgb;
}

void hueToRGB(float hue, byte &r, byte &g, byte &b) {
    hue /= 60.0;
    unsigned int hue_integer = (unsigned int) hue;
    float hue_fraction = hue - hue_integer;
    byte q = 255 * (1 - hue_fraction);
    byte t = 255 * (1 - (1 - hue_fraction));
    switch (hue_integer) {
        case 0:  r = 255; g = t;   b = 0; break;
        case 1:  r = q;   g = 255; b = 0; break;
        case 2:  r = 0;   g = 255; b = t; break;
        case 3:  r = 0;   g = q;   b = 255; break;
        case 4:  r = t;   g = 0;   b = 255; break;
        default: r = 255; g = 0;   b = q; break;
    }
}

void switchMode(_MODE new_mode, byte r = 0, byte g = 0, byte b = 0, byte w = 0) {

    Serial.print("Switching mode to: ");
    Serial.println(getModeString(new_mode));

    _mode = new_mode;
    _data.led_index = 0;
    _data.phase = 0;
    _data.special = false;
    _data.brightness = 1.0;
    _data.interval = 1000;
    unsigned int sum = 0;

    if (modeUsesSpecificColor(new_mode)) {
        if (r != 0 || g != 0 || b != 0 || w != 0) {
            Serial.println("Using given color: ");
        } else {
            Serial.println("Using random color: ");
            randomColor(r, g, b);
        }
        Serial.print("   R: "); Serial.println(r);
        Serial.print("   G: "); Serial.println(g);
        Serial.print("   B: "); Serial.println(b);
        Serial.print("   W: "); Serial.println(w);
    }

    switch (_mode) {

        case OFF:
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i);
            }
            break;

        case HUE_ROTATE_CONSTANT:
        case HUE_ROTATE_VARIABLE:
            _data.interval = 10;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, 0, 255, 255);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 255;
            break;

        case SINGLE_COLOR_CONSTANT:
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, r, g, b);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * (r + g + b);
            break;

        case SINGLE_COLOR_BOUNCE:
            _data.interval = 25;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, r, g, b);
                setValues(i);
            }
            break;

        case SINGLE_COLOR_GLOW:
            _data.interval = 10;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, r, g, b, 0, PI * (i - _FIRST_LED_INDEX) / (float) _NUMBER_OF_LEDS);
            }
            break;

        case REAL_WHITE_GLOW:
            _data.interval = 10;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, 0, 0, 0, 0, PI * (i - _FIRST_LED_INDEX) / (float) _NUMBER_OF_LEDS);
            }
            break;

        case MULTICOLOR_TWINKLE:
            _data.interval = 10;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, random(256), random(256), random(256), random(256), PI * 0.01 * random(101));
            }
            break;

        case SINGLE_COLOR_TWINKLE:
            _data.interval = 10;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, r, g, b, 0, PI * 0.01 * random(101));
            }
            break;

        case MULTICOLOR_SINGLE_TWINKLE:
        case SINGLE_COLOR_SINGLE_TWINKLE:
            _data.interval = 100;
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                if (_mode == MULTICOLOR_SINGLE_TWINKLE) {
                    randomColor(r, g, b);
                }
                setInitValues(i, r, g, b, 0, PI * 0.01 * random(1001));
                setValues(i);
            }
            break;

        case REAL_WHITE_CONSTANT:
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, 0, 0, 0, 255);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 255;
            break;

        case REAL_WHITE_CONSTANT_50:
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i += 2) {
                setInitValues(i, 0, 0, 0, 255);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * 255 / 2;
            break;

        case RGB_WHITE_CONSTANT:
            for (byte i = _FIRST_LED_INDEX; i < _NUMBER_OF_LEDS; i++ ) {
                setInitValues(i, 255, 255, 255, 0);
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

    byte r;
    byte g;
    byte b;
    byte w;
    unsigned int i;
    double br;
    unsigned int sum = 0;
    _data.phase += 0.01;

    // Null the phase after 1000*2*PI to keep things predictable.
    if (_data.phase > 6283.18) {
        _data.phase = 0.0;
    }

    switch (_mode) {

        case OFF:
            break;

        case HUE_ROTATE_VARIABLE:
            _data.interval += _data.special ? random(10) : -random(10);
            _data.interval = max(1, (int)_data.interval);
            if ((unsigned int)random(200) < _data.interval) {
                _data.special = !_data.special;
            }
        case HUE_ROTATE_CONSTANT:
            if (_data.phase > 36.0) {
                _data.phase = 0.0;
            }
            hueToRGB(_data.phase * 10.0, r, g, b);
            for (byte n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++ ) {
                setValues(n, r, g, b);
            }
            sum = (_NUMBER_OF_LEDS - _FIRST_LED_INDEX) * (r + g + b);
            break;

        case SINGLE_COLOR_BOUNCE:
            setValues(_data.led_index);
            // Special here reverses the direction.
            _data.special ? _data.led_index-- : _data.led_index++;
            _data.special |= _data.led_index == _NUMBER_OF_LEDS - 1;
            _data.special &= _data.led_index != _FIRST_LED_INDEX;
            i = 4 * _data.led_index;
            g = (byte)(_data.init_val[i + 0]);
            r = (byte)(_data.init_val[i + 1]);
            b = (byte)(_data.init_val[i + 2]);
            w = (byte)(_data.init_val[i + 3]);
            setValues(_data.led_index, r, g, b, w);
            sum = r + g + b + w;
            break;

        case REAL_WHITE_GLOW:
            for (byte n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                br = pow((float)sin(_data.phase + _data.phase_offset[n]), 6) * _data.brightness;
                w = (byte)(br * 255);
                setValues(n, 0, 0, 0, w);
                sum += w;
            }
            break;

        case SINGLE_COLOR_GLOW:
        case MULTICOLOR_TWINKLE:
        case SINGLE_COLOR_TWINKLE:
            for (byte n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                br = pow((float)sin(_data.phase + _data.phase_offset[n]), 6) * _data.brightness;
                i = 4 * n;
                g = (byte)(br * _data.init_val[i + 0]);
                r = (byte)(br * _data.init_val[i + 1]);
                b = (byte)(br * _data.init_val[i + 2]);
                w = (byte)(br * _data.init_val[i + 3]);
                setValues(n, r, g, b, w);
                sum += r + g + b + w;
            }
            break;

        case MULTICOLOR_SINGLE_TWINKLE:
        case SINGLE_COLOR_SINGLE_TWINKLE:
            for (byte n = _FIRST_LED_INDEX; n < _NUMBER_OF_LEDS; n++) {
                _data.phase_offset[n] += 0.01;
                if (_data.phase_offset[n] > 10 * PI) {
                    if (_mode == MULTICOLOR_SINGLE_TWINKLE) {
                        randomColor(r, g, b);
                        setInitValues(n, r, g, b, 0);
                    }
                    _data.phase_offset[n] = PI * 0.01 * random(501);
                    setValues(n, 0, 0, 0, 0);
                } else if (_data.phase_offset[n] > 9 * PI) {
                    br = pow((float)sin(_data.phase_offset[n]), 4) * _data.brightness;
                    i = 4 * n;
                    g = (byte)(br * _data.init_val[i + 0]);
                    r = (byte)(br * _data.init_val[i + 1]);
                    b = (byte)(br * _data.init_val[i + 2]);
                    w = (byte)(br * _data.init_val[i + 3]);
                    setValues(n, r, g, b, w);
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

void requestMode(String new_mode, byte r, byte g, byte b) {
    Serial.print("Request mode: ");
    Serial.println(new_mode);
    switchMode(getModeEnum(new_mode), r, g, b);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(_GPIO_PIN, OUTPUT);
    WebConfig::setupWifi(requestMode);
    switchMode(REAL_WHITE_CONSTANT_50);
}

void loop() {
    WebConfig::handleClient();
    updateData();
    write(_data.led_val, _DATA_BYTE_LENGTH);
    delay(_data.interval);
}
