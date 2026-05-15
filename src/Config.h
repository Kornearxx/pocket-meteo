#pragma once

#include <Arduino.h>

// --- Пины ---
constexpr uint8_t BMP_SDA     = 0; // Пин 8 занят встроенным RGB-светодиодом, переносим SDA на свободный GPIO 0
constexpr uint8_t BMP_SCL     = 9;
constexpr uint8_t BMP_ADDR    = 0x77;
constexpr uint8_t TILT_PIN    = 5;
constexpr uint8_t RGB_LED_PIN = 8; // Пин встроенного RGB-светодиода (WS2812) на ESP32-C6-LCD-1.47

constexpr uint8_t EPD_CS      = 10;
constexpr uint8_t EPD_DC      = 4;
constexpr uint8_t EPD_RST     = 3;
constexpr uint8_t EPD_BUSY    = 2;
constexpr uint8_t EPD_SCLK    = 6;
constexpr uint8_t EPD_MOSI    = 7;
constexpr int8_t  EPD_MISO    = -1;

// --- Настройки ---
constexpr float ALTITUDE_M         = 150.0f;
constexpr int MAX_BUFFER_SIZE      = 1200;
constexpr uint32_t TREND_LOOKBACK_SEC = 30;
constexpr float TREND_THRESHOLD    = 0.2f;
constexpr uint32_t DEBOUNCE_MS     = 50;

// --- WiFi ---
constexpr char AP_SSID[] = "PocketMeteo"; // Чуть удлинили имя для избежания кэширования старых сетей в телефоне