#include "DisplayManager.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// Профессиональные настройки цветов для LCD
#define COLOR_BG      ST77XX_BLACK
#define COLOR_AXIS    0x7BEF // Светло-серый цвет для осей
#define COLOR_TEXT    ST77XX_WHITE
#define COLOR_GRAPH   ST77XX_CYAN
#define COLOR_TEMP    ST77XX_ORANGE
#define COLOR_PRESS   ST77XX_GREEN

DisplayManager::DisplayManager()
    : tft(&SPI, TFT_CS, TFT_DC, TFT_RST) {}

void DisplayManager::init(uint8_t rotation) {
    // Инициализация ST7789 (библиотека сама учитывает offset для матрицы 172x320)
    tft.init(172, 320);
    tft.setRotation(rotation);
    tft.fillScreen(COLOR_BG);
}

void DisplayManager::setRotation(uint8_t rotation) {
    tft.setRotation(rotation);
    tft.fillScreen(COLOR_BG);
}

void DisplayManager::drawScreen(const WeatherModel& model) {
    // На TFT мы можем просто заливать экран без циклов обновления страниц EPD
    tft.fillScreen(COLOR_BG);

    if (model.display_mode == 0) {
        drawGraphMode(model);
    } else {
        drawCurrentMode(model);
    }
}

void DisplayManager::drawGraphMode(const WeatherModel& model) {
    tft.setFont(&FreeSans9pt7b);
    const int ml = 40, mr = 5, mt = 10, mb = 20;
    const int gw = tft.width() - ml - mr;
    const int gh = tft.height() - mt - mb;
    const float P_MIN = 990.0f, P_MAX = 1030.0f, P_RANGE = P_MAX - P_MIN;
    
    auto pToY = [&](float p) -> int { 
        return mt + gh - (int)(constrain((p - P_MIN) / P_RANGE, 0.0f, 1.0f) * gh); 
    };

    tft.drawLine(ml, mt + gh, ml + gw, mt + gh, COLOR_AXIS);
    tft.drawLine(ml, mt, ml, mt + gh, COLOR_AXIS);
    
    tft.setTextColor(COLOR_AXIS);
    for (float p = P_MIN; p <= P_MAX; p += 10) { 
        tft.setCursor(2, pToY(p) + 4); 
        tft.printf("%.0f", p); 
    }
    tft.setCursor(ml - 5, mt + gh + 15); tft.print("0");
    tft.setCursor(ml + gw / 2 - 8, mt + gh + 15); tft.print("30m");
    tft.setCursor(ml + gw - 15, mt + gh + 15); tft.print("1h");

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
        
        tft.fillRect(x - 2, y - 2, 4, 4, COLOR_GRAPH);
        if (prev_x != -1) tft.drawLine(prev_x, prev_y, x, y, COLOR_GRAPH);
        prev_x = x; prev_y = y;
    }
}

void DisplayManager::drawCurrentMode(const WeatherModel& model) {
    tft.setFont(&FreeSansBold12pt7b);
    
    tft.setTextColor(COLOR_TEMP);
    tft.setCursor(20, 45);
    // Аккуратно отрисовываем символ градусов (шрифты FreeFonts часто не поддерживают расширенное ASCII)
    tft.printf("Temp: %.1f", model.web_temp);
    int cx = tft.getCursorX() + 4;
    int cy = tft.getCursorY() - 16;
    tft.drawCircle(cx, cy, 3, COLOR_TEMP);
    tft.setCursor(cx + 8, tft.getCursorY());
    tft.print("C");
    
    tft.setTextColor(COLOR_PRESS);
    tft.setCursor(20, 90);
    tft.printf("Press: %.0f hPa", model.web_press);
    
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(20, 135);
    tft.print("FC: ");
    
    tft.setTextColor(COLOR_GRAPH);
    tft.println(model.web_forecast);
}