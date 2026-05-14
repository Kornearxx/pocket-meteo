#include "DisplayManager.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

DisplayManager::DisplayManager() 
    : eink_driver(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY),
      display(eink_driver) {}

void DisplayManager::init(uint8_t rotation) {
    display.init(115200);
    display.setRotation(rotation);
    display.setFullWindow();
    display.firstPage(); 
    do { 
        display.fillScreen(GxEPD_WHITE); 
    } while (display.nextPage());
}

void DisplayManager::setRotation(uint8_t rotation) {
    display.setRotation(rotation);
    display.setFullWindow();
    display.firstPage(); 
    do { 
        display.fillScreen(GxEPD_WHITE); 
    } while (display.nextPage());
}

void DisplayManager::drawScreen(const WeatherModel& model) {
    if (model.display_mode == 0) {
        drawGraphMode(model);
    } else {
        drawCurrentMode(model);
    }
}

void DisplayManager::drawGraphMode(const WeatherModel& model) {
    bool full_refresh = (++cycle_cnt % 20 == 0);
    if (full_refresh) display.setFullWindow();
    else display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeSans9pt7b);
        const int ml = 40, mr = 5, mt = 10, mb = 20;
        const int gw = display.width() - ml - mr;
        const int gh = display.height() - mt - mb;
        const float P_MIN = 990.0f, P_MAX = 1030.0f, P_RANGE = P_MAX - P_MIN;
        
        auto pToY = [&](float p) -> int { 
            return mt + gh - (int)(constrain((p - P_MIN) / P_RANGE, 0.0f, 1.0f) * gh); 
        };

        display.drawLine(ml, mt + gh, ml + gw, mt + gh, GxEPD_BLACK);
        display.drawLine(ml, mt, ml, mt + gh, GxEPD_BLACK);
        
        for (float p = P_MIN; p <= P_MAX; p += 10) { 
            display.setCursor(2, pToY(p) + 4); 
            display.printf("%.0f", p); 
        }
        display.setCursor(ml - 5, mt + gh + 15); display.print("0");
        display.setCursor(ml + gw / 2 - 8, mt + gh + 15); display.print("30м");
        display.setCursor(ml + gw - 15, mt + gh + 15); display.print("1ч");

        uint32_t now_sec = millis() / 1000;
        int prev_x = -1, prev_y = -1;
        int count = model.getHistoryCount();
        
        for (int i = 0; i < count; i++) {
            const DataPoint& dp = model.getHistory(i);
            uint32_t age = now_sec - dp.ts;
            if (age > 3600) continue;
            
            float x_norm = (float)age / 3600.0f;
            int x = ml + (int)(x_norm * gw);
            int y = pToY(dp.pressure);
            
            display.drawRect(x - 2, y - 2, 4, 4, GxEPD_BLACK);
            if (prev_x != -1) display.drawLine(prev_x, prev_y, x, y, GxEPD_BLACK);
            prev_x = x; prev_y = y;
        }
    } while (display.nextPage());
}

void DisplayManager::drawCurrentMode(const WeatherModel& model) {
    bool full_refresh = (++cycle_cnt % 10 == 0);
    if (full_refresh) display.setFullWindow();
    else display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.printf("Temp: %.1f°C", model.web_temp);
        display.setCursor(20, 75);
        display.printf("Press: %.0f hPa", model.web_press);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, 110);
        display.print("Прогноз: ");
        display.println(model.web_forecast);
    } while (display.nextPage());
}