#include "webserver.h"
#include "provision.h"
#include "ui.h"
#include "config.h"
#include <Arduino.h>
#if defined(ESP32)
#include <WebServer.h>
#include <Update.h>
static WebServer srv(80);
#else
#include <ESP8266WebServer.h>
#include <Updater.h>
static ESP8266WebServer srv(80);
#endif

// ESP32's Update class exposes errorString(); ESP8266's Updater getErrorString().
#if defined(ESP32)
#define OTA_ERR() Update.errorString()
#else
#define OTA_ERR() Update.getErrorString()
#endif

static const UsageData* _usage = nullptr;

static const char OTA_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Monitor</title>
<style>
  body{font-family:system-ui,sans-serif;background:#191919;color:#e0e0e0;padding:16px}
  h2{color:#e8733a;margin-bottom:4px}
  .card{background:#252525;border:1px solid #3a3a3a;border-radius:10px;
        padding:14px 18px;max-width:420px;margin:12px auto}
  .row{display:flex;justify-content:space-between;padding:4px 0;
       border-bottom:1px solid #333}
  .row:last-child{border-bottom:none}
  .label{color:#888;font-size:.9em}
  .val{font-weight:600}
  .bar-bg{background:#111;border-radius:4px;height:10px;margin-top:6px}
  .bar{background:#e8733a;border-radius:4px;height:10px}
  button,input[type=submit]{background:#e8733a;color:#111;border:none;
    border-radius:8px;padding:9px 18px;font-weight:700;cursor:pointer;margin-top:8px}
  .danger{background:#c0392b}
  input[type=file]{color:#e0e0e0;margin:8px 0}
  #msg{margin-top:8px;font-size:.9em;min-height:1em}
  .ok{color:#4f4}.err{color:#f66}
</style></head>
<body>
<div class="card">
  <h2>Claude Usage Monitor</h2>
  <p style="color:#888;font-size:.85em">GeekMagic SmallTV Ultra</p>
</div>

<div class="card" id="stats">
  <div class="row"><span class="label">5-Hour Usage</span>
    <span class="val" id="h5">--</span></div>
  <div class="bar-bg"><div class="bar" id="h5bar" style="width:0%"></div></div>
  <div class="row" style="margin-top:8px"><span class="label">7-Day Usage</span>
    <span class="val" id="d7">--</span></div>
  <div class="bar-bg"><div class="bar" id="d7bar" style="width:0%"></div></div>
</div>

<div class="card">
  <b>Update Firmware</b>
  <form id="uf" method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="firmware" accept=".bin"><br>
    <input type="submit" value="Upload .bin">
  </form>
  <div id="msg"></div>
</div>

<div class="card">
  <b>Device</b><br>
  <button class="danger" onclick="if(confirm('Factory reset?'))fetch('/reset')
    .then(()=>document.getElementById('msg').textContent='Resetting...')">
    Factory Reset</button>
</div>

<script>
fetch('/status').then(r=>r.json()).then(d=>{
  document.getElementById('h5').textContent=d.h5.toFixed(1)+'%';
  document.getElementById('d7').textContent=d.d7.toFixed(1)+'%';
  document.getElementById('h5bar').style.width=Math.min(d.h5,100)+'%';
  document.getElementById('d7bar').style.width=Math.min(d.d7,100)+'%';
}).catch(()=>{});

document.getElementById('uf').addEventListener('submit',async e=>{
  e.preventDefault();
  const msg=document.getElementById('msg');
  msg.className='';msg.textContent='Uploading...';
  try{
    const r=await fetch('/update',{method:'POST',body:new FormData(e.target)});
    if(r.ok){msg.className='ok';msg.textContent='Done! Rebooting...';}
    else{msg.className='err';msg.textContent='Failed: '+await r.text();}
  }catch{msg.className='ok';msg.textContent='Rebooting (connection closed)...';}
});
</script>
</body></html>)html";

// ── Handlers ───────────────────────────────────────────────

static void handleRoot() {
    srv.send_P(200, "text/html", OTA_HTML);
}

static void handleStatus() {
    char buf[128];
    if (_usage && _usage->ok) {
        snprintf(buf, sizeof(buf),
            "{\"h5\":%.2f,\"d7\":%.2f,\"h5r\":%u,\"d7r\":%u}",
            _usage->h5, _usage->d7, _usage->h5ResetEpoch, _usage->d7ResetEpoch);
    } else {
        snprintf(buf, sizeof(buf), "{\"h5\":0,\"d7\":0,\"error\":\"%s\"}",
            _usage ? _usage->error : "no data");
    }
    srv.send(200, "application/json", buf);
}

static void handleReset() {
    configClear();
    srv.send(200, "text/plain", "Config cleared. Rebooting...");
    delay(500);
    ESP.restart();
}

static void handleOtaFinish() {
    bool ok = !Update.hasError();
    srv.send(ok ? 200 : 500, "text/plain", ok ? "OK" : OTA_ERR());
    delay(500);
    ESP.restart();
}

static void handleOtaUpload() {
    HTTPUpload& up = srv.upload();
    if (up.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Start: %s\n", up.filename.c_str());
#if defined(ESP32)
        bool begun = Update.begin(UPDATE_SIZE_UNKNOWN);
#else
        uint32_t maxSketch = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        bool begun = Update.begin(maxSketch);
#endif
        if (!begun) {
            Serial.printf("[OTA] begin error: %s\n", OTA_ERR());
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            Serial.printf("[OTA] write error: %s\n", OTA_ERR());
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[OTA] Success: %u bytes\n", up.totalSize);
        } else {
            Serial.printf("[OTA] end error: %s\n", OTA_ERR());
        }
    }
}

// ── Public ─────────────────────────────────────────────────

void webserverBegin(const UsageData* usagePtr) {
    _usage = usagePtr;
    srv.on("/",       HTTP_GET,  handleRoot);
    srv.on("/status", HTTP_GET,  handleStatus);
    srv.on("/reset",  HTTP_GET,  handleReset);
    srv.on("/update", HTTP_POST, handleOtaFinish, handleOtaUpload);
    srv.begin();
    Serial.printf("[WEB] Listening on http://%s/\n", WiFi.localIP().toString().c_str());
}

void webserverHandle() {
    srv.handleClient();
}
