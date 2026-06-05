/**
 * ============================================================================
 *  SAFE COMPASS DISPLAY: e-Ink Always Draws First + I2C Timeout Protection
 *  ✅ Пины: SDA=5, SCL=20 (по запросу)
 *  ✅ Экран НИКОГДА не будет белым: отрисовка происходит ДО I2C
 *  ✅ Защита от зависания шины: таймауты, проверка статусов, yield()
 *  ✅ getCardinal() объявлена ДО drawStatus() (C++ strict mode)
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <math.h>
#include <esp_task_wdt.h>

// --- Пины компаса ---
#define COMP_SDA    5
#define COMP_SCL    20  // ✅ Заменено на 20 по запросу
#define COMP_ADDR   0x1E // 0x0D для QMC5883L

// --- Пины e-Ink ---
#define EPD_CS      10
#define EPD_DC      4
#define EPD_RST     3
#define EPD_BUSY    2
#define EPD_SCLK    6
#define EPD_MOSI    7

// --- Объекты ---
TwoWire compWire(1);
GxEPD2_290_T94 epd_driver(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
GxEPD2_BW<GxEPD2_290_T94, GxEPD2_290_T94::HEIGHT> display(epd_driver);

// --- Состояние ---
float heading = 0.0;
bool compassOk = false;
String statusMsg = "Checking...";

// ============================================================================
// 🧭 Вспомогательные функции (объявлены ДО использования)
// ============================================================================
const char* getCardinal(float h) {
    const char* dirs[] = {"С", "СВ", "В", "ЮВ", "Ю", "ЮЗ", "З", "СЗ"};
    return dirs[(int)((h + 22.5f) / 45.0f) % 8];
}

// ============================================================================
// 🖥️ Гарантированная отрисовка (вызывается первой!)
// ============================================================================
void drawStatus(const String& msg) {
    esp_task_wdt_reset();
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setTextWrap(false);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(30, 60);
        display.print(msg);
        
        if (compassOk) {
            display.setCursor(80, 90);
            display.printf("%.0f° %s", heading, getCardinal(heading));
        }
    } while (display.nextPage());
    
    delay(300); // Критично: даём контроллеру e-Ink завершить цикл
    esp_task_wdt_reset();
}

// ============================================================================
// 🧭 Безопасная проверка I2C (не блокирует)
// ============================================================================
bool safeInitCompass() {
    pinMode(COMP_SDA, INPUT_PULLUP);
    pinMode(COMP_SCL, INPUT_PULLUP);
    yield();
    
    compWire.begin(COMP_SDA, COMP_SCL, 100000); // 100 кГц (стабильнее для диагностики)
    compWire.setTimeout(50);
    
    // Проверяем, отвечает ли устройство по адресу
    compWire.beginTransmission(COMP_ADDR);
    if (compWire.endTransmission(true) != 0) return false;
    
    // Инициализация в Continuous Mode
    compWire.beginTransmission(COMP_ADDR); compWire.write(0x00); compWire.write(0x70); 
    if (compWire.endTransmission(true) != 0) return false;
    yield(); delay(20);
    
    compWire.beginTransmission(COMP_ADDR); compWire.write(0x01); compWire.write(0xA0); 
    if (compWire.endTransmission(true) != 0) return false;
    yield(); delay(20);
    
    compWire.beginTransmission(COMP_ADDR); compWire.write(0x02); compWire.write(0x00); 
    return compWire.endTransmission(true) == 0;
}

bool readCompassSafe(float* h) {
    compWire.beginTransmission(COMP_ADDR);
    compWire.write(0x03);
    if (compWire.endTransmission(false) != 0) return false;
    if (compWire.requestFrom(COMP_ADDR, 6) != 6) return false;
    
    int16_t x = (int16_t)(compWire.read() << 8 | compWire.read());
    compWire.read(); compWire.read();
    int16_t y = (int16_t)(compWire.read() << 8 | compWire.read());
    
    if (x == -4096 || y == -4096) return false;
    *h = atan2((float)y, (float)x) * 180.0 / PI;
    if (*h < 0) *h += 360.0;
    return true;
}

// ============================================================================
// 🔄 ENTRY POINTS
// ============================================================================
void setup() {
    SPI.begin(EPD_SCLK, -1, EPD_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(1);
    display.setFullWindow();
    display.setTextColor(GxEPD_BLACK);

    // 1. СРАЗУ рисуем, чтобы экран не был белым
    drawStatus("EPD OK. Init I2C...");
    delay(1500);
    
    // 2. Безопасная проверка компаса
    if (safeInitCompass()) {
        float test_h;
        if (readCompassSafe(&test_h)) {
            heading = test_h;
            compassOk = true;
            statusMsg = "COMPASS OK";
        } else {
            statusMsg = "I2C NO DATA";
        }
    } else {
        statusMsg = "I2C FAIL";
    }
    
    drawStatus(statusMsg);
    delay(1500);
    
    // 3. Если пин 20 всё же блокирует (редко), выводим подсказку
    if (!compassOk && statusMsg == "I2C FAIL") {
        drawStatus("GPIO20 BLOCKED!\nTry SCL = 18");
        delay(5000);
    }
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last >= 2000) { // e-Ink физически не может чаще ~1.5с
        last = millis();
        if (compassOk) readCompassSafe(&heading);
        drawStatus(compassOk ? "RUNNING" : "NO SIGNAL");
    }
    delay(10);
}