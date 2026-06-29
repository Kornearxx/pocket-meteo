#pragma once

#include <Arduino.h>

// --- Waveshare ESP32-S3-LCD-2 ---
// LCD: ST7789T3 240×320 (SPI3 / HSPI), I2C: GPIO47/48 (QMI8658 + внешние датчики)

// BMP280 на общей I2C-шине платы
constexpr uint8_t BMP_SDA     = 48;
constexpr uint8_t BMP_SCL     = 47;
constexpr uint8_t BMP_ADDR    = 0x77;

// Внешние периферийные пины (28-pin header)
constexpr uint8_t TILT_PIN    = 2;
constexpr uint8_t JOY_X_PIN   = 4;
constexpr uint8_t JOY_Y_PIN   = 6;
constexpr uint8_t JOY_BTN_PIN = 9;
constexpr uint8_t RGB_LED_PIN = 15; // Опциональный WS2812 на шине

// LCD ST7789T3 (встроенный SPI-дисплей)
constexpr uint8_t TFT_CS      = 45;
constexpr uint8_t TFT_DC      = 42;
constexpr int8_t  TFT_RST     = -1; // RST не выведен — сброс через EN
constexpr uint8_t TFT_BL      = 1;
constexpr uint8_t TFT_SCLK    = 39;
constexpr uint8_t TFT_MOSI    = 38;
constexpr int8_t  TFT_MISO    = 40;
constexpr uint8_t TFT_BRIGHTNESS = 75;

// --- Настройки ---
constexpr float ALTITUDE_M         = 150.0f;
constexpr int MAX_BUFFER_SIZE      = 1200;
constexpr uint32_t TREND_LOOKBACK_SEC = 30;
constexpr float TREND_THRESHOLD    = 0.2f;
constexpr uint32_t DEBOUNCE_MS     = 50;

// --- WiFi ---
constexpr char AP_SSID[] = "PocketMeteo";
