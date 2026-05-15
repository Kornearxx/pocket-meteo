#include "WebController.h"

extern const char* HTML_PAGE; 

WebController::WebController() : server(80), AP_IP(192, 168, 4, 1) {}

void WebController::begin(WeatherModel* model) {
    weatherModel = model;

    WiFi.mode(WIFI_OFF); // Надежно выключаем Wi-Fi перед перенастройкой (вместо disconnect)
    delay(100);
    WiFi.mode(WIFI_AP); 
    WiFi.softAP(AP_SSID); // В ESP32 интерфейс AP запускается ДО конфигурации IP
    delay(100);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0)); // Теперь применяем IP
    dnsServer.start(53, "*", AP_IP);

    setupRoutes();
    server.begin();
}

void WebController::handleClient() {
    dnsServer.processNextRequest();
    server.handleClient();
}

void WebController::setupRoutes() {
    server.on("/", [this]() { 
        server.send(200, "text/html", HTML_PAGE); 
    });

    server.on("/api/data", [this]() {
        String forecastStr = String(weatherModel->web_forecast);
        String json = "{\"temp\":" + String(weatherModel->web_temp, 1) + 
                      ",\"press\":" + String(weatherModel->web_press, 0) + 
                      ",\"forecast\":\"" + forecastStr + 
                      "\",\"mode\":" + String(weatherModel->display_mode) + 
                      ",\"interval\":" + String(weatherModel->update_interval_ms) + 
                      ",\"rot\":" + String(weatherModel->current_rotation) + 
                      ",\"hist\":[";
        
        uint32_t now_sec = millis() / 1000; 
        bool first = true;
        int count = weatherModel->getHistoryCount();
        
        for (int i = 0; i < count; i++) {
            const DataPoint& dp = weatherModel->getHistory(i);
            uint32_t age_sec = now_sec - dp.ts;
            if (age_sec > 3600) continue;
            if (!first) json += ","; 
            first = false;
            json += "{\"t\":" + String(age_sec) + ",\"p\":" + String(dp.pressure, 2) + "}";
        }
        json += "]}"; 
        server.send(200, "application/json", json);
    });

    server.on("/toggle", [this]() { 
        weatherModel->display_mode = (weatherModel->display_mode == 0) ? 1 : 0; 
        server.send(200, "application/json", "{\"mode\":" + String(weatherModel->display_mode) + "}"); 
    });

    server.on("/api/interval", HTTP_POST, [this]() {
        if(server.hasArg("plain")) {
            String body = server.arg("plain");
            int p = body.indexOf("\"ms\":");
            if(p >= 0) {
                    int endP = body.indexOf("}", p);
                    uint32_t ms = body.substring(p + 5, endP > 0 ? endP : body.length()).toInt();
                if(ms >= 1000 && ms <= 60000) {
                    weatherModel->update_interval_ms = ms;
                }
            }
        }
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/log", [this]() {
        String csv = "time_sec,temperature_c,pressure_hpa\r\n";
        int count = weatherModel->getHistoryCount();
        for (int i = 0; i < count; i++) {
            const DataPoint& dp = weatherModel->getHistory(i);
            csv += String(dp.ts) + "," + String(dp.temp, 2) + "," + String(dp.pressure, 2) + "\r\n";
        }
        server.sendHeader("Content-Disposition", "attachment; filename=pocket_log.csv");
        server.send(200, "text/csv", csv);
    });

    server.onNotFound([this]() { 
        server.sendHeader("Location", "http://192.168.4.1", true); 
        server.send(302, "text/plain", ""); 
    });
}

const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pocket Weather</title>
<style>
  :root { --bg: #f4f4f9; --card: #fff; --text: #333; --accent: #3498db; --warn: #e67e22; }
  body{font-family:system-ui,-apple-system,sans-serif;background:var(--bg);margin:0;padding:15px;text-align:center;color:var(--text)}
  .wrap{max-width:850px;margin:0 auto}
  h1{margin:0 0 15px;font-size:24px}
  .card{background:var(--card);border-radius:12px;padding:15px;box-shadow:0 3px 6px rgba(0,0,0,.1)}
  .row{display:flex;justify-content:space-around;align-items:center}
  .val{font-size:32px;font-weight:bold}
  .unit{font-size:16px;color:#7f8c8d}
  .fc{font-size:18px;color:var(--warn);font-weight:600;margin-top:15px;padding-top:15px;border-top:1px solid #eee}
  .status-bar{font-size:13px;color:#95a5a6;display:flex;justify-content:space-between;margin-bottom:15px}
  input[type=range]{width:100%;margin-top:10px}
  .btn{display:inline-block;padding:10px 15px;border:none;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;color:#fff;margin:5px;flex:1}
  .btn-log{background:#3498db} .btn-mode{background:#2ecc71}
  .btn-container{display:flex;gap:10px;margin-top:10px}
  
  /* Адаптивная сетка (Стекинг для вертикального смартфона, Колонки для горизонтального/ПК) */
  .main-container{display:flex;flex-direction:column;gap:15px}
  .left-panel{display:flex;flex-direction:column;gap:15px;flex:1}
  .right-panel{flex:1.5;display:flex;flex-direction:column}
  .chart-container{background:var(--card);border-radius:12px;padding:15px;box-shadow:0 3px 6px rgba(0,0,0,.1);flex:1;display:flex;flex-direction:column}
  canvas{width:100%;flex:1;min-height:220px}
  
  @media(min-width: 600px) {
    .main-container{flex-direction:row;text-align:left}
    body{padding:30px}
  }
  
  #toast-container{position:fixed;top:20px;left:50%;transform:translateX(-50%);z-index:999;display:flex;flex-direction:column;gap:8px}
  .toast{padding:10px 15px;border-radius:8px;color:#fff;font-size:13px;opacity:0;transform:translateY(-10px);transition:all 0.3s;box-shadow:0 3px 6px rgba(0,0,0,.2)}
  .toast.show{opacity:1;transform:translateY(0)}
  .toast.info{background:#3498db} .toast.warn{background:#e67e22} .toast.ok{background:#27ae60}
</style>
</head>
<body>
<div class="wrap">
<h1>🌤 Pocket Weather</h1>
<div class="status-bar">
  <span id="orient">📐 Ориентация: --</span>
  <span id="interval-disp">⏱ 3 сек</span>
</div>

<div class="main-container">
  <div class="left-panel">
    <div class="card">
      <div class="row">
        <div style="text-align:center"><div style="font-size:13px;color:#95a5a6">Температура</div><div class="val"><span id="t">--</span><span class="unit">°C</span></div></div>
        <div style="text-align:center"><div style="font-size:13px;color:#95a5a6">Давление</div><div class="val"><span id="p">--</span><span class="unit">гПа</span></div></div>
      </div>
      <div class="fc" id="fc">Прогноз: --</div>
    </div>
    
    <div class="card">
      <label style="font-size:13px;color:#7f8c8d;font-weight:bold">Скорость обновления данных</label>
      <input type="range" id="int-slider" min="1" max="30" value="3" step="1">
    </div>
    
    <div class="btn-container">
      <button class="btn btn-mode" onclick="toggleMode()">🔄 Режим экрана</button>
      <button class="btn btn-log" onclick="downloadLog()">📥 Лог CSV</button>
    </div>
  </div>
  
  <div class="right-panel">
    <div class="chart-container">
      <div style="font-size:13px;color:#95a5a6;margin-bottom:10px;text-align:center;font-weight:bold">График давления</div>
      <canvas id="chart"></canvas>
    </div>
  </div>
</div>
</div>

<div id="toast-container"></div>

<script>
const canvas=document.getElementById('chart');
const ctx=canvas.getContext('2d');
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
  
  // Динамически подгоняем внутреннее разрешение канваса под CSS размер
  canvas.width = canvas.clientWidth;
  canvas.height = canvas.clientHeight;
  
  const W=canvas.width, H=canvas.height;
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
  
  ctx.fillStyle='rgba(52,152,219,0.1)'; ctx.beginPath();
  ctx.moveTo(points[0].x, pad.t+gh);
  points.forEach(p=>ctx.lineTo(p.x,p.y));
  ctx.lineTo(points[points.length-1].x, pad.t+gh); ctx.closePath(); ctx.fill();

  ctx.strokeStyle='#2c3e50'; ctx.lineWidth=2; ctx.lineCap='round'; ctx.lineJoin='round'; ctx.beginPath();
  ctx.moveTo(points[0].x, points[0].y);
  for(let i=0;i<points.length-1;i++){
    let xc=(points[i].x+points[i+1].x)/2, yc=(points[i].y+points[i+1].y)/2;
    ctx.quadraticCurveTo(points[i].x, points[i].y, xc, yc);
  }
  let last=points[points.length-1];
  ctx.quadraticCurveTo(last.x, last.y, last.x, last.y); ctx.stroke();

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
    
    // Локализация Замбретти на русский
    const fcMap = {
      "Clear, improving": "Ясно, улучшение ☀️",
      "Partly cloudy": "Переменная облачность ⛅️",
      "Clear, stable": "Ясно, стабильно ☀️",
      "Cloudy, clearing": "Облачно, прояснения 🌥",
      "Possible rain": "Возможен дождь 🌦",
      "Rain, worsening": "Дождь, ухудшение 🌧",
      "Cloudy, improving": "Облачно, улучшение 🌥",
      "Overcast": "Пасмурно ☁️"
    };
    let fcRu = fcMap[fcText] || fcText;
    document.getElementById('fc').innerHTML="Прогноз:<br><span style='color:#3498db;font-size:22px'>" + fcRu + "</span>";
    
    if (document.activeElement !== document.getElementById('int-slider')) {
      document.getElementById('interval-disp').textContent="⏱ "+(d.interval/1000)+" сек";
      document.getElementById('int-slider').value=d.interval/1000;
    }

    let rotText = d.rot==1 ? "📐 Нормально" : "📐 Перевёрнуто";
    document.getElementById('orient').textContent=rotText;

    if(fcText!==prevFC && prevFC!=="") { toast(fcRu, fcText.includes("Rain")||fcText.includes("worsening")?'warn':'info'); }
    if(d.rot!==prevRot && prevRot!=="") { toast(rotText, 'ok'); }
    prevFC=fcText; prevRot=d.rot;

    lastData=d.hist; drawChart(lastData);
  });
}

function toggleMode(){fetch('/toggle').then(r=>r.json()).then(d=>update());}
function downloadLog(){window.location.href='/log';}

const slider = document.getElementById('int-slider');
slider.addEventListener('input', e => {
  document.getElementById('interval-disp').textContent = "⏱ " + e.target.value + " сек";
});
slider.addEventListener('change', e => {
  let s = parseInt(e.target.value);
  fetch('/api/interval', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ms:s*1000})});
});

// Перерисовка графика при перевороте экрана устройства
window.addEventListener('resize', () => { if(lastData.length) drawChart(lastData); });

setInterval(update,2500); update();
</script>
</body>
</html>
)rawliteral";