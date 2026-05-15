/**
 * ============================================================================
 *  Pocket Weather Station v5 - Modular Architecture
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_NeoPixel.h>

#include "Config.h"
#include "WeatherModel.h"
#include "DisplayManager.h"
#include "WebController.h"

WeatherModel weatherModel;
DisplayManager displayManager;
WebController webController;
Adafruit_BMP280 bmp(&Wire); // Явно передаем нашу шину I2C
Adafruit_NeoPixel rgbLed(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Tilt ---
bool tilt_stable = false;
bool tilt_pending = false;
uint32_t tilt_last_ms = 0;

bool readBMP280(float* out_temp, float* out_press_hpa) {
    float t_sum = 0, p_sum = 0;
    for (uint8_t i = 0; i < 3; i++) {
        float t = bmp.readTemperature();
        float p = bmp.readPressure();
        
        // Защита от ошибочных данных (битые кадры I2C или обрыв линии)
        if (isnan(t) || isnan(p) || p == 0) return false;
        
        t_sum += t;
        p_sum += p / 100.0f; // Перевод Паскалей (Pa) в Гектопаскали (hPa)
        delay(10);
    }
    *out_temp = t_sum / 3;
    *out_press_hpa = p_sum / 3;
    return true;
}

void checkTiltSensor() {
    bool raw = (digitalRead(TILT_PIN) == LOW);
    if (raw != tilt_pending) { 
        tilt_pending = raw; 
        tilt_last_ms = millis(); 
    }
    if (millis() - tilt_last_ms >= DEBOUNCE_MS && raw != tilt_stable) {
        tilt_stable = raw;
        uint8_t new_rot = tilt_stable ? 3 : 1;
        if (new_rot != weatherModel.current_rotation) {
            weatherModel.current_rotation = new_rot;
            displayManager.setRotation(weatherModel.current_rotation);
            displayManager.setNeedsRedraw();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== BOOT: Pocket Station v6 (LCD C6) ===");
    
    pinMode(TILT_PIN, INPUT_PULLUP);
    
    rgbLed.begin();
    rgbLed.show(); // Выключаем RGB-диод при старте

    // Включаем подсветку TFT дисплея
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    Wire.begin(BMP_SDA, BMP_SCL);
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    
    Serial.print("[BMP280] ");
    // Fallback: пробуем сначала адрес из конфига (0x77), если не найден - пробуем 0x76
    if (bmp.begin(BMP_ADDR) || bmp.begin(0x76)) {
        // Профессиональные настройки для метеостанции (Indoor Navigation / Weather):
        // Максимальная фильтрация давления от аэродинамического шума (двери, ветер).
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Постоянные измерения */
                        Adafruit_BMP280::SAMPLING_X2,     /* Оверсэмплинг температуры x2 */
                        Adafruit_BMP280::SAMPLING_X16,    /* Оверсэмплинг давления x16 (ультравысокий) */
                        Adafruit_BMP280::FILTER_X16,      /* Мощный IIR-фильтр 16 */
                        Adafruit_BMP280::STANDBY_MS_500); /* Обновление данных раз в 500 мс */
        Serial.println("OK");
    } else {
        Serial.println("FAIL (Check wiring & I2C Address)");
    }
    
    Serial.print("[TFT] ");
    displayManager.init(weatherModel.current_rotation);
    Serial.println("OK");

    webController.begin(&weatherModel);
    Serial.println("[READY]");
}

void loop() {
    // Индикатор работы (мигает каждые 500 мс)
    static uint32_t led_last_ms = 0;
    static bool led_state = false;
    if (millis() - led_last_ms >= 500) {
        led_last_ms = millis();
        led_state = !led_state;
        if (led_state) {
            rgbLed.setPixelColor(0, rgbLed.Color(0, 10, 0)); // Тускло-зеленый цвет (R=0, G=10, B=0)
        } else {
            rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 0));  // Выключен
        }
        rgbLed.show();
    }

    webController.handleClient();
    checkTiltSensor();

    if (displayManager.needsRedraw()) { 
        displayManager.drawScreen(weatherModel); 
        displayManager.clearRedraw(); 
    }

    static uint32_t last_update = 0;
    if (millis() - last_update >= weatherModel.update_interval_ms) {
        last_update = millis();
        
        float t = 0, p = 0;
        if (readBMP280(&t, &p)) {
            float sea = weatherModel.calcSeaLevelPressure(p, t, ALTITUDE_M);
            float trend = weatherModel.getTrend(sea);
            
            weatherModel.web_temp = t; 
            weatherModel.web_press = sea;
            weatherModel.web_forecast = weatherModel.getZambrettiForecast(sea, trend);
            weatherModel.pushDataPoint(sea, t);
            
            displayManager.drawScreen(weatherModel);
            
            Serial.printf("P=%.0f | T=%.1f | Trend=%.2f | FC: %s | Rot: %d | Int: %ums\n", 
                          sea, t, trend, weatherModel.web_forecast, 
                          weatherModel.current_rotation, weatherModel.update_interval_ms);
        }
    }
    delay(1);
}