// SportWatch — IMU Data Collector for Edge Impulse
//
// Workflow:
//   1. Flash this sketch, power up the watch.
//   2. Watch OLED shows AP SSID/PASSWORD/URL. Connect phone WiFi to it.
//   3. Open http://192.168.4.1 — pick activity (run/walk/other), tap Start.
//   4. Stow phone, do the activity. Recording stops automatically after
//      RECORD_SECONDS, or sooner if you reopen the page and tap Stop.
//   5. After collecting all 3 activities, tap "Download" links to pull each
//      CSV. Upload directly to Edge Impulse Studio (Data acquisition →
//      Upload data → CSV wizard).
//
// CSV format (Edge Impulse compatible):
//   timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ
//   (timestamp = ms since recording start)
//
// Partition: select "Default 4MB with spiffs" or larger so LittleFS has
// ≥1.5MB free (each 15-min session ≈ 2MB).

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------- Pin/Hardware --------
#define I2C_SDA           8
#define I2C_SCL           9
#define MPU_ADDR          0x68
#define OLED_ADDR         0x3C
#define OLED_W            128
#define OLED_H            64

// -------- Sampling --------
#define SAMPLE_HZ         50
#define SAMPLE_PERIOD_US  (1000000UL / SAMPLE_HZ)
#define RECORD_SECONDS    900   // 15-min cap per session

// -------- WiFi AP --------
static const char* AP_SSID = "SportWatch-Collect";
static const char* AP_PASS = "12345678";

// -------- Globals --------
static Adafruit_MPU6050 mpu;
static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static WebServer server(80);

enum State { ST_IDLE, ST_RECORDING, ST_DONE };
static State g_state = ST_IDLE;

static File g_file;
static String g_label = "";
static uint32_t g_record_start_ms = 0;
static uint32_t g_sample_count = 0;
static uint64_t g_next_sample_us = 0;

// -------- OLED helpers --------
static void oled_show(const String& l1, const String& l2 = "",
                      const String& l3 = "", const String& l4 = "") {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);  oled.println(l1);
  oled.setCursor(0, 16); oled.println(l2);
  oled.setCursor(0, 32); oled.println(l3);
  oled.setCursor(0, 48); oled.println(l4);
  oled.display();
}

static void oled_status() {
  if (g_state == ST_IDLE) {
    oled_show("AP: SportWatch-Coll", "PW: 12345678",
              "http://192.168.4.1", String("Files: ") + String(LittleFS.usedBytes() / 1024) + "KB");
  } else if (g_state == ST_RECORDING) {
    uint32_t elapsed = (millis() - g_record_start_ms) / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lu:%02lu / %02d:%02d",
             (unsigned long)(elapsed / 60), (unsigned long)(elapsed % 60),
             RECORD_SECONDS / 60, RECORD_SECONDS % 60);
    oled_show("REC: " + g_label, buf,
              String("Samples: ") + g_sample_count,
              "open /stop to halt");
  } else {
    oled_show("Done: " + g_label,
              String("Samples: ") + g_sample_count,
              "Reconnect WiFi to",
              "download CSV file.");
  }
}

// -------- Recording --------
static String filename_for(const String& label) {
  // Avoid collisions: append upmillis. Edge Impulse only cares about content.
  return "/" + label + "_" + String(millis()) + ".csv";
}

static bool start_recording(const String& label) {
  if (g_state == ST_RECORDING) return false;
  String fname = filename_for(label);
  g_file = LittleFS.open(fname, FILE_WRITE);
  if (!g_file) {
    Serial.println("[rec] open failed");
    return false;
  }
  g_file.println("timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ");
  g_label = label;
  g_state = ST_RECORDING;
  g_record_start_ms = millis();
  g_sample_count = 0;
  g_next_sample_us = micros();
  Serial.printf("[rec] start %s -> %s\n", label.c_str(), fname.c_str());
  return true;
}

static void stop_recording() {
  if (g_state != ST_RECORDING) return;
  g_file.flush();
  g_file.close();
  g_state = ST_DONE;
  Serial.printf("[rec] stop, %lu samples\n", (unsigned long)g_sample_count);
}

static void sample_tick() {
  if (g_state != ST_RECORDING) return;
  uint64_t now = micros();
  if ((int64_t)(now - g_next_sample_us) < 0) return;
  g_next_sample_us += SAMPLE_PERIOD_US;
  // If we fell behind (e.g., WiFi stall), resync rather than burst-catch-up.
  if ((int64_t)(now - g_next_sample_us) > (int64_t)SAMPLE_PERIOD_US * 5) {
    g_next_sample_us = now + SAMPLE_PERIOD_US;
  }

  sensors_event_t a, gy, t;
  if (!mpu.getEvent(&a, &gy, &t)) return;

  uint32_t ts_ms = millis() - g_record_start_ms;
  // Write directly to file. LittleFS buffers internally; flush at end.
  g_file.printf("%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                (unsigned long)ts_ms,
                a.acceleration.x, a.acceleration.y, a.acceleration.z,
                gy.gyro.x, gy.gyro.y, gy.gyro.z);
  g_sample_count++;

  if (millis() - g_record_start_ms >= (uint32_t)RECORD_SECONDS * 1000UL) {
    stop_recording();
  }
}

// -------- Web UI --------
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SportWatch Collector</title>
<style>
body{font-family:system-ui;margin:0;padding:16px;background:#111;color:#eee}
h1{font-size:18px;margin:0 0 12px}
.btn{display:block;width:100%;padding:18px;margin:8px 0;font-size:18px;
     border:0;border-radius:10px;color:#fff;text-align:center;text-decoration:none}
.run{background:#c0392b}.walk{background:#2980b9}.other{background:#27ae60}
.stop{background:#7f8c8d}.dl{background:#34495e;font-size:14px;padding:10px}
.status{padding:12px;background:#222;border-radius:8px;margin-bottom:12px;font-family:monospace}
hr{border:0;border-top:1px solid #333;margin:16px 0}
</style></head><body>
<h1>SportWatch · Data Collector</h1>
<div class="status" id="st">loading…</div>
<a class="btn run"   href="/start?label=running">▶ Start RUNNING (15 min)</a>
<a class="btn walk"  href="/start?label=walking">▶ Start WALKING (15 min)</a>
<a class="btn other" href="/start?label=other">▶ Start OTHER (15 min)</a>
<a class="btn stop"  href="/stop">■ Stop now</a>
<hr><h1>Files</h1>
<div id="files">…</div>
<script>
async function refresh(){
  let s=await(await fetch('/status')).json();
  document.getElementById('st').textContent=
    `state: ${s.state}  label: ${s.label}  samples: ${s.samples}  elapsed: ${s.elapsed}s  flash: ${s.usedKB}/${s.totalKB} KB`;
  let f=await(await fetch('/list')).json();
  document.getElementById('files').innerHTML =
    f.files.length===0 ? '<i>no files yet</i>' :
    f.files.map(x=>`<div><a class="btn dl" href="/dl?f=${encodeURIComponent(x.name)}">⬇ ${x.name} (${x.size} B)</a> <a class="btn dl" href="/rm?f=${encodeURIComponent(x.name)}" style="background:#8b0000">delete</a></div>`).join('');
}
refresh(); setInterval(refresh,2000);
</script></body></html>
)HTML";

static void handle_root() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handle_start() {
  String label = server.arg("label");
  if (label != "running" && label != "walking" && label != "other") {
    server.send(400, "text/plain", "bad label");
    return;
  }
  if (!start_recording(label)) {
    server.send(409, "text/plain", "already recording");
    return;
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handle_stop() {
  stop_recording();
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handle_status() {
  uint32_t elapsed = (g_state == ST_RECORDING) ? (millis() - g_record_start_ms) / 1000 : 0;
  const char* st = (g_state == ST_IDLE) ? "idle" : (g_state == ST_RECORDING) ? "recording" : "done";
  String j = String("{\"state\":\"") + st +
             "\",\"label\":\"" + g_label +
             "\",\"samples\":" + g_sample_count +
             ",\"elapsed\":" + elapsed +
             ",\"usedKB\":" + (LittleFS.usedBytes() / 1024) +
             ",\"totalKB\":" + (LittleFS.totalBytes() / 1024) + "}";
  server.send(200, "application/json", j);
}

static void handle_list() {
  String j = "{\"files\":[";
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  bool first = true;
  while (f) {
    if (!f.isDirectory()) {
      if (!first) j += ",";
      first = false;
      j += String("{\"name\":\"") + f.name() + "\",\"size\":" + f.size() + "}";
    }
    f = root.openNextFile();
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handle_download() {
  String name = server.arg("f");
  if (!name.startsWith("/")) name = "/" + name;
  if (!LittleFS.exists(name)) { server.send(404, "text/plain", "not found"); return; }
  File f = LittleFS.open(name, FILE_READ);
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + name.substring(1) + "\"");
  server.streamFile(f, "text/csv");
  f.close();
}

static void handle_remove() {
  String name = server.arg("f");
  if (!name.startsWith("/")) name = "/" + name;
  LittleFS.remove(name);
  server.sendHeader("Location", "/");
  server.send(303);
}

// -------- Setup / Loop --------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[collector] boot");

  Wire.begin(I2C_SDA, I2C_SCL, 400000);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[collector] OLED fail");
  }
  oled_show("Collector boot", "init MPU...");

  if (!mpu.begin(MPU_ADDR, &Wire)) {
    Serial.println("[collector] MPU fail");
    oled_show("MPU6050 NOT FOUND", "check wiring", "SDA=8 SCL=9");
    while (1) delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  if (!LittleFS.begin(true)) {
    Serial.println("[collector] LittleFS fail");
    oled_show("LittleFS FAILED", "check partition", "scheme in IDE");
    while (1) delay(1000);
  }
  Serial.printf("[collector] LittleFS %u/%u KB\n",
                (unsigned)(LittleFS.usedBytes() / 1024),
                (unsigned)(LittleFS.totalBytes() / 1024));

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[collector] AP %s ip %s\n", AP_SSID, ip.toString().c_str());

  server.on("/", handle_root);
  server.on("/start", handle_start);
  server.on("/stop", handle_stop);
  server.on("/status", handle_status);
  server.on("/list", handle_list);
  server.on("/dl", handle_download);
  server.on("/rm", handle_remove);
  server.begin();

  oled_status();
}

void loop() {
  server.handleClient();
  sample_tick();

  static uint32_t last_oled = 0;
  if (millis() - last_oled >= 500) {
    last_oled = millis();
    oled_status();
  }
}
