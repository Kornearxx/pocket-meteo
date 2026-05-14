#pragma once

#include <Arduino.h>
#include "Config.h"

struct DataPoint { 
    float pressure; 
    float temp; 
    uint32_t ts; 
};

class WeatherModel {
public:
    WeatherModel() = default;

    void pushDataPoint(float p, float t);
    float getTrend(float current_p) const;
    const char* getZambrettiForecast(float press, float trend) const;
    float calcSeaLevelPressure(float p_raw, float temp_c, float altitude) const;

    const DataPoint& getHistory(int index) const;
    int getHistoryCount() const { return buf_count; }
    int getHistoryHead() const { return buf_head; }

    // Общее состояние
    float web_temp = 0.0f;
    float web_press = 0.0f;
    const char* web_forecast = "Загрузка...";
    uint32_t update_interval_ms = 3000;
    uint8_t display_mode = 0; // 0: График, 1: Данные
    uint8_t current_rotation = 1;

private:
    DataPoint history[MAX_BUFFER_SIZE];
    int buf_head = 0;
    int buf_count = 0;
};