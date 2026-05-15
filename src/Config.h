#pragma once

#include <Arduino.h>

// --- Пины ---
constexpr uint8_t BMP_SDA     = 0; // Пин 8 занят встроенным RGB-светодиодом, переносим SDA на свободный GPIO 0
constexpr uint8_t BMP_SCL     = 9;
constexpr uint8_t BMP_ADDR    = 0x77;
constexpr uint8_t TILT_PIN    = 5;
// --- Джойстик ---
constexpr uint8_t JOY_X_PIN   = 2; // Аналоговый X (ADC1)
constexpr uint8_t JOY_Y_PIN   = 3; // Аналоговый Y (ADC1)
constexpr uint8_t JOY_BTN_PIN = 4; // Кнопка джойстика
constexpr uint8_t RGB_LED_PIN = 8; // Пин встроенного RGB-светодиода (WS2812) на ESP32-C6-LCD-1.47

// Пины для LCD ST7789 на плате Waveshare ESP32-C6-LCD-1.47
constexpr uint8_t TFT_CS      = 14;
constexpr uint8_t TFT_DC      = 15;
constexpr uint8_t TFT_RST     = 21;
constexpr uint8_t TFT_BL      = 22; // Пин управления подсветкой LCD
constexpr uint8_t TFT_SCLK    = 7;
constexpr uint8_t TFT_MOSI    = 6;
constexpr int8_t  TFT_MISO    = -1;

// --- Настройки ---
constexpr float ALTITUDE_M         = 150.0f;
constexpr int MAX_BUFFER_SIZE      = 1200;
constexpr uint32_t TREND_LOOKBACK_SEC = 30;
constexpr float TREND_THRESHOLD    = 0.2f;
constexpr uint32_t DEBOUNCE_MS     = 50;

// --- WiFi ---
constexpr char AP_SSID[] = "PocketMeteo"; // Чуть удлинили имя для избежания кэширования старых сетей в телефоне