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
    const int ml = 45, mr = 5, mt = 10, mb = 20; // Увеличили ml для вмещения десятых долей
    const int gw = tft.width() - ml - mr;
    const int gh = tft.height() - mt - mb;
    
    uint32_t offset = model.graph_offset_sec;
    uint32_t window = 600; // Окно 10 минут
    uint32_t now_sec = millis() / 1000;
    int count = model.getHistoryCount();

    // 1. Поиск мин/макс значений в видимом окне (Pass 1)
    float min_p = 9999.0f, max_p = -9999.0f;
    bool has_data = false;
    for (int i = 0; i < count; i++) {
        const DataPoint& dp = model.getHistory(i);
        if (now_sec < dp.ts) continue;
        uint32_t age = now_sec - dp.ts;
        if (age >= offset && age <= offset + window) {
            if (dp.pressure < min_p) min_p = dp.pressure;
            if (dp.pressure > max_p) max_p = dp.pressure;
            has_data = true;
        }
    }
    if (!has_data) {
        min_p = model.web_press > 0 ? model.web_press : 1010.0f;
        max_p = min_p;
    }

    // 2. Расчет динамического масштаба "в половину экрана"
    float data_range = max_p - min_p;
    if (data_range < 1.0f) data_range = 1.0f; // Защита от слишком сильного зума
    
    float mid_p = (max_p + min_p) / 2.0f;
    float P_RANGE = data_range * 2.0f; // Данные займут ровно половину оси
    float P_MIN = mid_p - P_RANGE / 2.0f;
    float P_MAX = mid_p + P_RANGE / 2.0f;

    auto pToY = [&](float p) -> int { 
        return mt + gh - (int)(constrain((p - P_MIN) / P_RANGE, 0.0f, 1.0f) * gh); 
    };

    // 3. Отрисовка осей и подписей
    tft.drawLine(ml, mt + gh, ml + gw, mt + gh, COLOR_AXIS);
    tft.drawLine(ml, mt, ml, mt + gh, COLOR_AXIS);
    
    tft.setTextColor(COLOR_AXIS);
    tft.setCursor(0, pToY(P_MAX) + 12); tft.printf("%.1f", P_MAX);
    tft.setCursor(0, pToY(mid_p) + 5);  tft.printf("%.1f", mid_p);
    tft.setCursor(0, pToY(P_MIN) - 4);  tft.printf("%.1f", P_MIN);

    tft.setCursor(ml - 10, mt + gh + 15); tft.printf("%dm", offset / 60);
    tft.setCursor(ml + gw / 2 - 12, mt + gh + 15); tft.printf("%dm", (offset + window / 2) / 60);
    tft.setCursor(ml + gw - 20, mt + gh + 15); tft.printf("%dm", (offset + window) / 60);

    // 4. Отрисовка точек графика (Pass 2)
    int prev_x = -1, prev_y = -1;
    for (int i = 0; i < count; i++) {
        const DataPoint& dp = model.getHistory(i);
        if (now_sec < dp.ts) continue;
        uint32_t age = now_sec - dp.ts;
        
        if (age >= offset && age <= offset + window) {
            float x_norm = (float)(age - offset) / (float)window;
            int x = ml + (int)(x_norm * gw);
            int y = pToY(dp.pressure);
            
            tft.fillRect(x - 2, y - 2, 4, 4, COLOR_GRAPH);
            if (prev_x != -1) tft.drawLine(prev_x, prev_y, x, y, COLOR_GRAPH);
            prev_x = x; 
            prev_y = y;
        } else {
            prev_x = -1; // Разрываем линию соединений вне окна
        }
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