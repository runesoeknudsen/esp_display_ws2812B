#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include "big_digits.h"
#include "digits.h"

namespace {
constexpr uint8_t DATA_PIN = 13;
constexpr uint8_t PANEL_COLUMNS = 32;
constexpr uint8_t PANEL_ROWS = 8;
constexpr uint8_t PANEL_COUNT = 2;
constexpr uint8_t DISPLAY_COLUMNS = 32;
constexpr uint8_t DISPLAY_ROWS = 16;
constexpr uint16_t LED_COUNT = PANEL_COLUMNS * PANEL_ROWS * PANEL_COUNT;
constexpr bool PANEL_VERTICAL_SERPENTINE = true;
// Viewed from the front, arrange the panels as:
//   panel 1 (rotated 180 degrees)
//   panel 0 (chain input)
// Panel 0's output connects to panel 1's input. Panel 1 is physically above
// panel 0 and is rotated so its pixels face the same logical orientation.
constexpr uint8_t PANEL_ORDER[PANEL_COUNT] = {1, 0};

Adafruit_NeoPixel leds(LED_COUNT, DATA_PIN, NEO_GRB + NEO_KHZ800);
Preferences preferences;
WebServer server(80);
uint32_t configuredSeconds = 240;
uint32_t remainingSeconds = 240;
uint32_t lastTick = 0;
bool running = false;
bool finished = false;
uint8_t brightness = 16;

uint16_t ledIndex(uint8_t x, uint8_t y) {
  const uint8_t tileY = y / PANEL_ROWS;
  const uint8_t panel = PANEL_ORDER[tileY];
  uint8_t localX = x % PANEL_COLUMNS;
  uint8_t localY = y % PANEL_ROWS;
  if (tileY == 0) {
    localX = PANEL_COLUMNS - 1 - localX;
    localY = PANEL_ROWS - 1 - localY;
  }
  if (PANEL_VERTICAL_SERPENTINE && (localX & 1)) localY = PANEL_ROWS - 1 - localY;
  return panel * PANEL_COLUMNS * PANEL_ROWS + localX * PANEL_ROWS + localY;
}

void setPixel(uint8_t x, uint8_t y, uint32_t color) {
  if (x < DISPLAY_COLUMNS && y < DISPLAY_ROWS) leds.setPixelColor(ledIndex(x, y), color);
}

void drawDigit(uint8_t digit, uint8_t x, uint8_t y, uint32_t color) {
  for (uint8_t row = 0; row < 7; row++) {
    for (uint8_t col = 0; col < 5; col++) {
      if (DIGITS[digit][row][col] == '#') {
        setPixel(x + col, y + row, color);
      }
    }
  }
}

void drawBigDigit(uint8_t digit, uint8_t x, uint8_t y, uint32_t color) {
  for (uint8_t row = 0; row < 11; row++) {
    for (uint8_t col = 0; col < 5; col++) {
      if (BIG_DIGITS[digit][row][col] == '#') setPixel(x + col, y + row, color);
    }
  }
}

void renderDisplay() {
  leds.clear();
  const uint32_t color = finished ? leds.Color(255, 24, 8) : leds.Color(255, 150, 20);
  const uint8_t minutes = remainingSeconds / 60;
  const uint8_t seconds = remainingSeconds % 60;
  const uint8_t digits[] = {
    static_cast<uint8_t>(minutes / 10), static_cast<uint8_t>(minutes % 10),
    static_cast<uint8_t>(seconds / 10), static_cast<uint8_t>(seconds % 10)
  };
  const uint8_t positions[] = {2, 9, 18, 25};
  const uint8_t digitY = (DISPLAY_ROWS - 11) / 2;
  for (uint8_t i = 0; i < 4; i++) drawBigDigit(digits[i], positions[i], digitY, color);
  for (uint8_t y = 5; y < 7; y++) {
    setPixel(15, y, color);
    setPixel(16, y, color);
  }
  for (uint8_t y = 9; y < 11; y++) {
    setPixel(15, y, color);
    setPixel(16, y, color);
  }
  leds.setBrightness(brightness);
  leds.show();
}

void saveSettings() {
  preferences.begin("archery", false);
  preferences.putUInt("duration", configuredSeconds);
  preferences.putUChar("brightness", brightness);
  preferences.end();
}

void loadSettings() {
  preferences.begin("archery", true);
  configuredSeconds = preferences.getUInt("duration", 240);
  brightness = preferences.getUChar("brightness", 64);
  preferences.end();
  if (configuredSeconds < 1 || configuredSeconds > 5999) configuredSeconds = 240;
  if (brightness == 0) brightness = 64;
  remainingSeconds = configuredSeconds;
}

String page() {
  return R"HTML(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Archery timer</title><style>body{font-family:system-ui,sans-serif;background:#10151b;color:#f4f0e8;max-width:520px;margin:40px auto;padding:0 20px}main{border:1px solid #39434c;padding:24px;background:#18212a}h1{margin-top:0;color:#ff9638}#time{font-size:4rem;text-align:center;font-variant-numeric:tabular-nums;margin:24px 0}label{display:block;margin:14px 0 6px}input,button{font:inherit;padding:10px;border-radius:4px;border:1px solid #52606b}input{width:100%;box-sizing:border-box;background:#0d1217;color:white}button{margin:16px 6px 0 0;background:#ff9638;color:#161616;font-weight:700;cursor:pointer}button.secondary{background:#d8e0e5}.status{color:#b8c5ce}</style></head><body><main>
<h1>Archery timer</h1><div id="time">04:00</div><div class="status" id="status">Ready</div>
<label for="duration">Round duration (seconds)</label><input id="duration" type="number" min="1" max="5999">
<label for="brightness">Brightness <output id="brightnessValue">16</output></label><input id="brightness" type="range" min="1" max="255">
<button onclick="send('/api/start')">Start</button><button class="secondary" onclick="send('/api/pause')">Pause</button><button class="secondary" onclick="send('/api/reset')">Reset</button><button onclick="save()">Save settings</button></main><script>
async function state(){let s=await fetch('/api/state').then(r=>r.json());let m=Math.floor(s.remaining/60),sec=s.remaining%60;time.textContent=String(m).padStart(2,'0')+':'+String(sec).padStart(2,'0');duration.value=s.duration;brightness.value=s.brightness;brightnessValue.textContent=s.brightness;status.textContent=s.finished?'TIME':s.running?'Running':'Ready'}
async function send(url){await fetch(url,{method:'POST'});state()} async function save(){await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({duration:+duration.value,brightness:+brightness.value})});state()} async function applyBrightness(){brightnessValue.textContent=brightness.value;await fetch('/api/brightness',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({brightness:+brightness.value})});state()} brightness.addEventListener('change',applyBrightness); state();setInterval(state,1000);
</script></body></html>)HTML";
}

void sendState() {
  String json = "{\"duration\":" + String(configuredSeconds) + ",\"remaining\":" + String(remainingSeconds) +
                ",\"brightness\":" + String(brightness) + ",\"running\":" + (running ? "true" : "false") +
                ",\"finished\":" + (finished ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", page()); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/start", HTTP_POST, []() { if (remainingSeconds == 0) remainingSeconds = configuredSeconds; finished = false; running = true; lastTick = millis(); renderDisplay(); sendState(); });
  server.on("/api/pause", HTTP_POST, []() { running = false; sendState(); });
  server.on("/api/reset", HTTP_POST, []() { running = false; finished = false; remainingSeconds = configuredSeconds; renderDisplay(); sendState(); });
  server.on("/api/settings", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      int durationSeparator = body.indexOf(':', body.indexOf("duration"));
      int brightnessSeparator = body.indexOf(':', body.indexOf("brightness"));
      int duration = durationSeparator >= 0 ? body.substring(durationSeparator + 1).toInt() : 0;
      int level = brightnessSeparator >= 0 ? body.substring(brightnessSeparator + 1).toInt() : 0;
      if (duration >= 1 && duration <= 5999) configuredSeconds = duration;
      if (level >= 1 && level <= 255) brightness = level;
      remainingSeconds = configuredSeconds;
      running = false;
      finished = false;
      saveSettings();
      renderDisplay();
    }
    sendState();
  });
  server.on("/api/brightness", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      int separator = body.indexOf(':');
      int level = separator >= 0 ? body.substring(separator + 1).toInt() : 0;
      if (level >= 1 && level <= 255) {
        brightness = level;
        saveSettings();
        renderDisplay();
      }
    }
    sendState();
  });
  server.begin();
}
}

void setup() {
  Serial.begin(115200);
  loadSettings();
  leds.begin();
  leds.setBrightness(brightness);
  leds.clear();
  renderDisplay();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Archery-Timer", "archery123");
  Serial.print("Open http://");
  Serial.println(WiFi.softAPIP());
  setupWeb();
}

void loop() {
  server.handleClient();
  if (running && millis() - lastTick >= 1000) {
    lastTick += 1000;
    if (remainingSeconds > 0) remainingSeconds--;
    if (remainingSeconds == 0) {
      running = false;
      finished = true;
    }
    renderDisplay();
  }
}