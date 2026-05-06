#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>

namespace lgfx {
inline namespace v1 {

struct Touch_AXS15231B : public ITouch {
    static constexpr uint8_t i2c_addr = 0x3B;

    Touch_AXS15231B(void) {
        _cfg.i2c_addr = i2c_addr;
        _cfg.x_min = 0;
        _cfg.x_max = 319;
        _cfg.y_min = 0;
        _cfg.y_max = 479;
        _cfg.freq = 400000;
        _cfg.pin_int = -1;
        _cfg.pin_rst = -1;
    }

    bool init(void) override {
        _inited = false;
        if (isSPI()) {
            return false;
        }

        if (_cfg.pin_rst >= 0) {
            lgfx::pinMode(_cfg.pin_rst, pin_mode_t::output);
            lgfx::gpio_lo(_cfg.pin_rst);
            lgfx::delay(10);
            lgfx::gpio_hi(_cfg.pin_rst);
            lgfx::delay(10);
        }

        if (_cfg.pin_int >= 0) {
            lgfx::pinMode(_cfg.pin_int, pin_mode_t::input_pullup);
        }

        if (_cfg.pin_sda >= 0 && _cfg.pin_scl >= 0) {
            Wire.begin(_cfg.pin_sda, _cfg.pin_scl);
        } else {
            Wire.begin();
        }
        Wire.setClock(_cfg.freq);

        Wire.beginTransmission(_cfg.i2c_addr);
        _inited = (Wire.endTransmission() == 0);
        return _inited;
    }

    void wakeup(void) override {}
    void sleep(void) override {}

    uint_fast8_t getTouchRaw(touch_point_t* tp, uint_fast8_t count) override {
        if (!_inited || count == 0 || tp == nullptr) {
            return 0;
        }

        constexpr uint8_t max_touch_points = 1;
        constexpr uint8_t data_len = (max_touch_points * 6) + 2;
        constexpr uint8_t cmd_len = 11;
        const uint8_t read_cmd[cmd_len] = {
            0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00,
            static_cast<uint8_t>(data_len >> 8),
            static_cast<uint8_t>(data_len & 0xFF),
            0x00, 0x00, 0x00
        };

        uint8_t data[data_len] = {0};

        Wire.beginTransmission(_cfg.i2c_addr);
        Wire.write(read_cmd, cmd_len);
        if (Wire.endTransmission() != 0) {
            return 0;
        }

        if (Wire.requestFrom(_cfg.i2c_addr, data_len) != data_len) {
            return 0;
        }

        for (uint8_t i = 0; i < data_len; ++i) {
            data[i] = Wire.read();
        }

        uint8_t touches = data[1];
        if (touches == 0) {
            return 0;
        }

        uint8_t* p = data + 2;
        uint8_t event = p[0] >> 4;
        if (event != 0x04 && event != 0x08) {
            return 0;
        }

        uint16_t raw_x = static_cast<uint16_t>(((p[0] & 0x0F) << 8) | p[1]);
        uint16_t raw_y = static_cast<uint16_t>(((p[2] & 0x0F) << 8) | p[3]);

        if (raw_x > _cfg.x_max || raw_y > _cfg.y_max) {
            return 0;
        }

        tp[0].id = 0;
        tp[0].x = raw_x;
        tp[0].y = raw_y;
        tp[0].size = p[4];

        return 1;
    }
};

} // namespace v1
} // namespace lgfx
