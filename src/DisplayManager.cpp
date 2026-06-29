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

namespace {

int16_t textWidth(Adafruit_GFX& gfx, const char* text) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return static_cast<int16_t>(w);
}

void drawTextRight(Adafruit_GFX& gfx, int16_t rightX, int16_t baselineY, const char* text) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.getTextBounds(text, 0, baselineY, &x1, &y1, &w, &h);
    gfx.setCursor(rightX - static_cast<int16_t>(w), baselineY);
    gfx.print(text);
}

} // namespace

DisplayManager::DisplayManager()
    : spiDisplay(HSPI), tft(&spiDisplay, TFT_CS, TFT_DC, TFT_RST) {}

DisplayManager::~DisplayManager() {
    delete canvas;
}

void DisplayManager::ensureCanvas() {
    const int w = tft.width();
    const int h = tft.height();
    if (canvas && canvas->width() == w && canvas->height() == h) {
        return;
    }
    delete canvas;
    canvas = new GFXcanvas16(w, h);
}

void DisplayManager::blitCanvas() {
    if (!canvas) {
        return;
    }
    tft.drawRGBBitmap(0, 0, canvas->getBuffer(), canvas->width(), canvas->height());
}

void DisplayManager::init(uint8_t rotation) {
    spiDisplay.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    tft.init(240, 320);
    tft.invertDisplay(true); // IPS-панель ST7789T3
    tft.setRotation(rotation);
    ensureCanvas();
    if (canvas) {
        canvas->fillScreen(COLOR_BG);
        blitCanvas();
    } else {
        tft.fillScreen(COLOR_BG);
    }
}

void DisplayManager::setRotation(uint8_t rotation) {
    tft.setRotation(rotation);
    ensureCanvas();
    if (canvas) {
        canvas->fillScreen(COLOR_BG);
        blitCanvas();
    } else {
        tft.fillScreen(COLOR_BG);
    }
}

void DisplayManager::drawScreen(const WeatherModel& model) {
    ensureCanvas();
    if (!canvas) {
        tft.fillScreen(COLOR_BG);
        if (model.display_mode == 0) {
            drawGraphMode(model, tft);
        } else {
            drawCurrentMode(model, tft);
        }
        return;
    }

    canvas->fillScreen(COLOR_BG);
    if (model.display_mode == 0) {
        drawGraphMode(model, *canvas);
    } else {
        drawCurrentMode(model, *canvas);
    }
    blitCanvas();
}

void DisplayManager::drawGraphMode(const WeatherModel& model, Adafruit_GFX& gfx) {
    gfx.setFont(&FreeSans9pt7b);
    const int ml = 52, mr = 5, mt = 10, mb = 22;
    const int gw = gfx.width() - ml - mr;
    const int gh = gfx.height() - mt - mb;
    const int labelRight = ml - 5;
    const int timeY = mt + gh + 16;
    
    uint32_t offset = model.graph_offset_sec;
    uint32_t window = 600; // Окно 10 минут
    uint32_t now_sec = millis() / 1000;
    int count = model.getHistoryCount();

    // Расчет "окна просмотра" (прыжок на 1 минуту вперед при достижении края)
    uint32_t logical_now = window;
    if (now_sec > window) {
        logical_now = window + ((now_sec - window - 1) / 60 + 1) * 60;
    }
    uint32_t max_offset = logical_now - window;
    if (offset > max_offset) offset = max_offset; // Блокируем скролл в пустоту
    uint32_t view_end_time = logical_now - offset;
    uint32_t view_start_time = view_end_time - window;

    // 1. Поиск мин/макс значений в видимом окне (Pass 1)
    float min_p = 9999.0f, max_p = -9999.0f;
    bool has_data = false;
    for (int i = 0; i < count; i++) {
        const DataPoint& dp = model.getHistory(i);
        if (dp.ts >= view_start_time && dp.ts <= view_end_time) {
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

    // 3. Отрисовка осей, делений и подписей
    gfx.drawLine(ml, mt + gh, ml + gw, mt + gh, COLOR_AXIS);
    gfx.drawLine(ml, mt, ml, mt + gh, COLOR_AXIS);

    auto drawYTick = [&](float p) {
        int y = pToY(p);
        gfx.drawLine(ml - 4, y, ml, y, COLOR_AXIS);
    };
    drawYTick(P_MAX);
    drawYTick(mid_p);
    drawYTick(P_MIN);

    // Подписи давления — фиксированные слоты слева, без наложения при узком диапазоне
    gfx.setTextColor(COLOR_AXIS);
    char pbuf[12];
    snprintf(pbuf, sizeof(pbuf), "%.1f", P_MAX);
    drawTextRight(gfx, labelRight, mt + 12, pbuf);
    snprintf(pbuf, sizeof(pbuf), "%.1f", mid_p);
    drawTextRight(gfx, labelRight, mt + gh / 2 + 6, pbuf);
    snprintf(pbuf, sizeof(pbuf), "%.1f", P_MIN);
    drawTextRight(gfx, labelRight, mt + gh - 2, pbuf);

    // Подписи времени — выравнивание по краям и центру с защитой от пересечения
    char tbuf[10];
    snprintf(tbuf, sizeof(tbuf), "%lum", view_start_time / 60);
    const int wStart = textWidth(gfx, tbuf);
    gfx.setCursor(ml, timeY);
    gfx.print(tbuf);

    snprintf(tbuf, sizeof(tbuf), "%lum", (view_start_time + view_end_time) / 120);
    const int wMid = textWidth(gfx, tbuf);
    const int xMid = ml + gw / 2 - wMid / 2;

    snprintf(tbuf, sizeof(tbuf), "%lum", view_end_time / 60);
    const int wEnd = textWidth(gfx, tbuf);
    const int xEnd = ml + gw - wEnd;

    snprintf(tbuf, sizeof(tbuf), "%lum", (view_start_time + view_end_time) / 120);
    if (xMid >= ml + wStart + 4 && xMid + wMid <= xEnd - 4) {
        gfx.setCursor(xMid, timeY);
        gfx.print(tbuf);
    }

    snprintf(tbuf, sizeof(tbuf), "%lum", view_end_time / 60);
    gfx.setCursor(xEnd, timeY);
    gfx.print(tbuf);

    // 4. Отрисовка точек графика (Pass 2)
    int prev_x = -1, prev_y = -1;
    for (int i = 0; i < count; i++) {
        const DataPoint& dp = model.getHistory(i);
        
        if (dp.ts >= view_start_time && dp.ts <= view_end_time) {
            float x_norm = (float)(dp.ts - view_start_time) / (float)window;
            int x = ml + (int)(x_norm * gw);
            int y = pToY(dp.pressure);
            
            gfx.fillRect(x - 2, y - 2, 4, 4, COLOR_GRAPH);
            if (prev_x != -1) gfx.drawLine(prev_x, prev_y, x, y, COLOR_GRAPH);
            prev_x = x; 
            prev_y = y;
        } else {
            prev_x = -1; // Разрываем линию соединений вне окна
        }
    }
}

void DisplayManager::drawCurrentMode(const WeatherModel& model, Adafruit_GFX& gfx) {
    gfx.setFont(&FreeSansBold12pt7b);
    
    gfx.setTextColor(COLOR_TEMP);
    gfx.setCursor(20, 45);
    // Аккуратно отрисовываем символ градусов (шрифты FreeFonts часто не поддерживают расширенное ASCII)
    gfx.printf("Temp: %.1f", model.web_temp);
    int cx = gfx.getCursorX() + 4;
    int cy = gfx.getCursorY() - 16;
    gfx.drawCircle(cx, cy, 3, COLOR_TEMP);
    gfx.setCursor(cx + 8, gfx.getCursorY());
    gfx.print("C");
    
    gfx.setTextColor(COLOR_PRESS);
    gfx.setCursor(20, 90);
    gfx.printf("Press: %.0f hPa", model.web_press);
    
    gfx.setFont(&FreeSans9pt7b);
    gfx.setTextColor(COLOR_TEXT);
    gfx.setCursor(20, 135);
    gfx.print("FC: ");
    
    gfx.setTextColor(COLOR_GRAPH);
    gfx.println(model.web_forecast);
}