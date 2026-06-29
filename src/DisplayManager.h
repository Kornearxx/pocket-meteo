#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
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

    SPIClass spiDisplay;
    Adafruit_ST7789 tft;
    bool needs_redraw = false;
};
