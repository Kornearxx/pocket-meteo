#pragma once

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "Config.h"
#include "WeatherModel.h"

class DisplayManager {
public:
    DisplayManager();
    void init(uint8_t rotation);
    void setRotation(uint8_t rotation);
    void drawScreen(const WeatherModel& model);
    void setNeedsRedraw() { needs_redraw = true; }
    bool needsRedraw() const { return needs_redraw; }
    void clearRedraw() { needs_redraw = false; }
    
private:
    void drawGraphMode(const WeatherModel& model);
    void drawCurrentMode(const WeatherModel& model);
    
    GxEPD2_290_T94 eink_driver;
    GxEPD2_BW<GxEPD2_290_T94, GxEPD2_290_T94::HEIGHT> display;
    bool needs_redraw = false;
    uint32_t cycle_cnt = 0;
};