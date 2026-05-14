/**
 * ============================================================================
 *  Pocket Weather Station v5: BMP390 + e-Ink + WiFi AP + Zambretti + TILT
 *  ✅ Плавный Canvas-график с авто-масштабом и областью под кривой
 *  ✅ Toast-уведомления (прогноз, данные, ориентация)
 *  ✅ Веб-ползунок скорости обновления (1-30 сек)
 *  ✅ Неблокирующий цикл для стабильной WiFi-работы
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// --- Пины ---
#define BMP_SDA     8
#define BMP_SCL     9
#define BMP_ADDR    0x77
#define TILT_PIN    5

#define EPD_CS      10
#define EPD_DC      4
#define EPD_RST     3
#define EPD_BUSY    2
#define EPD_SCLK    6
#define EPD_MOSI    7
#define EPD_MISO    -1

// --- Настройки ---
#define ALTITUDE_M          150.0f
#define MAX_BUFFER_SIZE     1200
#define TREND_LOOKBACK_SEC  30
#define TREND_THRESHOLD     0.2f 
#define DEBOUNCE_MS         50

// --- WiFi ---
#define AP_SSID     "Pocket"
const IPAddress AP_IP(192, 168, 4, 1);

// --- Объекты ---
GxEPD2_290_T94 eink_driver(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
GxEPD2_BW<GxEPD2_290_T94, GxEPD2_290_T94::HEIGHT> display(eink_driver);
Adafruit_BMP3XX bmp;
DNSServer dnsServer;
WebServer server(80);

// --- Состояние ---
volatile uint8_t display_mode = 0; // 0: График, 1: Данные
volatile uint32_t update_interval_ms = 3000;
struct DataPoint { float pressure; float temp; uint32_t ts; };
static DataPoint history[MAX_BUFFER_SIZE];
static int buf_head = 0, buf_count = 0;
volatile float web_temp = 0.0, web_press = 0.0;
const char* web_forecast = "Загрузка...";

// Tilt
uint8_t current_rotation = 1;
bool tilt_stable = false, tilt_pending = false, display_needs_redraw = false;
uint32_t tilt_last_ms = 0;

// ============================================================================
// 📊 ДАТЧИК И РАСЧЁТЫ
// ============================================================================
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

float calcSeaLevelPressure(float p_raw, float temp_c, float altitude) {
    float T_k = temp_c + 273.15f;
    return p_raw * powf(1.0f + (0.0065f * altitude) / T_k, 5.255f);
}

void pushDataPoint(float p, float t) {
    uint32_t ts = millis() / 1000;
    history[buf_head] = {p, t, ts};
    buf_head = (buf_head + 1) % MAX_BUFFER_SIZE;
    if (buf_count < MAX_BUFFER_SIZE) buf_count++;
}

float getTrend(float current_p) {
    if (buf_count < 2) return 0.0f;
    uint32_t now_sec = millis() / 1000;
    float oldest_p = 0; bool found = false;
    for (int i = 0; i < buf_count; i++) {
        int idx = (buf_head - buf_count + i + MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE;
        if ((now_sec - history[idx].ts) <= TREND_LOOKBACK_SEC) { oldest_p = history[idx].pressure; found = true; }
    }
    return found ? (current_p - oldest_p) : 0.0f;
}

const char* getZambrettiForecast(float press, float trend) {
    if (press > 1020.0f) {
        if (trend > TREND_THRESHOLD) return "Ясно, улучшение";
        if (trend < -TREND_THRESHOLD) return "Переменная облачность";
        return "Ясно, стабильно";
    } else if (press > 1000.0f) {
        if (trend > TREND_THRESHOLD) return "Облачно, прояснения";
        if (trend < -TREND_THRESHOLD) return "Возможен дождь";
        return "Переменная облачность";
    } else {
        if (trend < -TREND_THRESHOLD * 1.3f) return "Дождь, ухудшение";
        if (trend > TREND_THRESHOLD * 1.3f) return "Облачно, улучшение";
        return "Пасмурно";
    }
}

// ============================================================================
// 🌐 ВЕБ-ИНТЕРФЕЙС И API
// ============================================================================
const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pocket Weather</title>
<style>
  :root { --bg: #f4f4f9; --card: #fff; --text: #333; --accent: #3498db; --warn: #e67e22; }
  body{font-family:system-ui,-apple-system,sans-serif;background:var(--bg);margin:0;padding:15px;text-align:center;color:var(--text)}
  .wrap{max-width:360px;margin:0 auto}
  h1{margin:0 0 10px;font-size:20px}
  .card{background:var(--card);border-radius:12px;padding:15px;margin:10px 0;box-shadow:0 3px 6px rgba(0,0,0,.1)}
  .row{display:flex;justify-content:space-around;align-items:center}
  .val{font-size:28px;font-weight:bold}
  .unit{font-size:14px;color:#7f8c8d}
  .fc{font-size:16px;color:var(--warn);font-weight:600}
  .status-bar{font-size:12px;color:#95a5a6;display:flex;justify-content:space-between;margin:8px 0}
  .ctrl{margin:10px 0}
  input[type=range]{width:100%}
  .btn{display:inline-block;padding:10px 20px;border:none;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;color:#fff;margin:5px}
  .btn-log{background:#3498db} .btn-mode{background:#2ecc71}
  canvas{background:#fff;width:100%;height:200px;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,.1);margin:10px 0}
  #toast-container{position:fixed;top:20px;left:50%;transform:translateX(-50%);z-index:999;display:flex;flex-direction:column;gap:8px}
  .toast{padding:10px 15px;border-radius:8px;color:#fff;font-size:13px;opacity:0;transform:translateY(-10px);transition:all 0.3s;box-shadow:0 3px 6px rgba(0,0,0,.2)}
  .toast.show{opacity:1;transform:translateY(0)}
  .toast.info{background:#3498db} .toast.warn{background:#e67e22} .toast.ok{background:#27ae60}
</style>
</head>
<body>
<div class="wrap">
<h1>Pocket Weather</h1>
<div class="status-bar">
  <span id="orient">📐 Ориентация: --</span>
  <span id="interval-disp">⏱ 3 сек</span>
</div>

<div class="card">
  <div class="row">
    <div><div style="font-size:12px;color:#95a5a6">Температура</div><div class="val"><span id="t">--</span><span class="unit">°C</span></div></div>
    <div><div style="font-size:12px;color:#95a5a6">Давление</div><div class="val"><span id="p">--</span><span class="unit">гПа</span></div></div>
  </div>
  <div class="fc" id="fc" style="margin-top:8px">Прогноз: --</div>
</div>

<canvas id="chart" width="320" height="200"></canvas>

<div class="ctrl">
  <label>Скорость обновления: <input type="range" id="int-slider" min="1" max="30" value="3" step="1"></label>
</div>

<div>
  <button class="btn btn-mode" onclick="toggleMode()">🔄 Режим e-Ink</button>
  <button class="btn btn-log" onclick="downloadLog()">📥 Лог CSV</button>
</div>
</div>

<div id="toast-container"></div>

<script>
const ctx=document.getElementById('chart').getContext('2d');
let prevFC="", prevRot="";
let lastData=[];

function toast(msg, type='info'){
  const c=document.getElementById('toast-container');
  const t=document.createElement('div');
  t.className='toast '+type; t.textContent=msg;
  c.appendChild(t); setTimeout(()=>t.classList.add('show'),50);
  setTimeout(()=>{t.classList.remove('show');setTimeout(()=>t.remove(),300)},3000);
}

function drawChart(data){
  if(!data.length)return;
  const W=ctx.canvas.width, H=ctx.canvas.height;
  const pad={l:40,r:10,t:10,b:20}, gw=W-pad.l-pad.r, gh=H-pad.t-pad.b;
  let vals=data.map(d=>d.p);
  let minP=Math.min(...vals), maxP=Math.max(...vals);
  let range=maxP-minP||2; minP-=range*0.15; maxP+=range*0.15; range=maxP-minP;

  ctx.clearRect(0,0,W,H);
  ctx.strokeStyle='#eee'; ctx.lineWidth=1;
  ctx.beginPath(); ctx.moveTo(pad.l,pad.t); ctx.lineTo(pad.l,pad.t+gh); ctx.lineTo(pad.l+gw,pad.t+gh); ctx.stroke();

  ctx.fillStyle='#888'; ctx.font='10px sans-serif'; ctx.textAlign='right';
  for(let i=0;i<=4;i++){let p=minP+range*(i/4), y=pad.t+gh-(gh*(i/4)); ctx.fillText(p.toFixed(0),pad.l-4,y+3);}

  let points=data.map((d,i)=>({x: pad.l+(i/(data.length-1||1))*gw, y: pad.t+gh-((d.p-minP)/range)*gh}));
  
  // Область под графиком
  ctx.fillStyle='rgba(52,152,219,0.1)'; ctx.beginPath();
  ctx.moveTo(points[0].x, pad.t+gh);
  points.forEach(p=>ctx.lineTo(p.x,p.y));
  ctx.lineTo(points[points.length-1].x, pad.t+gh); ctx.closePath(); ctx.fill();

  // Плавная линия (квадратичные кривые)
  ctx.strokeStyle='#2c3e50'; ctx.lineWidth=2; ctx.lineCap='round'; ctx.lineJoin='round'; ctx.beginPath();
  ctx.moveTo(points[0].x, points[0].y);
  for(let i=0;i<points.length-1;i++){
    let xc=(points[i].x+points[i+1].x)/2, yc=(points[i].y+points[i+1].y)/2;
    ctx.quadraticCurveTo(points[i].x, points[i].y, xc, yc);
  }
  let last=points[points.length-1];
  ctx.quadraticCurveTo(last.x, last.y, last.x, last.y); ctx.stroke();

  // Точки замеров
  ctx.fillStyle='#fff'; ctx.strokeStyle='#2c3e50'; ctx.lineWidth=1;
  points.forEach(p=>{ctx.beginPath();ctx.arc(p.x,p.y,3,0,Math.PI*2);ctx.fill();ctx.stroke();});

  ctx.fillStyle='#888'; ctx.textAlign='center';
  ctx.fillText('0',pad.l,pad.t+gh+14); ctx.fillText('30м',pad.l+gw*0.25,pad.t+gh+14); ctx.fillText('1ч',pad.l+gw,pad.t+gh+14);
}

function update(){
  fetch('/api/data').then(r=>r.json()).then(d=>{
    document.getElementById('t').textContent=d.temp.toFixed(1);
    document.getElementById('p').textContent=d.press.toFixed(0);
    let fcText = d.forecast;
    document.getElementById('fc').textContent="Прогноз: "+fcText;
    document.getElementById('interval-disp').textContent="⏱ "+(d.interval/1000)+" сек";
    document.getElementById('int-slider').value=d.interval/1000;

    let rotText = d.rot==1 ? "📐 Нормально" : "📐 Перевёрнуто";
    document.getElementById('orient').textContent=rotText;

    if(fcText!==prevFC) { toast("🌤 "+fcText, fcText.includes("Дождь")||fcText.includes("улучшение")?'warn':'info'); prevFC=fcText; }
    if(d.rot!==prevRot) { toast(rotText, 'ok'); prevRot=d.rot; }

    lastData=d.hist; drawChart(lastData);
  });
}

function toggleMode(){fetch('/toggle').then(r=>r.json()).then(d=>update());}
function downloadLog(){window.location.href='/log';}

document.getElementById('int-slider').addEventListener('input', e=>{
  let s=parseInt(e.target.value);
  fetch('/api/interval',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ms:s*1000})}).then(()=>{});
});

setInterval(update,2500); update();
</script>
</body>
</html>
)rawliteral";

void setupWebServer() {
    WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    dnsServer.start(53, "*", AP_IP);

    server.on("/", []() { server.send(200, "text/html", HTML_PAGE); });

    server.on("/api/data", []() {
        String forecastStr = String((const char*)web_forecast);
        String json = "{\"temp\":" + String(web_temp, 1) + ",\"press\":" + String(web_press, 0) + 
                      ",\"forecast\":\"" + forecastStr + "\",\"mode\":" + String(display_mode) + 
                      ",\"interval\":" + String(update_interval_ms) + ",\"rot\":" + String(current_rotation) + ",\"hist\":[";
        uint32_t now_sec = millis() / 1000; bool first = true;
        for (int i = 0; i < buf_count; i++) {
            int idx = (buf_head - buf_count + i + MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE;
            uint32_t age_sec = now_sec - history[idx].ts;
            if (age_sec > 3600) continue;
            if (!first) json += ","; first = false;
            json += "{\"t\":" + String(age_sec) + ",\"p\":" + String(history[idx].pressure, 2) + "}";
        }
        json += "]}"; server.send(200, "application/json", json);
    });

    server.on("/toggle", []() { display_mode = (display_mode == 0) ? 1 : 0; server.send(200, "application/json", "{\"mode\":" + String(display_mode) + "}"); });

    server.on("/api/interval", HTTP_POST, []() {
        if(server.hasArg("plain")) {
            String body = server.arg("plain");
            int p = body.indexOf("\"ms\":");
            if(p>=0) {
                uint32_t ms = body.substring(p+4, body.indexOf(",", p)).toInt();
                if(ms>=1000 && ms<=60000) update_interval_ms = ms;
            }
        }
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/log", []() {
        String csv = "time_sec,temperature_c,pressure_hpa\r\n";
        for (int i = 0; i < buf_count; i++) {
            int idx = (buf_head - buf_count + i + MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE;
            csv += String(history[idx].ts) + "," + String(history[idx].temp, 2) + "," + String(history[idx].pressure, 2) + "\r\n";
        }
        server.sendHeader("Content-Disposition", "attachment; filename=pocket_log.csv");
        server.send(200, "text/csv", csv);
    });

    server.onNotFound([]() { server.sendHeader("Location", "http://192.168.4.1", true); server.send(302, "text/plain", ""); });
    server.begin();
}

// ============================================================================
// 📈 ОТРИСОВКА E-INK
// ============================================================================
void drawGraphMode() {
    static uint32_t cycle_cnt = 0;
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
        auto pToY = [&](float p) -> int { return mt + gh - (int)(constrain((p - P_MIN) / P_RANGE, 0.0f, 1.0f) * gh); };

        display.drawLine(ml, mt + gh, ml + gw, mt + gh, GxEPD_BLACK);
        display.drawLine(ml, mt, ml, mt + gh, GxEPD_BLACK);
        for (float p = P_MIN; p <= P_MAX; p += 10) { display.setCursor(2, pToY(p) + 4); display.printf("%.0f", p); }
        display.setCursor(ml - 5, mt + gh + 15); display.print("0");
        display.setCursor(ml + gw / 2 - 8, mt + gh + 15); display.print("30м");
        display.setCursor(ml + gw - 15, mt + gh + 15); display.print("1ч");

        uint32_t now_sec = millis() / 1000;
        int prev_x = -1, prev_y = -1;
        for (int i = 0; i < buf_count; i++) {
            int idx = (buf_head - buf_count + i + MAX_BUFFER_SIZE) % MAX_BUFFER_SIZE;
            uint32_t age = now_sec - history[idx].ts;
            if (age > 3600) continue;
            float x_norm = (float)age / 3600.0f;
            int x = ml + (int)(x_norm * gw);
            int y = pToY(history[idx].pressure);
            display.drawRect(x - 2, y - 2, 4, 4, GxEPD_BLACK);
            if (prev_x != -1) display.drawLine(prev_x, prev_y, x, y, GxEPD_BLACK);
            prev_x = x; prev_y = y;
        }
    } while (display.nextPage());
}

void drawCurrentMode() {
    static uint32_t cycle_cnt = 0;
    bool full_refresh = (++cycle_cnt % 10 == 0);
    if (full_refresh) display.setFullWindow();
    else display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(20, 35);
        display.printf("Temp: %.1f°C", web_temp);
        display.setCursor(20, 75);
        display.printf("Press: %.0f hPa", web_press);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(20, 110);
        display.print("Прогноз: ");
        display.println((const char*)web_forecast);
    } while (display.nextPage());
}

void drawScreen() {
    if (display_mode == 0) drawGraphMode();
    else drawCurrentMode();
}

// ============================================================================
// 🔘 ОБРАБОТКА НАКЛОНА (SW-520D)
// ============================================================================
void checkTiltSensor() {
    bool raw = (digitalRead(TILT_PIN) == LOW);
    if (raw != tilt_pending) { tilt_pending = raw; tilt_last_ms = millis(); }
    if (millis() - tilt_last_ms >= DEBOUNCE_MS && raw != tilt_stable) {
        tilt_stable = raw;
        uint8_t new_rot = tilt_stable ? 3 : 1;
        if (new_rot != current_rotation) {
            current_rotation = new_rot;
            display.setRotation(current_rotation);
            display.setFullWindow();
            display.firstPage(); do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
            display_needs_redraw = true;
        }
    }
}

// ============================================================================
// 🔄 ARDUINO ENTRY POINTS
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== BOOT: Pocket Station v5 ===");
    
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
    } else Serial.println("FAIL");
    
    Serial.print("[EPD] ");
    display.init(115200);
    display.setRotation(current_rotation);
    Serial.println("OK");

    display.setFullWindow();
    display.firstPage(); do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());

    setupWebServer();
    Serial.println("[READY]");
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    checkTiltSensor();

    static uint32_t last_update = 0;
    if (millis() - last_update >= update_interval_ms) {
        last_update = millis();
        if (display_needs_redraw) { drawScreen(); display_needs_redraw = false; }
        
        float t = 0, p = 0;
        if (readBMP390(&t, &p)) {
            float sea = calcSeaLevelPressure(p, t, ALTITUDE_M);
            float trend = getTrend(sea);
            web_temp = t; web_press = sea;
            web_forecast = getZambrettiForecast(sea, trend);
            pushDataPoint(sea, t);
            drawScreen(); // Обновляем экран с новыми данными
            Serial.printf("P=%.0f | T=%.1f | Trend=%.2f | FC: %s | Rot: %d | Int: %dms\n", sea, t, trend, web_forecast, current_rotation, update_interval_ms);
        }
    }
    // Небольшая задержка для разгрузки CPU, но не блокирует WiFi
    delay(1);
}