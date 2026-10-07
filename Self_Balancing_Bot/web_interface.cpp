#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h> // Links2004/arduinoWebSockets -- Library Manager: "WebSockets" by Markus Sattler
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "web_interface.h"
#include "config.h"

// WebServer instance is private to this file -- every other
// module only ever sees initWebInterface().
static WebServer server(80);

// Push-based telemetry lives on its own port so it never has to share
// a TCP connection (or the HTTP server's blocking handleClient() call)
// with page/API requests. Port 81 is what the dashboard's JS expects.
static WebSocketsServer wsServer(81);

// How often a fresh sample goes out to every connected client. 20 Hz
// is fast enough for the dashboard while reducing WiFi traffic and
// repeated JSON String allocations on the ESP32.
static const uint32_t WS_BROADCAST_INTERVAL_MS = 50;
static uint32_t lastWsBroadcastMs = 0;

static void webInterfaceTask(void *parameter);

// ============================================================
// WEB PAGE  (live telemetry and PID controls)
// ============================================================

static void handleRoot()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Balance Robot — Telemetry &amp; PID Controls</title>
<style>
/* ==============================================================
   DESIGN TOKENS
   Keep these in sync with the COLORS object in the <script> below —
   canvas drawing can't read CSS variables cheaply at 30fps, so the
   chart colors are duplicated there on purpose.
   ============================================================== */
:root{
  --bg:#070b12;
  --panel:#0d1521;
  --panel-2:#111c2e;
  --border:#243550;
  --border-soft:#182437;
  --text:#e7edf7;
  --text-dim:#8ea0bb;
  --text-faint:#5b6d87;
  --mono: ui-monospace, "SFMono-Regular", "Cascadia Mono", Consolas, "Roboto Mono", monospace;
  --sans: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;

  --pitch:#4ade80;    /* green  */
  --setpoint:#f8fafc; /* white  */
  --error:#facc15;    /* yellow */
  --p-term:#ef4444;   /* red    */
  --i-term:#4ade80;   /* green  */
  --d-term:#3b82f6;   /* blue   */

  --accent:#38bdf8;
  --good:#34d399;
  --crit:#f87171;
}

*{box-sizing:border-box;}
html,body{margin:0;padding:0;background:var(--bg);color:var(--text);}
body{font-family:var(--sans);padding:18px;-webkit-font-smoothing:antialiased;}
.dash{max-width:1200px;margin:0 auto;display:flex;flex-direction:column;gap:16px;}

/* ---- header ------------------------------------------------- */
.dash-head{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:12px;}
.brand{display:flex;align-items:center;gap:12px;}
.brand-mark{font-size:20px;color:var(--accent);text-shadow:0 0 12px rgba(56,189,248,.55);}
.brand h1{font-size:17px;letter-spacing:.06em;margin:0;font-weight:700;}
.brand-sub{margin:2px 0 0;font-size:11.5px;color:var(--text-dim);font-family:var(--mono);letter-spacing:.03em;}

.status-cluster{display:flex;align-items:center;gap:8px;flex-wrap:wrap;}
.status-pill{display:flex;align-items:center;gap:8px;background:var(--panel);border:1px solid var(--border);border-radius:999px;padding:7px 13px;font-size:11.5px;font-family:var(--mono);color:var(--text-dim);}
.dot{width:8px;height:8px;border-radius:50%;background:var(--text-faint);transition:background .2s;}
.dot-live{background:var(--good);box-shadow:0 0 8px rgba(52,211,153,.7);}
.dot-wait{background:var(--text-faint);}
.btn{background:var(--panel);border:1px solid var(--border);color:var(--text);font-family:var(--mono);font-size:11px;letter-spacing:.05em;padding:8px 12px;border-radius:6px;cursor:pointer;}
.btn:hover{border-color:var(--accent);color:var(--accent);}
.btn:active{transform:translateY(1px);}

/* ---- shared panel shell -------------------------------------- */
.panel{background:var(--panel);border:1px solid var(--border);border-radius:10px;padding:14px 16px 16px;}
.panel-head{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:10px;flex-wrap:wrap;gap:6px;}
.panel-head h2{font-size:13px;margin:0;letter-spacing:.04em;color:var(--text);font-weight:700;}
.panel-tag{font-family:var(--mono);font-size:10px;color:var(--text-faint);letter-spacing:.05em;}

/* ---- PID input ----------------------------------------------- */
.pid-form{display:grid;grid-template-columns:repeat(4,minmax(120px,1fr)) auto;gap:10px;align-items:end;}
.pid-field{display:flex;flex-direction:column;gap:5px;}
.pid-field label{font-size:9.5px;color:var(--text-faint);letter-spacing:.05em;text-transform:uppercase;}
.pid-field input{width:100%;background:var(--panel-2);border:1px solid var(--border-soft);border-radius:6px;color:var(--text);font-family:var(--mono);font-size:14px;padding:9px 10px;}
.pid-field input:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 2px rgba(56,189,248,.15);}
.pid-actions{display:flex;flex-direction:column;gap:5px;}
.pid-status{min-height:13px;font-family:var(--mono);font-size:10px;color:var(--text-dim);}
.pid-status.error{color:var(--crit);}
.pid-status.success{color:var(--good);}

/* ---- charts ---------------------------------------------------*/
.chart-row{display:grid;grid-template-columns:1fr 1fr;gap:16px;}
.canvas-wrap{position:relative;width:100%;height:260px;}
.canvas-wrap-lg{height:420px;}
canvas{width:100%;height:100%;display:block;}

/* ---- metrics grid ------------------------------------------- */
.metrics-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px;}
.metric{background:var(--panel-2);border:1px solid var(--border-soft);border-radius:8px;padding:10px 12px;}
.metric-label{font-size:9.5px;color:var(--text-faint);letter-spacing:.05em;text-transform:uppercase;}
.metric-value{font-family:var(--mono);font-size:20px;font-weight:600;margin-top:4px;color:var(--text);}
.telemetry-tables{display:grid;grid-template-columns:minmax(220px,.8fr) minmax(360px,1.4fr) minmax(220px,.8fr);gap:14px;align-items:start;}
.telemetry-table{width:100%;border-collapse:collapse;background:var(--panel-2);border:1px solid var(--border-soft);border-radius:8px;overflow:hidden;font-family:var(--mono);font-size:11px;}
.telemetry-table th,.telemetry-table td{padding:9px 10px;border-bottom:1px solid var(--border-soft);white-space:nowrap;}
.telemetry-table tr:last-child th,.telemetry-table tr:last-child td{border-bottom:none;}
.telemetry-table thead th{background:rgba(56,189,248,.08);color:var(--text-dim);font-size:10px;letter-spacing:.06em;text-align:left;}
.telemetry-table tbody th{color:var(--text-dim);font-weight:400;text-align:left;}
.telemetry-table td{color:var(--text);font-weight:600;text-align:right;}
.motor-table thead th:not(:first-child),.motor-table td{text-align:center;}
.motor-table tbody th{text-align:left;}
.metric.c-pitch .metric-value{color:var(--pitch);text-shadow:0 0 10px rgba(74,222,128,.25);}
.metric.c-error .metric-value{color:var(--error);text-shadow:0 0 10px rgba(250,204,21,.22);}
.metric.c-accent .metric-value{color:var(--accent);text-shadow:0 0 10px rgba(56,189,248,.25);}
.metric.c-p .metric-value{color:var(--p-term);text-shadow:0 0 10px rgba(239,68,68,.2);}
.metric.c-i .metric-value{color:var(--i-term);text-shadow:0 0 10px rgba(74,222,128,.2);}
.metric.c-d .metric-value{color:var(--d-term);text-shadow:0 0 10px rgba(59,130,246,.25);}

@media(max-width:900px){
  .chart-row{grid-template-columns:1fr;}
  .telemetry-tables{grid-template-columns:1fr 1fr;}
  .motor-table{grid-column:1 / -1;grid-row:1;}
}
@media(max-width:640px){
  .canvas-wrap{height:200px;}
  .canvas-wrap-lg{height:260px;}
  .dash-head{flex-direction:column;align-items:flex-start;}
  .pid-form{grid-template-columns:repeat(2,minmax(120px,1fr));}
  .telemetry-tables{grid-template-columns:1fr;}
  .motor-table{grid-column:auto;grid-row:auto;}
}
</style>
</head>
<body>
<div class="dash">

  <header class="dash-head">
    <div class="brand">
      <span class="brand-mark">&#9679;</span>
      <div>
        <h1>BALANCE ROBOT TELEMETRY</h1>
        <p class="brand-sub">Self-balancing PID diagnostics console</p>
      </div>
    </div>
    <div class="status-cluster">
      <div class="status-pill"><span class="dot dot-wait" id="conn-dot"></span><span id="conn-label">Initializing…</span></div>
      <button class="btn" id="btn-start">START</button>
      <button class="btn" id="btn-pause">PAUSE</button>
      <button class="btn" id="btn-export">EXPORT CSV</button>
      <button class="btn" id="btn-export-full">EXPORT FULL DATA</button>
      <button class="btn" id="btn-reset">RESET</button>
    </div>
  </header>

  <section class="panel">
    <div class="panel-head">
      <h2>PID Parameters</h2>
      <span class="panel-tag">SENT TO ROBOT</span>
    </div>
    <form class="pid-form" id="pid-form">
      <div class="pid-field">
        <label for="pid-kp">Kp</label>
        <input id="pid-kp" type="number" min="-1000" max="1000" step="any" required>
      </div>
      <div class="pid-field">
        <label for="pid-ki">Ki</label>
        <input id="pid-ki" type="number" min="-1000" max="1000" step="any" required>
      </div>
      <div class="pid-field">
        <label for="pid-kd">Kd</label>
        <input id="pid-kd" type="number" min="-1000" max="1000" step="any" required>
      </div>
      <div class="pid-field">
        <label for="pid-setpoint">Setpoint</label>
        <input id="pid-setpoint" type="number" min="-20" max="20" step="any" required>
      </div>
      <div class="pid-field">
        <label for="pid-positionhold">Position Hold</label>
        <input id="pid-positionhold" type="number" min="0" max="1" step="any" required>
      </div>
      <div class="pid-actions">
        <span class="pid-status" id="pid-status">Change a value to send it</span>
      </div>
    </form>
  </section>

  <div class="chart-row">
    <section class="panel">
      <div class="panel-head">
        <h2>01 — Pitch Response</h2>
        <span class="panel-tag">PITCH · SETPOINT · ERROR (deg)</span>
      </div>
      <div class="canvas-wrap"><canvas id="chart-pitch"></canvas></div>
    </section>

    <section class="panel">
      <div class="panel-head">
        <h2>02 — Pitch Angle Detail</h2>
        <span class="panel-tag">HIGH-RESOLUTION · STABILITY &amp; VIBRATION</span>
      </div>
      <div class="canvas-wrap"><canvas id="chart-detail"></canvas></div>
    </section>
  </div>

  <section class="panel">
    <div class="panel-head">
      <h2>03 — PID Term Contributions</h2>
      <span class="panel-tag">P · I · D (controller output) · enlarged for tuning</span>
    </div>
    <div class="canvas-wrap canvas-wrap-lg"><canvas id="chart-pid"></canvas></div>
  </section>

  <section class="panel">
    <div class="panel-head">
      <h2>Metrics</h2>
      <span class="panel-tag" id="sample-count">0 samples</span>
    </div>
    <div class="metrics-grid">
      <div class="metric c-pitch"><div class="metric-label">Current Pitch Angle</div><div class="metric-value" id="m-pitch">—</div></div>
      <div class="metric c-error"><div class="metric-label">Current Error</div><div class="metric-value" id="m-error">—</div></div>
      <div class="metric c-error"><div class="metric-label">RMS Error</div><div class="metric-value" id="m-rms">—</div></div>
      <div class="metric c-error"><div class="metric-label">Average Error</div><div class="metric-value" id="m-avg">—</div></div>
      <div class="metric c-error"><div class="metric-label">Maximum Error</div><div class="metric-value" id="m-max">—</div></div>
      <div class="metric c-accent"><div class="metric-label">Peak Overshoot</div><div class="metric-value" id="m-overshoot">—</div></div>
      <div class="metric c-accent"><div class="metric-label">Oscillation Freq.</div><div class="metric-value" id="m-oscfreq">—</div></div>
      <div class="metric"><div class="metric-label">Loop Rate</div><div class="metric-value" id="m-looprate">—</div></div>
      <div class="metric c-p"><div class="metric-label">Current Kp</div><div class="metric-value" id="m-kp">—</div></div>
      <div class="metric c-i"><div class="metric-label">Current Ki</div><div class="metric-value" id="m-ki">—</div></div>
      <div class="metric c-d"><div class="metric-label">Current Kd</div><div class="metric-value" id="m-kd">—</div></div>
      <div class="metric c-accent"><div class="metric-label">Left Wheel RPM</div><div class="metric-value" id="m-left-rpm">—</div></div>
      <div class="metric c-accent"><div class="metric-label">Right Wheel RPM</div><div class="metric-value" id="m-right-rpm">—</div></div>
      <div class="metric"><div class="metric-label">Left PWM</div><div class="metric-value" id="m-left-pwm">—</div></div>
      <div class="metric"><div class="metric-label">Right PWM</div><div class="metric-value" id="m-right-pwm">—</div></div>
      <div class="metric"><div class="metric-label">MPU6050</div><div class="metric-value" id="m-mpu">—</div></div>
    </div>
  </section>

  <section class="panel">
    <div class="panel-head">
      <h2>Live Hardware Telemetry</h2>
      <span class="panel-tag">MOTORS · ENCODERS · IMU</span>
    </div>
    <div class="telemetry-tables">
      <table class="telemetry-table sensor-table">
        <thead><tr><th colspan="2">MPU6050 / SYSTEM</th></tr></thead>
        <tbody>
          <tr><th>Accelerometer angle</th><td id="t-accel">—</td></tr>
          <tr><th>Filtered angle</th><td id="t-angle">—</td></tr>
          <tr><th>Gyroscope rate</th><td id="t-gyro">—</td></tr>
          <tr><th>MPU status</th><td id="t-mpu">—</td></tr>
          <tr><th>System</th><td id="t-system">—</td></tr>
        </tbody>
      </table>

      <table class="telemetry-table motor-table">
        <thead>
          <tr><th>Motor data</th><th>LEFT</th><th>RIGHT</th></tr>
        </thead>
        <tbody>
          <tr><th>RPM</th><td id="t-left-rpm">—</td><td id="t-right-rpm">—</td></tr>
          <tr><th>Direction</th><td id="t-left-dir">—</td><td id="t-right-dir">—</td></tr>
          <tr><th>PWM</th><td id="t-left-pwm">—</td><td id="t-right-pwm">—</td></tr>
          <tr><th>Encoder count</th><td id="t-left-count">—</td><td id="t-right-count">—</td></tr>
          <tr><th>Auto trim</th><td id="t-left-trim">—</td><td id="t-right-trim">—</td></tr>
        </tbody>
      </table>

      <table class="telemetry-table status-table">
        <thead><tr><th colspan="2">SYNC / CONTROL</th></tr></thead>
        <tbody>
          <tr><th>RPM difference</th><td id="t-rpm-diff">—</td></tr>
          <tr><th>Velocity feedback</th><td id="t-velocity-feedback">—</td></tr>
          <tr><th>Position feedback</th><td id="t-position-feedback">—</td></tr>
          <tr><th>PID output</th><td id="t-pid">—</td></tr>
        </tbody>
      </table>
    </div>
  </section>

</div>

<script>
/* ==================================================================
   BALANCE ROBOT TELEMETRY DASHBOARD
   ------------------------------------------------------------------
   Fully self-contained, offline diagnostics UI for an ESP32
   self-balancing robot's PID controller. No external requests of
   any kind are made — every chart is hand-drawn on <canvas> and all
   logic below is vanilla JS, so this file works served directly
   from the robot's own WiFi access point with zero internet access.

   Public entry point for real/live telemetry:

       updateTelemetry({ pitch, setpoint, error, kp, ki, kd,
                          pTerm, iTerm, dTerm })

  Everything else in this file (charts and metrics)
   is driven purely off the rolling sample buffer that function
   fills. See "ESP32 INTEGRATION" below for how this file currently
   talks to the robot and where a future WebSocket should be wired
   in.
   ================================================================== */

// ---- tunables ------------------------------------------------------
const MAX_SAMPLES             = 1000;  // rolling history kept for every channel
const RENDER_FPS              = 30;    // chart/metrics redraw cap
const RENDER_INTERVAL_MS      = 1000 / RENDER_FPS;
const LIVE_TIMEOUT_MS         = 2500;  // clear the graph after this long without robot data
const WS_RECONNECT_DELAY_MS   = 1000;  // backoff before retrying a dropped socket
const STALE_CHECK_INTERVAL_MS = 250;   // watchdog cadence for "socket open, robot silent"

// Mirrors the CSS custom properties in :root — see the note at the
// top of the <style> block. Canvas drawing re-reads these every
// frame, so plain JS constants are far cheaper than getComputedStyle().
const COLORS = {
  panel2: '#111c2e', border: '#243550', borderSoft: '#182437',
  text: '#e7edf7', textDim: '#8ea0bb', textFaint: '#5b6d87',
  pitch: '#4ade80', setpoint: '#f8fafc', error: '#facc15',
  pTerm: '#ef4444', iTerm: '#4ade80', dTerm: '#3b82f6',
};

function numOr(v, fallback){ return (typeof v === 'number' && isFinite(v)) ? v : fallback; }
function setText(id, text){ const el = document.getElementById(id); if (el) el.textContent = text; }

// ====================================================================
// SAMPLE STORE — fixed-capacity circular buffer, one Float64Array per
// channel. Pushing a sample is O(1); building a render-ready ordered
// snapshot is O(capacity) but only happens once per rendered frame
// (max 30 times/sec), which is trivial at this size. Typed arrays keep
// memory flat regardless of how long the dashboard has been running.
// ====================================================================
class SampleStore {
  constructor(capacity){
    this.capacity = capacity;
    this.fields = ['t', 'pitch', 'accel', 'gyro', 'setpoint', 'error', 'pTerm', 'iTerm', 'dTerm', 'pid', 'kp', 'ki', 'kd', 'leftRPM', 'rightRPM', 'rpmDiff', 'velocityFeedback', 'positionFeedback', 'leftPWM', 'rightPWM', 'leftDir', 'rightDir', 'leftCount', 'rightCount', 'leftTrim', 'rightTrim', 'alpha', 'sync', 'leftMin', 'rightMin', 'cpr', 'mpu', 'system'];
    this.buffers = {};
    this.fields.forEach(f => {
      this.buffers[f] = (f === 'leftDir' || f === 'rightDir') ? new Array(capacity).fill('') : new Float64Array(capacity);
    });
    this.writeIndex = 0;
    this.count = 0;
  }
  push(sample){
    for (const f of this.fields) this.buffers[f][this.writeIndex] = sample[f] ?? 0;
    this.writeIndex = (this.writeIndex + 1) % this.capacity;
    if (this.count < this.capacity) this.count++;
  }
  clear(){ this.writeIndex = 0; this.count = 0; }
  // Oldest -> newest plain-array snapshot, ready for charting and metrics.
  snapshot(){
    const n = this.count;
    const start = (this.count < this.capacity) ? 0 : this.writeIndex;
    const out = {};
    for (const f of this.fields){
      const src = this.buffers[f];
      const arr = new Array(n);
      for (let i = 0; i < n; i++) arr[i] = src[(start + i) % this.capacity];
      out[f] = arr;
    }
    out.count = n;
    return out;
  }
}

const store = new SampleStore(MAX_SAMPLES);
let sampleVersion = 0; // bumped on every push(); lets the render loop skip idle frames

// ====================================================================
// TELEMETRY INGEST
// ====================================================================
// Accepts either the documented wire format:
//   { pitch, setpoint, error, kp, ki, kd, pTerm, iTerm, dTerm }
// ...or this robot's existing firmware field names (angle instead of
// pitch, pOutput/iOutput/dOutput instead of pTerm/iTerm/dTerm), so the
// same dashboard drops straight onto the /data endpoint already being
// served without touching the firmware. Whatever "error" value is
// supplied is trusted as-is rather than recomputed, since sign
// convention (setpoint-pitch vs pitch-setpoint) can vary by firmware.
function normalizeTelemetry(raw){
  const pitch    = numOr(raw.pitch, numOr(raw.angle, 0));
  const setpoint = numOr(raw.setpoint, 0);
  const error    = (typeof raw.error === 'number' && isFinite(raw.error)) ? raw.error : (setpoint - pitch);
  return {
    pitch, accel: numOr(raw.accel, 0), gyro: numOr(raw.gyro, 0), setpoint, error,
    kp: numOr(raw.kp, 0), ki: numOr(raw.ki, 0), kd: numOr(raw.kd, 0),
    pTerm: numOr(raw.pTerm, numOr(raw.pOutput, 0)),
    iTerm: numOr(raw.iTerm, numOr(raw.iOutput, 0)),
    dTerm: numOr(raw.dTerm, numOr(raw.dOutput, 0)),
    pid: numOr(raw.pid, 0), rpmDiff: numOr(raw.rpmDiff, 0),
    velocityFeedback: numOr(raw.velocityFeedback, 0),
    positionFeedback: numOr(raw.positionFeedback, 0),
    leftRPM: numOr(raw.leftRPM, 0), rightRPM: numOr(raw.rightRPM, 0),
    leftPWM: numOr(raw.leftPWM, 0), rightPWM: numOr(raw.rightPWM, 0),
    leftDir: raw.leftDir || '', rightDir: raw.rightDir || '',
    leftCount: numOr(raw.leftCount, 0), rightCount: numOr(raw.rightCount, 0),
    leftTrim: numOr(raw.leftTrim, 0), rightTrim: numOr(raw.rightTrim, 0),
    alpha: numOr(raw.alpha, 0), sync: numOr(raw.sync, 0),
    leftMin: numOr(raw.leftMin, 0), rightMin: numOr(raw.rightMin, 0),
    cpr: numOr(raw.cpr, 0), system: raw.system === true ? 1 : 0,
    mpu: raw.mpu === true ? 1 : 0,
  };
}

let lastArrivalTime = null;  // performance.now() of the previous sample, any source
let loopRateEMA     = null;  // smoothed Hz of incoming telemetry (measured, not assumed)
let lastLiveDataTime = 0;    // performance.now() of the last *real* ESP32 sample

// The one function every integration path below calls.
// source identifies the telemetry transport; the dashboard accepts live data.
function updateTelemetry(data, source){
  source = source || 'live';
  if (source !== 'live') return;
  const now = performance.now();
  if (lastArrivalTime !== null){
    const dt = (now - lastArrivalTime) / 1000;
    if (dt > 0.0005){
      const instantHz = 1 / dt;
      loopRateEMA = (loopRateEMA === null) ? instantHz : (loopRateEMA * 0.9 + instantHz * 0.1);
    }
  }
  lastArrivalTime = now;
  lastLiveDataTime = now;

  store.push({ t: now, ...normalizeTelemetry(data) });
  sampleVersion++;
}
window.updateTelemetry = updateTelemetry;

// ====================================================================
// ESP32 INTEGRATION — WebSocket push telemetry
// ====================================================================
// Firmware pushes one JSON sample over ws://<host>:81/ at up to 20 Hz
// (WS_BROADCAST_INTERVAL_MS in web_interface.cpp). GET /setPID stays on HTTP,
// since PID edits are occasional human-triggered writes, not a
// push-worthy stream.
let esp32Reachable = false;
let pidInputsInitialized = false;
// Declared here (not down in CONTROLS below) because connectWebSocket()
// reads it on the very first message, which happens before the script
// reaches the CONTROLS section.
let paused = false;
let ws = null;

function setPIDStatus(text, state){
  const status = document.getElementById('pid-status');
  status.textContent = text;
  status.className = 'pid-status' + (state ? ' ' + state : '');
}

function initializePIDInputs(raw){
  if (pidInputsInitialized) return;
  const values = { kp: raw.kp, ki: raw.ki, kd: raw.kd, setpoint: raw.setpoint, positionhold: raw.positionHoldGain };
  for (const parameter of Object.keys(values)){
    if (typeof values[parameter] !== 'number' || !isFinite(values[parameter])) return;
    document.getElementById('pid-' + parameter).value = values[parameter];
  }
  pidInputsInitialized = true;
  setPIDStatus('Live values loaded');
}

function renderHardwareTelemetry(raw){
  const numberValue = (value, digits = 2) => Number.isFinite(Number(value)) ? Number(value).toFixed(digits) : '—';
  setText('t-accel', numberValue(raw.accel) + '°');
  setText('t-angle', numberValue(raw.angle) + '°');
  setText('t-gyro', numberValue(raw.gyro) + '°/s');
  setText('t-left-rpm', numberValue(raw.leftRPM));
  setText('t-left-dir', raw.leftDir || '—');
  setText('t-left-pwm', numberValue(raw.leftPWM, 0));
  setText('t-left-count', numberValue(raw.leftCount, 0));
  setText('t-left-trim', numberValue(raw.leftTrim));
  setText('t-right-rpm', numberValue(raw.rightRPM));
  setText('t-right-dir', raw.rightDir || '—');
  setText('t-right-pwm', numberValue(raw.rightPWM, 0));
  setText('t-right-count', numberValue(raw.rightCount, 0));
  setText('t-right-trim', numberValue(raw.rightTrim));
  setText('t-rpm-diff', numberValue(raw.rpmDiff));
  setText('t-velocity-feedback', numberValue(raw.velocityFeedback));
  setText('t-position-feedback', numberValue(raw.positionFeedback));
  setText('t-pid', numberValue(raw.pid));
  setText('t-mpu', raw.mpu ? 'OK' : 'FAULT');
  setText('t-system', raw.system ? 'ACTIVE' : 'STOPPED');
}

function connectWebSocket(){
  ws = new WebSocket(`ws://${location.hostname}:81/`);

  ws.onopen = () => {
    esp32Reachable = true;
  };

  ws.onmessage = (event) => {
    let raw;
    try { raw = JSON.parse(event.data); } catch (err) { return; } // ignore malformed frames
    esp32Reachable = true;
    // Keep the connection indicator honest even while paused -- it
    // reports "is the ESP32 still there", not "is data flowing into
    // the charts", and those are different things once paused.
    lastLiveDataTime = performance.now();
    initializePIDInputs(raw);
    renderHardwareTelemetry(raw);
    // Paused = freeze data collection, not just the redraw, so
    // whatever is on screen when you hit PAUSE is exactly what
    // EXPORT CSV writes out -- no extra samples sneak in behind it.
    if (!paused) updateTelemetry(raw, 'live');
  };

  ws.onclose = () => {
    esp32Reachable = false;
    ws = null;
    setTimeout(connectWebSocket, WS_RECONNECT_DELAY_MS);
  };

  ws.onerror = () => {
    // A WebSocket always fires onclose right after onerror, so the
    // reconnect above is already scheduled there -- just force the
    // close now instead of waiting out the browser's own timeout.
    ws.close();
  };
}
connectWebSocket();

// Safety net for "socket technically open but no sample has actually
// arrived in a while" -- robot rebooting, WiFi at the edge of AP
// range, firmware stalled, etc. Mirrors the old HTTP poller's failure
// behavior: clear the graph rather than let it silently go stale.
setInterval(() => {
  if (lastLiveDataTime !== 0 && performance.now() - lastLiveDataTime >= LIVE_TIMEOUT_MS && store.count > 0){
    store.clear();
    loopRateEMA = null;
    sampleVersion++;
  }
}, STALE_CHECK_INTERVAL_MS);

// ====================================================================
// CHART RENDERING
// ====================================================================
// One reusable canvas line-chart class shared by all three graphs.
//
// AUTO-SCALING
//   mode:'shared' — Y bounds come from the min/max across *all* series
//                    in the chart (Graph 1 and Graph 3: every line has
//                    to stay on-screen together).
//   mode:'single' — Y bounds come from only the first series, with an
//                    enforced minimum span (minSpan) so a near-flat
//                    signal still fills the chart instead of looking
//                    like a straight line (Graph 2's whole purpose is
//                    zooming in on small balancing movements).
// Both modes add padPercent of headroom above/below so traces never
// touch the very top/bottom edge of the plot.
class LineChart {
  constructor(canvas, opts){
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.series = opts.series;
    this.yLabel = opts.yLabel || '';
    this.mode = opts.mode || 'shared';
    this.minSpan = opts.minSpan || 0;
    this.padPercent = (opts.padPercent != null) ? opts.padPercent : 0.15;
  }

  _resize(){
    const rect = this.canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(rect.width * dpr));
    const h = Math.max(1, Math.round(rect.height * dpr));
    if (this.canvas.width !== w || this.canvas.height !== h){
      this.canvas.width = w;
      this.canvas.height = h;
    }
    this.cssWidth = rect.width;
    this.cssHeight = rect.height;
    this.dpr = dpr;
  }

  render(snapshot){
    this._resize(); // cheap no-op unless the container actually changed size
    const ctx = this.ctx, W = this.cssWidth, H = this.cssHeight;
    ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = COLORS.panel2;
    ctx.fillRect(0, 0, W, H);

    const n = snapshot.count;
    const pad = { left: 54, right: 14, top: 12, bottom: 40 }; // extra bottom room: x-axis ticks + legend get their own rows
    const plotW = Math.max(1, W - pad.left - pad.right);
    const plotH = Math.max(1, H - pad.top - pad.bottom);

    if (n < 2){
      ctx.fillStyle = COLORS.textFaint;
      ctx.font = '12px ' + getComputedStyle(document.body).fontFamily;
      ctx.fillText('Waiting for telemetry…', pad.left, pad.top + plotH / 2);
      return;
    }

    const seriesData = this.series.map(s => ({ ...s, data: snapshot[s.key] }));

    // ---- auto-scale Y ----
    const values = (this.mode === 'single') ? seriesData[0].data : [].concat(...seriesData.map(s => s.data));
    let minV = Math.min(...values), maxV = Math.max(...values);
    if (!isFinite(minV) || !isFinite(maxV)){ minV = -1; maxV = 1; }
    let span = maxV - minV;
    if (span < this.minSpan){
      const mid = (maxV + minV) / 2;
      minV = mid - this.minSpan / 2; maxV = mid + this.minSpan / 2; span = this.minSpan;
    }
    const headroom = (span * this.padPercent) || 0.5;
    minV -= headroom; maxV += headroom;
    if (maxV <= minV) maxV = minV + 1;

    const xFor = i => pad.left + (i / (n - 1)) * plotW;
    const yFor = v => pad.top + (1 - (v - minV) / (maxV - minV)) * plotH;

    // ---- grid + Y axis labels ----
    ctx.lineWidth = 1;
    ctx.font = '10px ' + getComputedStyle(document.body).fontFamily;
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    const yTicks = 4;
    for (let i = 0; i <= yTicks; i++){
      const v = minV + (maxV - minV) * i / yTicks;
      const y = yFor(v);
      ctx.strokeStyle = COLORS.borderSoft;
      ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(W - pad.right, y); ctx.stroke();
      ctx.fillStyle = COLORS.textFaint;
      ctx.fillText(v.toFixed(2), pad.left - 8, y);
    }

    // ---- X axis (samples-ago, 0 = most recent) ----
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    const xTicks = 6;
    for (let i = 0; i <= xTicks; i++){
      const idx = Math.round(i * (n - 1) / xTicks);
      const x = xFor(idx);
      ctx.strokeStyle = COLORS.borderSoft;
      ctx.beginPath(); ctx.moveTo(x, pad.top); ctx.lineTo(x, H - pad.bottom); ctx.stroke();
      ctx.fillStyle = COLORS.textFaint;
      ctx.fillText('-' + (n - 1 - idx), x, H - pad.bottom + 8);
    }

    // ---- zero line, when in range ----
    if (minV < 0 && maxV > 0){
      ctx.strokeStyle = COLORS.border;
      ctx.lineWidth = 1.4;
      const y0 = yFor(0);
      ctx.beginPath(); ctx.moveTo(pad.left, y0); ctx.lineTo(W - pad.right, y0); ctx.stroke();
      ctx.lineWidth = 1;
    }

    // ---- series traces ----
    // Thin (1px) strokes so small-amplitude signals -- a few tenths
    // of a degree of pitch, or a small P/I/D contribution -- don't
    // get visually swallowed by a thick line. That matters here
    // specifically because this chart is what you eyeball to decide
    // whether Kp/Ki/Kd needs adjusting.
    seriesData.forEach(s => {
      ctx.strokeStyle = s.color;
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let i = 0; i < n; i++){
        const x = xFor(i), y = yFor(s.data[i]);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
    });

    // ---- Y axis unit label ----
    if (this.yLabel){
      ctx.fillStyle = COLORS.textDim;
      ctx.textAlign = 'left';
      ctx.textBaseline = 'alphabetic';
      ctx.fillText(this.yLabel, 4, 12);
    }

    // ---- legend ----
    let lx = pad.left;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    seriesData.forEach(s => {
      ctx.fillStyle = s.color;
      ctx.fillRect(lx, H - 10, 10, 3);
      ctx.fillStyle = COLORS.textDim;
      ctx.fillText(s.label, lx + 14, H - 8.5);
      lx += 14 + ctx.measureText(s.label).width + 18;
    });
  }
}

const chartPitch = new LineChart(document.getElementById('chart-pitch'), {
  series: [
    { key: 'pitch',    label: 'Pitch',    color: COLORS.pitch },
    { key: 'setpoint', label: 'Setpoint', color: COLORS.setpoint },
    { key: 'error',    label: 'Error',    color: COLORS.error },
  ],
  yLabel: 'deg', mode: 'shared', padPercent: 0.15,
});

const chartDetail = new LineChart(document.getElementById('chart-detail'), {
  series: [{ key: 'pitch', label: 'Pitch Angle', color: COLORS.pitch }],
  yLabel: 'deg (zoomed)', mode: 'single', minSpan: 0.6, padPercent: 0.25,
});

const chartPID = new LineChart(document.getElementById('chart-pid'), {
  series: [
    { key: 'pTerm', label: 'P term', color: COLORS.pTerm },
    { key: 'iTerm', label: 'I term', color: COLORS.iTerm },
    { key: 'dTerm', label: 'D term', color: COLORS.dTerm },
  ],
  yLabel: 'output', mode: 'shared', padPercent: 0.15,
});

// ====================================================================
// METRICS
// ====================================================================
// Peak overshoot = largest |error| reached during any completed
// half-cycle (from one zero-crossing to the next) in the buffer — the
// standard step-response definition, without needing an explicit
// "step was commanded here" marker.
function computeOvershoot(errors){
  let best = 0, current = 0, sign = Math.sign(errors[0]) || 1;
  for (let i = 0; i < errors.length; i++){
    const e = errors[i], s = Math.sign(e);
    if (s !== 0 && s !== sign){ best = Math.max(best, current); current = 0; sign = s; }
    current = Math.max(current, Math.abs(e));
  }
  return Math.max(best, current);
}

function computeMetrics(snap){
  const n = snap.count;
  if (n === 0) return null;
  const errors = snap.error;

  let sumSq = 0, sum = 0, maxAbs = 0;
  for (let i = 0; i < n; i++){
    const e = errors[i];
    sumSq += e * e; sum += e;
    if (Math.abs(e) > maxAbs) maxAbs = Math.abs(e);
  }
  const rms = Math.sqrt(sumSq / n);
  const avg = sum / n;
  const overshoot = computeOvershoot(errors);

  // Oscillation frequency: zero-crossings of error over the visible
  // window, converted to Hz using real sample timestamps (not an
  // assumed sample rate).
  let crossings = 0, prevSign = Math.sign(errors[0]) || 1;
  for (let i = 1; i < n; i++){
    const s = Math.sign(errors[i]);
    if (s !== 0 && s !== prevSign){ crossings++; prevSign = s; }
  }
  const durationSec = (snap.t[n - 1] - snap.t[0]) / 1000;
  const oscFreq = durationSec > 0 ? (crossings / 2) / durationSec : 0;

  return {
    n, pitch: snap.pitch[n - 1], error: errors[n - 1],
    rms, avg, max: maxAbs, overshoot, oscFreq,
    loopRate: loopRateEMA || 0,
    kp: snap.kp[n - 1], ki: snap.ki[n - 1], kd: snap.kd[n - 1],
    leftRPM: snap.leftRPM[n - 1], rightRPM: snap.rightRPM[n - 1],
    leftPWM: snap.leftPWM[n - 1], rightPWM: snap.rightPWM[n - 1],
    mpu: snap.mpu[n - 1] === 1,
  };
}

function renderMetrics(m){
  setText('sample-count', (m ? m.n : 0) + ' samples');
  if (!m){
    ['m-pitch','m-error','m-rms','m-avg','m-max','m-overshoot','m-oscfreq','m-looprate',
     'm-kp','m-ki','m-kd','m-left-rpm','m-right-rpm','m-left-pwm','m-right-pwm','m-mpu']
      .forEach(id => setText(id, '—'));
    return;
  }
  setText('m-pitch', m.pitch.toFixed(2) + '°');
  setText('m-error', m.error.toFixed(2) + '°');
  setText('m-rms', m.rms.toFixed(2) + '°');
  setText('m-avg', m.avg.toFixed(2) + '°');
  setText('m-max', m.max.toFixed(2) + '°');
  setText('m-overshoot', m.overshoot.toFixed(2) + '°');
  setText('m-oscfreq', m.oscFreq.toFixed(2) + ' Hz');
  setText('m-looprate', m.loopRate.toFixed(1) + ' Hz');
  setText('m-kp', m.kp.toFixed(2));
  setText('m-ki', m.ki.toFixed(2));
  setText('m-kd', m.kd.toFixed(2));
  setText('m-left-rpm', m.leftRPM.toFixed(1));
  setText('m-right-rpm', m.rightRPM.toFixed(1));
  setText('m-left-pwm', Math.round(m.leftPWM));
  setText('m-right-pwm', Math.round(m.rightPWM));
  setText('m-mpu', m.mpu ? 'OK' : 'FAULT');
}

// ====================================================================
// ====================================================================
// CONNECTION STATUS
// ====================================================================
function renderConnectionStatus(){
  const dot = document.getElementById('conn-dot');
  const label = document.getElementById('conn-label');
  const now = performance.now();
  const liveFresh = lastLiveDataTime !== 0 && (now - lastLiveDataTime) < LIVE_TIMEOUT_MS;
  if (liveFresh){
    dot.className = 'dot dot-live'; label.textContent = 'Live ESP32 data';
  } else {
    dot.className = 'dot dot-wait'; label.textContent = 'Initializing…';
  }
}

// ====================================================================
// CONTROLS
// ====================================================================
document.getElementById('btn-start').addEventListener('click', () => {
  paused = false;
});
document.getElementById('btn-pause').addEventListener('click', () => {
  paused = true;
});
document.getElementById('btn-reset').addEventListener('click', () => {
  store.clear();
  loopRateEMA = null;
  sampleVersion++;
  lastRenderedVersion = sampleVersion;
  const emptySnapshot = store.snapshot();
  chartPitch.render(emptySnapshot);
  chartDetail.render(emptySnapshot);
  chartPID.render(emptySnapshot);
  renderMetrics(null);
});

// Dumps every buffered sample (up to MAX_SAMPLES = 1000, oldest
// first) as a CSV file. Workflow: hit PAUSE so the buffer stops
// changing, EXPORT CSV, then upload that file straight into a chat
// with Claude -- the columns line up with computeMetrics() above, so
// it's enough to go over the step response and suggest new Kp/Ki/Kd.
function exportCSV(){
  const snap = store.snapshot();
  if (snap.count === 0) return;

  const fields = ['t', 'pitch', 'setpoint', 'error', 'pTerm', 'iTerm', 'dTerm', 'kp', 'ki', 'kd'];
  const rows = new Array(snap.count + 1);
  rows[0] = fields.join(',');
  for (let i = 0; i < snap.count; i++){
    rows[i + 1] = fields.map(f => snap[f][i]).join(',');
  }

  const blob = new Blob([rows.join('\n')], { type: 'text/csv' });
  const url = URL.createObjectURL(blob);
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');
  const a = document.createElement('a');
  a.href = url;
  a.download = `balance-robot-telemetry-${stamp}.csv`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
document.getElementById('btn-export').addEventListener('click', exportCSV);

function exportFullCSV(){
  const snap = store.snapshot();
  if (snap.count === 0) return;

  const rows = new Array(snap.count + 1);
  rows[0] = store.fields.join(',');
  for (let i = 0; i < snap.count; i++){
    rows[i + 1] = store.fields.map(field => {
      const value = snap[field][i];
      return typeof value === 'string' ? '"' + value.replace(/"/g, '""') + '"' : value;
    }).join(',');
  }

  const blob = new Blob([rows.join('\n')], { type: 'text/csv' });
  const url = URL.createObjectURL(blob);
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');
  const link = document.createElement('a');
  link.href = url;
  link.download = `balance-robot-full-telemetry-${stamp}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}
document.getElementById('btn-export-full').addEventListener('click', exportFullCSV);

async function sendPIDField(parameter, input){
  const value = Number(input.value);
  if (!Number.isFinite(value) || !input.checkValidity()){
    input.reportValidity();
    setPIDStatus('Enter a valid value', 'error');
    return;
  }
  setPIDStatus('Sending value…');
  try{
    const query = new URLSearchParams({ parameter, value: value.toFixed(5) });
    const response = await fetch('/setPID?' + query.toString(), { cache: 'no-store' });
    if (!response.ok) throw new Error('HTTP ' + response.status);
    setPIDStatus('Value applied to robot', 'success');
  } catch (err){
    setPIDStatus('Could not apply value', 'error');
  }
}

function adjustPIDField(parameter, input, direction){
  const current = Number(input.value);
  if (!Number.isFinite(current)) return;
  const next = Math.max(Number(input.min), Math.min(Number(input.max), current + direction * 0.5));
  input.value = next.toFixed(5);
  sendPIDField(parameter, input);
}

['kp', 'ki', 'kd', 'setpoint', 'positionhold'].forEach(parameter => {
  const input = document.getElementById('pid-' + parameter);
  input.addEventListener('change', () => sendPIDField(parameter, input));
  input.addEventListener('keydown', event => {
    if (event.key === 'ArrowUp' || event.key === 'ArrowDown'){
      event.preventDefault();
      adjustPIDField(parameter, input, event.key === 'ArrowUp' ? 1 : -1);
      return;
    }
    if (event.key === 'Enter'){
      event.preventDefault();
      input.blur();
    }
  });
});

// ====================================================================
// RENDER LOOP — capped at RENDER_FPS via requestAnimationFrame, and
// skipped entirely on frames where no new sample has arrived, so the
// dashboard never redraws work nobody will see (also lets the browser
// throttle everything automatically when the tab is backgrounded).
// ====================================================================
let lastRenderTime = 0;
let lastRenderedVersion = -1;

function renderLoop(ts){
  requestAnimationFrame(renderLoop);
  if (ts - lastRenderTime < RENDER_INTERVAL_MS) return;
  lastRenderTime = ts;
  if (paused) return;
  if (sampleVersion === lastRenderedVersion) { renderConnectionStatus(); return; }
  lastRenderedVersion = sampleVersion;

  const snap = store.snapshot();
  chartPitch.render(snap);
  chartDetail.render(snap);
  chartPID.render(snap);

  const metrics = computeMetrics(snap);
  renderMetrics(metrics);
  renderConnectionStatus();
}
requestAnimationFrame(renderLoop);
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ============================================================
// WEB PID CONTROL
// ============================================================

static void handleSetPID()
{
  if (!server.hasArg("parameter"))
  {
    server.send(400, "application/json", "{\"error\":\"Missing parameter\"}");
    return;
  }

  String parameter = server.arg("parameter");

  float *selectedValue = nullptr;
  float step = 0.5f;
  float minValue = -1000.0f;
  float maxValue = 1000.0f;

  // Each gain gets its own step size, so one arrow-key press or
  // one button click moves Kp, Ki, and Kd by different amounts.
  if (parameter == "kp")
  {
    selectedValue = &Kp;
    step = 0.5f;
  }
  else if (parameter == "ki")
  {
    selectedValue = &Ki;
    step = 0.5f;
  }
  else if (parameter == "kd")
  {
    selectedValue = &Kd;
    step = 0.5f;
  }
  else if (parameter == "setpoint")
  {
    // Balance point rarely needs to move far from 0 -- COG/MPU
    // mounting offsets are usually a few degrees, not tens of
    // them. Capped well inside the +-35 degree safety cutoff in
    // controlMotors() so the two never contradict each other.
    selectedValue = &balanceSetpoint;
    step = 0.5f;
    minValue = -20.0f;
    maxValue = 20.0f;
  }
  else if (parameter == "positionhold")
  {
    // Position hold gain (0.0 = off, 0.15+ = active drift correction).
    // Prevents linear travel while maintaining balance stability.
    selectedValue = &positionHoldGain;
    step = 0.05f;
    minValue = 0.0f;
    maxValue = 1.0f;
  }
  else
  {
    server.send(400, "application/json", "{\"error\":\"Invalid parameter\"}");
    return;
  }

  if (server.hasArg("value"))
  {
    // Absolute set -- typed directly into the box, Enter to apply.
    float value = server.arg("value").toFloat();
    if (value < minValue)
      value = minValue;
    if (value > maxValue)
      value = maxValue;
    *selectedValue = roundf(value * 100000.0f) / 100000.0f;
  }
  else if (server.hasArg("change"))
  {
    // Relative nudge -- +/- buttons or arrow keys. Only the SIGN
    // of "change" matters here; the step size above is what
    // actually decides how far each parameter moves.
    float requested = server.arg("change").toFloat();
    float sign = (requested >= 0.0f) ? 1.0f : -1.0f;

    *selectedValue += sign * step;
    if (*selectedValue < minValue)
      *selectedValue = minValue;
    if (*selectedValue > maxValue)
      *selectedValue = maxValue;
    *selectedValue = roundf(*selectedValue * 100000.0f) / 100000.0f;
  }
  else
  {
    server.send(400, "application/json", "{\"error\":\"Missing value or change\"}");
    return;
  }

  String response = "{\"parameter\":\"" + parameter + "\",\"value\":" + String(*selectedValue, 5) + "}";
  server.send(200, "application/json", response);
}

// ============================================================
// WEB LIVE DATA
// ============================================================

// Single source of truth for the telemetry wire format. Both the HTTP
// GET /data handler and the WebSocket broadcaster below call this, so
// the two transports can never drift out of sync with each other.
static String buildTelemetryJSON()
{
  long leftCount, rightCount;
  noInterrupts();
  leftCount = leftEncoderCount;
  rightCount = rightEncoderCount;
  interrupts();

  // Emit the dashboard's canonical field names as well as the older
  // aliases consumed by existing tools.
  String json = "{";
  json += "\"timestamp\":" + String(millis());
  json += ",\"pitch\":" + String(filteredAngle, 2);
  json += ",\"pTerm\":" + String(pidProportional, 2);
  json += ",\"iTerm\":" + String(pidIntegralOutput, 2);
  json += ",\"dTerm\":" + String(pidDerivativeOutput, 2);
  json += ",\"accel\":" + String(accelAngle, 2);
  json += ",\"angle\":" + String(filteredAngle, 2);
  json += ",\"gyro\":" + String(gyroRate, 2);
  json += ",\"leftRPM\":" + String(leftRPMFiltered, 2);
  json += ",\"rightRPM\":" + String(rightRPMFiltered, 2);
  json += ",\"rpmDiff\":" + String(rpmDifference, 2);
  json += ",\"velocityFeedback\":" + String(velocityFeedback, 2);
  json += ",\"positionFeedback\":" + String(positionFeedback, 2);
  json += ",\"leftPWM\":" + String(leftPWM);
  json += ",\"rightPWM\":" + String(rightPWM);
  json += ",\"leftDir\":\"" + leftDirection + "\"";
  json += ",\"rightDir\":\"" + rightDirection + "\"";
  json += ",\"leftCount\":" + String(leftCount);
  json += ",\"rightCount\":" + String(rightCount);
  json += ",\"leftTrim\":" + String(leftMotorTrim, 2);
  json += ",\"rightTrim\":" + String(rightMotorTrim, 2);
  json += ",\"kp\":" + String(Kp, 5);
  json += ",\"ki\":" + String(Ki, 5);
  json += ",\"kd\":" + String(Kd, 5);
  json += ",\"setpoint\":" + String(balanceSetpoint, 5);
  json += ",\"positionHoldGain\":" + String(positionHoldGain, 5);
  json += ",\"error\":" + String(filteredAngle - balanceSetpoint, 2);
  json += ",\"alpha\":" + String(filterAlpha, 2);
  json += ",\"sync\":" + String(syncGain, 2);
  json += ",\"leftMin\":" + String(leftMinPWM);
  json += ",\"rightMin\":" + String(rightMinPWM);
  json += ",\"cpr\":" + String(COUNTS_PER_REV, 0);
  json += ",\"pid\":" + String(pidOutput, 2);
  json += ",\"pOutput\":" + String(pidProportional, 2);
  json += ",\"iOutput\":" + String(pidIntegralOutput, 2);
  json += ",\"dOutput\":" + String(pidDerivativeOutput, 2);
  json += ",\"mpu\":" + String(mpuOK ? "true" : "false");
  json += ",\"system\":" + String(systemEnabled ? "true" : "false");
  json += "}";

  return json;
}

static void handleData()
{
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "application/json", buildTelemetryJSON());
}

// ============================================================
// WEBSOCKET TELEMETRY (push, up to 20 Hz)
// ============================================================
// Read-only from the browser's side -- no inbound commands are
// accepted over this socket. PID edits still go through the existing
// HTTP /setPID endpoint below, since those are occasional,
// human-triggered writes rather than a push-worthy stream.
static void onWsEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.printf("[WS] client #%u connected\n", clientId);
    {
      // Send one sample immediately so a freshly-opened
      // dashboard doesn't sit blank for up to
      // WS_BROADCAST_INTERVAL_MS waiting for the next tick.
      String json = buildTelemetryJSON();
      wsServer.sendTXT(clientId, json.c_str(), json.length());
    }
    break;
  case WStype_DISCONNECTED:
    Serial.printf("[WS] client #%u disconnected\n", clientId);
    break;
  default:
    break; // WStype_TEXT/BIN etc. -- nothing inbound to handle
  }
}

static void broadcastTelemetryWS()
{
  uint32_t now = millis();
  if (now - lastWsBroadcastMs < WS_BROADCAST_INTERVAL_MS)
    return;
  lastWsBroadcastMs = now;

  if (wsServer.connectedClients() == 0)
    return; // nobody listening -- skip building the JSON at all

  String json = buildTelemetryJSON();
  wsServer.broadcastTXT(json.c_str(), json.length());
}

// ============================================================
// PUBLIC API
// ============================================================

void initWebInterface()
{
  Serial.println("Starting WiFi AP...");
  WiFi.mode(WIFI_AP);
  delay(200);

  bool wifiStarted = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  if (wifiStarted)
  {
    Serial.println("WiFi AP started.");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("Password: ");
    Serial.println(WIFI_PASSWORD);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("WiFi AP FAILED!");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/setPID", handleSetPID);
  server.begin();

  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  // Arduino's loop task normally runs on core 1. Keep HTTP/WebSocket
  // work on core 0 so it cannot delay the balance-control loop.
  xTaskCreatePinnedToCore(
      webInterfaceTask,
      "webInterface",
      8192,
      nullptr,
      1,
      nullptr,
      0);

  Serial.println("Web server started.");
  Serial.println("========================================");
  Serial.print("Connect WiFi: ");
  Serial.println(WIFI_SSID);
  Serial.print("Open browser: http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Telemetry WS: ws://");
  Serial.print(WiFi.softAPIP());
  Serial.println(":81/");
  Serial.println("========================================");
}

static void webInterfaceTask(void *parameter)
{
  (void)parameter;

  for (;;)
  {
    server.handleClient();
    wsServer.loop();
    broadcastTelemetryWS();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void webInterfaceLoop()
{
  // HTTP and WebSocket servicing runs in webInterfaceTask().
}