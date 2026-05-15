#include "WeatherModel.h"

void WeatherModel::pushDataPoint(float p, float t) {
    uint32_t ts = millis() / 1000;
    history[buf_head] = {p, t, ts};
    buf_head = (buf_head + 1) % MAX_BUFFER_SIZE;
    if (buf_count < MAX_BUFFER_SIZE) buf_count++;
}

float WeatherModel::getTrend(float current_p) const {
    if (buf_count < 2) return 0.0f;
    uint32_t now_sec = millis() / 1000;
    float oldest_p = 0; 
    bool found = false;
    for (int i = 0; i < buf_count; i++) {
        int idx = (buf_head - buf_count + i + MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE;
        if ((now_sec - history[idx].ts) <= TREND_LOOKBACK_SEC) { 
            oldest_p = history[idx].pressure; 
            found = true; 
        }
    }
    return found ? (current_p - oldest_p) : 0.0f;
}

const char* WeatherModel::getZambrettiForecast(float press, float trend) const {
    if (press > 1020.0f) {
        if (trend > TREND_THRESHOLD) return "Clear, improving";
        if (trend < -TREND_THRESHOLD) return "Partly cloudy";
        return "Clear, stable";
    } else if (press > 1000.0f) {
        if (trend > TREND_THRESHOLD) return "Cloudy, clearing";
        if (trend < -TREND_THRESHOLD) return "Possible rain";
        return "Partly cloudy";
    } else {
        if (trend < -TREND_THRESHOLD * 1.3f) return "Rain, worsening";
        if (trend > TREND_THRESHOLD * 1.3f) return "Cloudy, improving";
        return "Overcast";
    }
}

float WeatherModel::calcSeaLevelPressure(float p_raw, float temp_c, float altitude) const {
    float T_k = temp_c + 273.15f;
    return p_raw * powf(1.0f + (0.0065f * altitude) / T_k, 5.255f);
}

const DataPoint& WeatherModel::getHistory(int index) const {
    int idx = (buf_head - buf_count + index + MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE;
    return history[idx];
}