/**
 * ============================================================================
 *  Pocket Weather Station v5 - Modular Architecture
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>

#include "Config.h"
#include "WeatherModel.h"
#include "DisplayManager.h"
#include "WebController.h"

WeatherModel weatherModel;
DisplayManager displayManager;
WebController webController;
Adafruit_BMP3XX bmp;

// --- Tilt ---
bool tilt_stable = false;
bool tilt_pending = false;
uint32_t tilt_last_ms = 0;

bool readBMP390(float* out_temp, float* out_press_hpa) {
    float t_sum = 0, p_sum = 0;
    for (uint8_t i = 0; i < 3; i++) {
        if (!bmp.performReading()) return false;
        t_sum += bmp.temperature;
        p_sum += bmp.pressure / 100.0f;
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
    Serial.println("\n=== BOOT: Pocket Station v5 (Modular) ===");
    
    pinMode(TILT_PIN, INPUT_PULLUP);
    Wire.begin(BMP_SDA, BMP_SCL);
    SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI, EPD_CS);
    
    Serial.print("[BMP] ");
    if (bmp.begin_I2C(BMP_ADDR, &Wire)) {
        bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
        bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
        bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
        bmp.setOutputDataRate(BMP3_ODR_50_HZ);
        Serial.println("OK");
    } else {
        Serial.println("FAIL");
    }
    
    Serial.print("[EPD] ");
    displayManager.init(weatherModel.current_rotation);
    Serial.println("OK");

    webController.begin(&weatherModel);
    Serial.println("[READY]");
}

void loop() {
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
        if (readBMP390(&t, &p)) {
            float sea = weatherModel.calcSeaLevelPressure(p, t, ALTITUDE_M);
            float trend = weatherModel.getTrend(sea);
            
            weatherModel.web_temp = t; 
            weatherModel.web_press = sea;
            weatherModel.web_forecast = weatherModel.getZambrettiForecast(sea, trend);
            weatherModel.pushDataPoint(sea, t);
            
            displayManager.drawScreen(weatherModel);
            
            Serial.printf("P=%.0f | T=%.1f | Trend=%.2f | FC: %s | Rot: %d | Int: %dms\n", 
                          sea, t, trend, weatherModel.web_forecast, 
                          weatherModel.current_rotation, weatherModel.update_interval_ms);
        }
    }
    delay(1);
}