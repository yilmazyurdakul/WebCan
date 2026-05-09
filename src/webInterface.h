#pragma once

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"/>
<title>WebCan Terminal</title>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<style>
#historyModal tbody tr:hover { background:#0c1524; }

  :root{ --bg:#0b0f14; --panel:#111827; --ink:#e6edf3; --muted:#aab3c2; --border:#334155; --accent:#3b82f6; --accent2:#22c55e; --danger:#ef4444; --rowEven:#0e1420; --rowOdd:#0b101a; --head:#0f172a }
  *{box-sizing:border-box} html,body{height:100%}
  body{display:flex;min-height:100vh;flex-direction:column;font-family:ui-monospace,Menlo,Consolas,monospace;background:var(--bg);color:var(--ink);margin:0}
  header{flex:none;padding:10px 14px;background:var(--panel);display:flex;gap:10px;align-items:center;flex-wrap:wrap;border-bottom:1px solid var(--border)}
  #status{font-size:12px;opacity:.85}
  .wrap{flex:1;min-height:0;display:grid;grid-template-columns:280px 1fr}
  @media (max-width:880px){ .wrap{grid-template-columns:1fr} #sidebar{order:2} }
  #sidebar{border-right:1px solid var(--border); background:#0f172a; min-height:0; display:flex; flex-direction:column}
  #sidetop{padding:10px 12px; border-bottom:1px solid var(--border); display:flex; flex-direction:column; gap:8px}
  #idfilter{width:100%; padding:8px 10px; background:#0b1220; color:#e6edf3; border:1px solid var(--border); border-radius:6px; outline:none}
  .sidebtns{display:flex; gap:8px}
  .sidebtns button{flex:1; padding:6px 8px; background:#1f2937; color:#e6edf3; border:1px solid var(--border); border-radius:6px; cursor:pointer}
  .sidebtns button:hover{background:#273449}
  #idlist{padding:6px 0 8px 0; overflow:auto; flex:1}
  .idrow{display:flex; align-items:center; gap:8px; padding:6px 12px; border-bottom:1px dashed #192030; font-size:13px}
  .idrow input{width:16px; height:16px}
  .idtag{font-weight:600; color:#cbd5e1}
  .idmeta{font-size:11px; color:#aab3c2; margin-left:auto}
  main{min-height:0;display:flex;flex-direction:column}
  #controls{flex:none;display:flex;flex-wrap:wrap;gap:8px;padding:10px 14px;background:var(--panel);border-bottom:1px solid var(--border);align-items:center}
  .group{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  .label{font-size:12px;color:#aab3c2;margin-right:4px}
  button{padding:8px 10px;background:#1f2937;color:#e6edf3;border:1px solid var(--border);border-radius:8px;cursor:pointer}
  button:hover{background:#273449}
  button[disabled]{opacity:.5;cursor:not-allowed}
  input[type="text"]{padding:8px 10px; background:#0b1220; color:#e6edf3; border:1px solid var(--border); border-radius:6px; outline:none}
  .ack.on{outline:2px solid var(--accent2)} .ack.off{outline:2px solid var(--danger)}
  #term{flex:1;min-height:0;overflow:auto;background:#030712;border-top:1px solid var(--border)}
  table{width:100%; border-collapse:separate; border-spacing:0; font-size:13px}
  thead th{position:sticky; top:0; z-index:1; text-align:left; font-weight:600; background:var(--head); color:#cbd5e1; padding:8px 10px; border-bottom:1px solid var(--border)}
  tbody td{padding:6px 10px; border-bottom:1px solid #0f172a; vertical-align:top}
  tbody tr:nth-child(even){background:var(--rowEven)} tbody tr:nth-child(odd){background:var(--rowOdd)}
  .col-idx{width:56px; color:#93a3b8} .col-time{width:118px; color:#a5b4fc}
  .col-id{width:120px; font-weight:700} .col-type{width:64px} .col-rtr{width:54px} .col-dlc{width:54px}
  .col-data{font-family:ui-monospace,Menlo,Consolas,monospace} .row-status td{color:#aab3c2; background:#0b1220}
  #sendbar{position:sticky; bottom:0; left:0; right:0; display:flex; gap:10px; align-items:center; padding:10px 14px; background:#0d1320; border-top:1px solid var(--border)}
  #tx_id{width:140px;}
  #tx_data{flex:1;}
  #tx_id,#tx_data{padding:8px 10px; background:#091120; color:#e6edf3; border:1px solid var(--border); border-radius:6px}
  #tx_opts{display:flex; gap:12px; align-items:center; color:#cbd5e1; font-size:12px}
  #dlc_badge{min-width:54px; text-align:center; padding:6px 8px; border-radius:6px; border:1px solid var(--border); background:#101828; color:#aab3c2}
  .bad{outline:2px solid var(--danger)}
  .ok{outline:2px solid var(--accent2)}
  #filegrp input{width:220px}
  #settingsBtn{margin-left:auto}

.modal{
  position:fixed;
  inset:0;
  z-index:9999;
  background:rgba(0,0,0,.5);
  display:none;
  align-items:center;
  justify-content:center;
  padding:20px;
}

.form-grid{
  display:grid;
  grid-template-columns:140px 1fr 140px 1fr;
  gap:12px 16px;
  align-items:center;
}

.form-grid label{
  width:auto;
  margin:0;
}

.form-grid input{
  width:100%;
}

@media (max-width:900px){
  .form-grid{
    grid-template-columns:1fr;
  }
}

.card{
  width:min(1100px,95vw);
}

.actions{
  display:flex;
  justify-content:flex-end;
  margin-top:16px;
}
  .modal.open{display:flex}
  .card{background:#0f172a; border:1px solid var(--border); border-radius:12px; width:min(900px,98vw); padding:16px; color:#e6edf3}
  .card h3{margin:8px 0 12px 0}
 .grid{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:20px;
  align-items:start;
}

.grid-full{
  grid-column:1 / -1;
}
  .row{display:flex; gap:8px; align-items:center}
  .row label{width:90px; font-size:12px; color:#aab3c2}
  .actions{display:flex; gap:8px; justify-content:flex-end; margin-top:12px}
  .badge{font-size:12px; color:#aab3c2}
  .sep{height:1px; background:#192030; margin:12px 0}
  .help{font-size:12px; color:#94a3b8}

  #toast{position:fixed; right:12px; bottom:12px; background:#0b1220; border:1px solid var(--border); color:#e6edf3; padding:10px 12px; border-radius:8px; display:none}
</style></head>
<body>
<header>
  <strong>WebCan Terminal</strong>
  
  <span style="margin-left: 10px; color: var(--muted); font-size: 12px;">WS:</span>
  <span id="status">connecting…</span>

  <span style="margin-left: 15px; color: var(--muted); font-size: 12px;">MQTT:</span>
  <span id="mqttDot" style="height:10px; width:10px; background-color:#ef4444; border-radius:50%; display:inline-block; margin-left:4px;" title="MQTT Status"></span>

  <div style="margin-left:auto; display:flex; gap:10px;">
    <button id="sequencerBtn" style="border-color:var(--accent); color:var(--accent)">Sequencer</button>
    <button id="settingsBtn">Settings</button>
  </div>
</header>

<div class="wrap">
  <aside id="sidebar">
    <div id="sidetop">
      <div style="font-size:12px;color:#aab3c2">Filter by CAN ID</div>
      <input id="idfilter" type="text" placeholder="Search (e.g. 0x7E8, 0x123)"/>
      <div class="sidebtns">
        <button id="selectAllBtn">Select all</button>
        <button id="selectNoneBtn">None</button>
      </div>
      <div style="font-size:12px;color:#aab3c2">Visible: <span id="selCount">all</span></div>
    </div>
    <div id="idlist"></div>
  </aside>

  <main>
    <div id="controls">
      <div class="group" id="rates">
        <span class="label">Bitrate:</span>
        <button onclick="applyBitrate(10)">10</button>
        <button onclick="applyBitrate(20)">20</button>
        <button onclick="applyBitrate(50)">50</button>
        <button onclick="applyBitrate(100)">100</button>
        <button onclick="applyBitrate(125)">125</button>
        <button onclick="applyBitrate(250)">250</button>
        <button onclick="applyBitrate(500)">500</button>
        <button onclick="applyBitrate(800)">800</button>
        <button onclick="applyBitrate(1000)">1000</button>
      </div>

<div class="group" id="ackgrp" style="margin-left:12px">
        <span class="label">Mode:</span>
        <button class="ack" id="ack-on"  onclick="openNormal()">Open</button>
        <button class="ack" id="ack-off" onclick="openListen()">Listen</button>
        <button onclick="closeCan()">Close</button>
        <span class="label" style="margin-left:8px">Routing:</span>
        <button id="bridgeBtn" onclick="toggleBridge()">Bridge: ON</button>
      </div>

      <div class="group" id="overridegrp" style="margin-left:12px">
        <span class="label">Terminal:</span>
        <button id="overrideBtn" title="When ON, each ID shows a single updating row">Override: OFF</button>
      </div>

<div class="group" id="manipgrp" style="margin-left:12px; padding-left:12px; border-left:1px solid var(--border);">
        <span class="label">Byte[0] </span>
        <input id="m_filter" type="text" style="width:40px; padding:4px;" value="21" placeholder="Hex" maxlength="2">
        <span class="label">, Set Byte[</span>
        <input id="m_idx" type="text" style="width:40px; padding:4px;" min="0" max="7" value="4">
        <span class="label">] to </span>
        <input id="m_val" type="text" style="width:40px; padding:4px;" value="00" placeholder="Hex" maxlength="2">
        <button id="manipBtn" onclick="toggleManip()">Enable Rule</button>
      </div>

      <div class="group" id="filegrp" style="margin-left:auto">
        <span class="label">Filename:</span>
        <input id="logName" type="text" placeholder="trip_2025_10_15.csv" spellcheck="false">
        <button id="startBtn" onclick="startLogging()" disabled>Start logging</button>
        <button id="stopBtn"  onclick="stopLogging()" disabled>Stop logging</button>
        <button id="downloadBtn" title="Download current CSV">Download CSV</button>
        <button id="uploadBtn" title="Upload current CSV to server">Upload to Server</button>
        <button onclick="clearTerm()">Clear</button>
      </div>
    </div>

    <div id="term">
      <table>
        <thead>
          <tr>
            <th class="col-idx">#</th>
            <th class="col-time">Time</th>
            <th class="col-id">ID</th>
            <th class="col-type">Type</th>
            <th class="col-rtr">RTR</th>
            <th class="col-dlc">DLC</th>
            <th class="col-data">Data</th>
          </tr>
        </thead>
        <tbody id="framebody"></tbody>
      </table>
    </div>

    <div id="sendbar">
      <label class="label" for="tx_id">ID</label>
      <input id="tx_id" list="idHistory" placeholder="0x123 or 123" spellcheck="false">
      <datalist id="idHistory"></datalist>

      <div id="tx_opts">
        <label><input type="checkbox" id="tx_ext"> EXT</label>
        <label><input type="checkbox" id="tx_rtr"> RTR</label>
      </div>

      <label class="label" for="tx_data">Data</label>
      <input id="tx_data" list="dataHistory" placeholder="03 22 01 05 00 00 00 00" spellcheck="false">
      <datalist id="dataHistory"></datalist>

      <div id="dlc_badge">DLC: 0</div>
      <button id="tx_send" disabled>Send</button>
      <button id="historyBtn" title="Insert last sent">⟳</button>

      <button id="openHistoryBtn" title="Open history modal">History</button>
    </div>

  </main>
</div>

<div class="modal" id="settingsModal" aria-hidden="true">
  <div class="card">
    <div style="display:flex; align-items:center; gap:10px">
      <h3 style="margin:0">Settings</h3>
      <span class="badge" id="netBadge"></span>
      <div style="margin-left:auto"><button id="closeSettings">Close</button></div>
    </div> 
    <div class="sep"></div>
    <div class="grid">
      <section>
        <h4 style="margin:4px 0 8px 0">Access Point</h4>
        <div class="row"><label>SSID</label><input id="ap_ssid" type="text" maxlength="32"></div>
        <div class="row"><label>Password</label><input id="ap_pass" type="password" maxlength="64" placeholder="min 8 chars"></div>
        <div class="actions">
        <button id="saveAp">Save AP</button>
        </div>
        <div class="help">Used when device runs its own Wi-Fi network.</div>
      </section>

      <section>
        <h4 style="margin:4px 0 8px 0">Wi-Fi STA (Client)</h4>
        <div class="row"><label>Enable</label><input id="sta_enabled" type="checkbox"></div>
        <div class="row"><label>SSID</label><input id="sta_ssid" type="text" maxlength="32" placeholder="leave blank to disable"></div>
        <div class="row"><label>Password</label><input id="sta_pass" type="password" maxlength="64"></div>
        <div class="actions">
          <button id="saveSta">Save STA</button>
        </div>
        <div class="help">On next boot, device tries to join your Wi-Fi if enabled + SSID set.</div>
      </section>

<section class="grid-full">
  <h4 style="margin:4px 0 12px 0">MQTT Broker</h4>

  <div class="form-grid">

    <label>Enable</label>
    <input id="mqtt_enabled" type="checkbox">

    <label>Server/IP</label>
    <input id="mqtt_server" type="text">

    <label>Port</label>
    <input id="mqtt_port" type="number">

    <label>Username</label>
    <input id="mqtt_user" type="text">

    <label>Password</label>
    <input id="mqtt_pass" type="password">

    <label>Sub Topic</label>
    <input id="mqtt_subTopic" type="text">

    <label>Pub Topic</label>
    <input id="mqtt_pubTopic" type="text">

  </div>

  <div class="actions">
    <button id="saveMqtt">Save MQTT</button>
  </div>

  <div class="help">
    Connects to broker via Wi-Fi STA to bridge CAN frames.
  </div>
</section>
    </div>

    <div class="sep"></div>
    <div style="display:flex; justify-content:flex-end; gap:10px">
      <button id="resetBtn" style="background:#b91c1c;border-color:#7f1d1d">Reset Device</button>
    </div>
  </div>
</div>

<div class="modal" id="historyModal" aria-hidden="true">
  <div class="card" style="max-width:1000px">
    <div style="display:flex; align-items:center; gap:10px">
      <h3 style="margin:0">Send History</h3>
      <span class="badge" id="histCount">0 items</span>
      <div style="margin-left:auto; display:flex; gap:8px; align-items:center">
        <input id="histSearch" type="text" placeholder="Filter (ID/Data/EXT/RTR)" style="padding:6px 8px; background:#0b1220; color:#e6edf3; border:1px solid var(--border); border-radius:6px; min-width:220px">
        <button id="clearHistoryBtn" style="background:#7f1d1d;border-color:#7f1d1d">Clear all</button>
        <button id="closeHistory">Close</button>
      </div>
    </div>
    <div class="sep"></div>
    <div style="max-height:50vh; overflow:auto">
      <table style="width:100%; border-collapse:collapse; font-size:13px">
        <thead>
          <tr style="position:sticky; top:0; background:#0f172a">
            <th style="text-align:left; padding:8px; border-bottom:1px solid var(--border); width:150px">Time</th>
            <th style="text-align:left; padding:8px; border-bottom:1px solid var(--border); width:110px">ID</th>
            <th style="text-align:left; padding:8px; border-bottom:1px solid var(--border); width:70px">EXT</th>
            <th style="text-align:left; padding:8px; border-bottom:1px solid var(--border); width:70px">RTR</th>
            <th style="text-align:left; padding:8px; border-bottom:1px solid var(--border); width:50px">DLC</th>
            <th style="text-align:left; padding:8px; border-bottom:1px solid var(--border)">Data</th>
            <th style="text-align:right; padding:8px; border-bottom:1px solid var(--border); width:190px">Actions</th>
          </tr>
        </thead>
        <tbody id="historyTBody"></tbody>
      </table>
    </div>
  </div>
</div>

<div class="modal" id="sequencerModal" aria-hidden="true">
  <div class="card" style="max-width:600px">
    <div style="display:flex; align-items:center; gap:10px">
      <h3 style="margin:0; color:var(--accent)">CSV Sequencer</h3>
      <div style="margin-left:auto"><button id="closeSequencer">Close</button></div>
    </div>
    <div class="sep"></div>
    <div class="grid">
      <section>
        <div class="row">
          <label>CSV File</label>
          <input type="file" id="seqFile" accept=".csv" style="width:100%">
        </div>
        <div class="row" style="margin-top:10px">
          <label>Delay (ms)</label>
          <input type="number" id="seqDelay" value="50" min="10" style="width:80px">
        </div>
      </section>
      <section style="display:flex; flex-direction:column; justify-content:center; gap:10px">
        <div id="seqStatus" style="font-size:12px; color:var(--muted); text-align:center; min-height:20px">No file loaded</div>
        <div style="display:flex; gap:5px">
          <button id="startSeqBtn" class="ok" disabled style="flex:1">Start Sequence</button>
          <button id="stopSeqBtn" class="bad" disabled style="flex:1">Stop</button>
        </div>
      </section>
    </div>
    <div class="sep"></div>
    <div class="help">
      Upload <code>c4bsilog.csv</code> style. Will parse ID, EXT, LEN, DATA and send one by one.
    </div>
  </div>
</div>

<div id="toast"></div>

<script>
// ===== Auth token for uploads =====
const AUTH_TOKEN = 'd29fbe3c8c154a7e91f9b0a6c4e8f57a';

let statusEl=document.getElementById('status');
let idFilterInput=document.getElementById('idfilter');
let idListEl=document.getElementById('idlist');
let selCountEl=document.getElementById('selCount');
let frameBody=document.getElementById('framebody');
let termScroller=document.getElementById('term');
let startBtn=document.getElementById('startBtn');
let stopBtn=document.getElementById('stopBtn');
let downloadBtn=document.getElementById('downloadBtn');
let uploadBtn=document.getElementById('uploadBtn');
let logName=document.getElementById('logName');
const settingsBtn = document.getElementById('settingsBtn');
const settingsModal = document.getElementById('settingsModal');
const closeSettings = document.getElementById('closeSettings');
const ap_ssid = document.getElementById('ap_ssid');
const ap_pass = document.getElementById('ap_pass');
const sta_enabled = document.getElementById('sta_enabled');
const sta_ssid = document.getElementById('sta_ssid');
const sta_pass = document.getElementById('sta_pass');
const saveAp = document.getElementById('saveAp');
const saveSta = document.getElementById('saveSta');
const netBadge = document.getElementById('netBadge');
const toast = document.getElementById('toast');
const resetBtn = document.getElementById('resetBtn'); 

const mqtt_enabled = document.getElementById('mqtt_enabled');
const mqtt_server = document.getElementById('mqtt_server');
const mqtt_port = document.getElementById('mqtt_port');
const mqtt_user = document.getElementById('mqtt_user');
const mqtt_pass = document.getElementById('mqtt_pass');
const saveMqtt = document.getElementById('saveMqtt');
const mqtt_subTopic = document.getElementById('mqtt_subTopic');
const mqtt_pubTopic = document.getElementById('mqtt_pubTopic'); 

// NEW: Sequencer Elements
const sequencerBtn = document.getElementById('sequencerBtn');
const sequencerModal = document.getElementById('sequencerModal');
const closeSequencer = document.getElementById('closeSequencer');
const seqFile = document.getElementById('seqFile');
const seqStatus = document.getElementById('seqStatus');
const startSeqBtn = document.getElementById('startSeqBtn');
const stopSeqBtn = document.getElementById('stopSeqBtn');
const seqDelay = document.getElementById('seqDelay');

// ===== Performance limits (tune as needed) =====
const MAX_DOM_ROWS         = 1500;
const MAX_PENDING_LINES    = 10000;
const MAX_PROCESS_PER_TICK = 1200;
const BATCH_FLUSH_MS       = 16;
const SCROLL_SNAPPING_PX   = 16;

let wsBuf = '';
let flushTimer = 0;
let nearBottomCached = true;

// ===== Sent frames cache (localStorage by default) =====
const USE_SESSION = false; 
const store = USE_SESSION ? sessionStorage : localStorage;
const LS_KEY_SENDS = 'webcan_sends_v1';
const MAX_SAVED = 100;

let sends = []; // Array<{id, ext, rtr, data, dlc, ts}>
const idHistory = document.getElementById('idHistory');
const dataHistory = document.getElementById('dataHistory');
const historyBtn = document.getElementById('historyBtn');

// Add this to your WebSocket onmessage handler logic
const mqttDot = document.getElementById('mqttDot');


function loadSends(){
  try { sends = JSON.parse(store.getItem(LS_KEY_SENDS) || '[]'); }
  catch { sends = []; }
}
function saveSends(){
  try { store.setItem(LS_KEY_SENDS, JSON.stringify(sends.slice(0, MAX_SAVED))); }
  catch {}
}
function refreshDatalists(){
  idHistory.innerHTML = '';
  dataHistory.innerHTML = '';
  const idSet = new Set();
  const dataSet = new Set();
  for (const s of sends){
    if (!idSet.has(s.id)){
      idSet.add(s.id);
      const o = document.createElement('option');
      o.value = s.id;
      idHistory.appendChild(o);
      if (idSet.size >= 50) break;
    }
  }
  for (const s of sends){
    if (s.data && !dataSet.has(s.data)){
      dataSet.add(s.data);
      const o = document.createElement('option');
      o.value = s.data;
      dataHistory.appendChild(o);
      if (dataSet.size >= 50) break;
    }
  }
}
function rememberSend(entry){
  const keyOf = (e)=>`${e.ext?'E':'S'}|${e.rtr?'R':'D'}|${e.id}|${e.data}`;
  const key = keyOf(entry);
  const i = sends.findIndex(e => keyOf(e) === key);
  if (i >= 0) sends.splice(i, 1);
  sends.unshift({ ...entry, ts: Date.now() });
  if (sends.length > MAX_SAVED) sends.length = MAX_SAVED;
  saveSends();
  refreshDatalists();
}

// Quick insert: put last sent back into the inputs
historyBtn.addEventListener('click', ()=>{
  if (!sends.length) return;
  const h = sends[0];
  txId.value = h.id;
  txExt.checked = !!h.ext;
  txRtr.checked = !!h.rtr;
  txData.value = h.data || '';
  updateDLCandValidity();
  showToast('Last sent restored');
});

resetBtn.addEventListener('click', async ()=>{
  if (!confirm('Are you sure you want to restart the device?')) return;
  try {
    const r = await fetch('/api/reset', { method:'POST' });
    await r.json();
    showToast('Device restarting…');
    setTimeout(()=> location.reload(), 4000);
  } catch (e) {
    showToast('Reset failed');
  }
});

// ===== History Modal =====
const openHistoryBtn   = document.getElementById('openHistoryBtn');
const historyModal     = document.getElementById('historyModal');
const closeHistory     = document.getElementById('closeHistory');
const clearHistoryBtn  = document.getElementById('clearHistoryBtn');
const historyTBody     = document.getElementById('historyTBody');
const histCount        = document.getElementById('histCount');
const histSearch       = document.getElementById('histSearch');

function fmtTs(ts){
  try {
    const d = new Date(ts);
    const ms = String(d.getMilliseconds()).padStart(3,'0');
    return d.toLocaleTimeString([], {hour12:false}) + '.' + ms;
  } catch { return String(ts); }
}
function renderHistory(filter=''){
  const q = (filter||'').trim().toUpperCase();
  historyTBody.innerHTML = '';
  let shown = 0;

  for (let i=0; i<sends.length; i++){
    const s = sends[i];
    const id = s.id || '';
    const data = s.data || '';
    const ext = s.ext ? '1' : '0';
    const rtr = s.rtr ? '1' : '0';
    const dlc = s.dlc ?? (data ? data.split(/[\s,]+/).filter(Boolean).length : 0);

    const hay = `${id} ${data} ${ext} ${rtr}`.toUpperCase();
    if (q && !hay.includes(q)) continue;

    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td style="padding:8px; border-bottom:1px solid #0f172a">${fmtTs(s.ts||0)}</td>
      <td style="padding:8px; border-bottom:1px solid #0f172a; font-weight:700">${id}</td>
      <td style="padding:8px; border-bottom:1px solid #0f172a">${ext==='1'?'YES':'NO'}</td>
      <td style="padding:8px; border-bottom:1px solid #0f172a">${rtr==='1'?'YES':'NO'}</td>
      <td style="padding:8px; border-bottom:1px solid #0f172a">${dlc}</td>
      <td style="padding:8px; border-bottom:1px solid #0f172a; font-family:ui-monospace,Menlo,Consolas,monospace">${data}</td>
      <td style="padding:8px; border-bottom:1px solid #0f172a; text-align:right">
        <button data-act="insert" data-idx="${i}">Insert</button>
        <button data-act="send"   data-idx="${i}">Send</button>
        <button data-act="del"    data-idx="${i}" style="background:#7f1d1d;border-color:#7f1d1d">✕</button>
      </td>
    `;
    historyTBody.appendChild(tr);
    shown++;
  }
  histCount.textContent = `${shown} item${shown===1?'':'s'}`;
}
function openHistory(){ historyModal.classList.add('open'); renderHistory(histSearch.value); }
function closeHistoryModal(){ historyModal.classList.remove('open'); }
openHistoryBtn.addEventListener('click', openHistory);
closeHistory.addEventListener('click', closeHistoryModal);
historyModal.addEventListener('click', (e)=>{ if(e.target===historyModal) closeHistoryModal(); });
histSearch.addEventListener('input', ()=> renderHistory(histSearch.value));
clearHistoryBtn.addEventListener('click', ()=>{
  if (!sends.length) return;
  if (!confirm('Clear all history?')) return;
  sends = [];
  saveSends();
  refreshDatalists();
  renderHistory(histSearch.value);
});
historyTBody.addEventListener('click', async (e)=>{
  const btn = e.target.closest('button[data-act]');
  if (!btn) return;
  const i = parseInt(btn.dataset.idx, 10);
  if (isNaN(i) || i<0 || i>=sends.length) return;

  const s = sends[i];
  if (btn.dataset.act === 'insert'){
    txId.value     = s.id || '';
    txExt.checked  = !!s.ext;
    txRtr.checked  = !!s.rtr;
    txData.value   = s.data || '';
    updateDLCandValidity();
    showToast('Inserted from history');
  }
  else if (btn.dataset.act === 'send'){
    await sendViaApi(s.id, !!s.ext, !!s.rtr, s.data || '');
  }
  else if (btn.dataset.act === 'del'){
    sends.splice(i, 1);
    saveSends();
    refreshDatalists();
    renderHistory(histSearch.value);
  }
});

async function sendViaApi(id, extBool, rtrBool, dataStr){
  let bytes = [];
  if (dataStr) {
    const s = dataStr.toUpperCase().replace(/0X/g,'').replace(/,/g,' ').trim();
    if (/\s/.test(s)) {
      bytes = s.split(/\s+/).filter(Boolean).map(x => x.length===1?'0'+x:x);
    } else {
      let flat = s.replace(/\s+/g,'');
      if (flat.length % 2) flat = '0' + flat;
      for (let i=0; i<flat.length && bytes.length<8; i+=2) bytes.push(flat.slice(i,i+2));
    }
  }
  const dlc = Math.min(bytes.length, 8);
  const body = new URLSearchParams({
    id: id,
    ext: extBool ? '1' : '0',
    rtr: rtrBool ? '1' : '0',
    dlc: String(dlc),
    data: bytes.join(' ')
  });
  try{
    const r = await fetch('/api/can/send', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
    const j = await r.json();
    if (j.ok){
      appendStatusRow('[TX OK] '+(extBool?'EXT ':'STD ')+id+' dlc='+dlc);
      rememberSend({ id, ext: extBool, rtr: rtrBool, data: bytes.join(' '), dlc });

      // ---- NEW: show TX frame in table + log it ----
      const idHex  = parseIdField(id) || id;
      const idDisp = '0x' + (parseIdField(id) || idHex);
      const rowBytes = Array.from({length:8}, (_,i)=> bytes[i] ? bytes[i].toUpperCase().padStart(2,'0') : '');
      incrementIdCountBatched(idDisp);
      const rowObj = {
        index: ++frameCounter,
        timePretty: fmtTime(),
        idDisp,
        ext: extBool,
        rtr: rtrBool,
        dlc,
        bytes: rowBytes
      };
      const fragTx = document.createDocumentFragment();
      if (overrideMode) upsertFrameRowBatched(rowObj, fragTx); else appendFrameRowBatched(rowObj, fragTx);
      frameBody.appendChild(fragTx);
      pruneOldRows();
      scrollBottom();

      logTxFrame(parseIdField(id) || id, extBool, dlc, bytes);
      // ----------------------------------------------

      if (historyModal.classList.contains('open')) renderHistory(histSearch.value);
    } else {
      appendStatusRow('[TX FAIL]');
    }
  }catch(_){ appendStatusRow('[TX ERROR]'); }
}

// Boot
loadSends();
refreshDatalists();

// Override mode (SavvyCAN-like)
let overrideMode = false;
const overrideBtn = document.getElementById('overrideBtn');
const rowMap = new Map(); // key -> <tr>
function makeRowKey(idDisp, ext, rtr){ return (ext ? 'E' : 'S') + '|' + idDisp.toUpperCase() + '|' + (rtr?'R':'D'); }
overrideBtn.addEventListener('click', ()=>{
  overrideMode = !overrideMode;
  overrideBtn.textContent = 'Override: ' + (overrideMode ? 'ON' : 'OFF');
  showToast('Override ' + (overrideMode ? 'enabled' : 'disabled'));
});

const txId   = document.getElementById('tx_id');
const txData = document.getElementById('tx_data');
const txExt  = document.getElementById('tx_ext');
const txRtr  = document.getElementById('tx_rtr');
const txSend = document.getElementById('tx_send');
const dlcBadge = document.getElementById('dlc_badge');

// Sidebar (unique IDs with checkboxes)
const knownIds = new Map(); const selected = new Set();
function filterActive(){ return selected.size > 0; }
function ensureIdRow(idStr){
  if (knownIds.has(idStr)) return knownIds.get(idStr);
  const row=document.createElement('div'); row.className='idrow';
  const cb=document.createElement('input'); cb.type='checkbox';
  cb.addEventListener('change',()=>{ if(cb.checked)selected.add(idStr); else selected.delete(idStr); updateSelCount(); });
  const tag=document.createElement('span'); tag.className='idtag'; tag.textContent=idStr;
  const meta=document.createElement('span'); meta.className='idmeta'; meta.textContent='0 msg';
  row.append(cb,tag,meta); idListEl.append(row);
  const rec={count:0,el:row,cb:cb,meta:meta}; knownIds.set(idStr,rec); return rec;
}
function incrementIdCountBatched(idStr){
  const rec = ensureIdRow(idStr);
  rec.count++;
  pendingCountUpdates.set(idStr, rec.count);
  if (!countRAF) {
    countRAF = requestAnimationFrame(()=>{
      for (const [id, cnt] of pendingCountUpdates) {
        const r = knownIds.get(id);
        if (r) r.meta.textContent = cnt + ' msg';
      }
      pendingCountUpdates.clear();
      countRAF = null;
    });
  }
}
function updateSelCount(){ selCountEl.textContent = selected.size>0 ? (selected.size+' selected') : 'all'; }
idFilterInput.addEventListener('input',()=>{
  const q=idFilterInput.value.trim().toUpperCase();
  for(const [idStr,rec] of knownIds.entries()){ rec.el.style.display = idStr.toUpperCase().includes(q)?'':'none'; }
});
document.getElementById('selectAllBtn').addEventListener('click',()=>{
  selected.clear(); for(const [idStr,rec] of knownIds.entries()){ rec.cb.checked=true; selected.add(idStr); } updateSelCount();
});
document.getElementById('selectNoneBtn').addEventListener('click',()=>{
  selected.clear(); for(const [idStr,rec] of knownIds.entries()){ rec.cb.checked=false; } updateSelCount();
});
// Batched counter updates (DOM throttle)
const pendingCountUpdates = new Map();
let countRAF = null;

// Table rendering
let frameCounter=0;
function fmtTime(){ const d=new Date(); const ms=String(d.getMilliseconds()).padStart(3,'0'); return d.toLocaleTimeString([], {hour12:false})+'.'+ms; }
function appendStatusRow(text){
  const tr=document.createElement('tr'); tr.className='row-status';
  const td=document.createElement('td'); td.colSpan=7; td.textContent=text;
  tr.appendChild(td); frameBody.appendChild(tr); scrollBottom();
}
function appendStatusRowBatched(text, frag){
  const tr=document.createElement('tr'); tr.className='row-status';
  const td=document.createElement('td'); td.colSpan=7; td.textContent=text;
  tr.appendChild(td); frag.appendChild(tr);
}
function appendFrameRowBatched(obj, frag){
  const tr=document.createElement('tr');
  tr.innerHTML=`<td class="col-idx">${obj.index}</td>
    <td class="col-time">${obj.timePretty}</td>
    <td class="col-id">${obj.idDisp}</td>
    <td class="col-type">${obj.ext?'EXT':'STD'}</td>
    <td class="col-rtr">${obj.rtr?'RTR':'DAT'}</td>
    <td class="col-dlc">${obj.dlc}</td>
    <td class="col-data">${obj.bytes.join(' ')}</td>`;
  frag.appendChild(tr);
}
function upsertFrameRowBatched(obj, frag){
  const key = makeRowKey(obj.idDisp, obj.ext, obj.rtr);
  let tr = rowMap.get(key);

  const html = `<td class="col-idx">${obj.index}</td>
    <td class="col-time">${obj.timePretty}</td>
    <td class="col-id">${obj.idDisp}</td>
    <td class="col-type">${obj.ext?'EXT':'STD'}</td>
    <td class="col-rtr">${obj.rtr?'RTR':'DAT'}</td>
    <td class="col-dlc">${obj.dlc}</td>
    <td class="col-data">${obj.bytes.join(' ')}</td>`;

  if (!tr){
    tr = document.createElement('tr');
    tr.dataset.key = key;
    tr._lastHtml = html;
    tr.innerHTML = html;
    rowMap.set(key, tr);
    frag.appendChild(tr);
  } else if (tr._lastHtml !== html) {
    tr.innerHTML = html; tr._lastHtml = html;
  }
  tr.style.outline = '2px solid #3b82f6';
  setTimeout(()=>{ tr.style.outline = ''; }, 100);
}
function pruneOldRows(){
  const overshoot = frameBody.rows.length - MAX_DOM_ROWS;
  if (overshoot > 0) {
    for (let i=0; i<overshoot; i++) {
      const r = frameBody.firstChild;
      if (!r) break;
      const key = r && r.dataset ? r.dataset.key : null;
      if (key) rowMap.delete(key);
      frameBody.removeChild(r);
    }
  }
}
function scrollBottom(){
  termScroller.scrollTop = termScroller.scrollHeight;
}

// Frame parsing from firmware text (Supports optional [CAN1]/[CAN2] prefixes)
function tryParseFrame(line){
  // Matches: "[CAN1] [ID:0x7E8..." OR just "[ID:0x7E8..."
  const m = line.match(/(?:\[CAN(\d+)\]\s*)?\[?ID:0x([0-9A-Fa-f]+)\s+(EXT|STD)\s+(RTR|DAT)\s+DLC:(\d+)\s+Data:\s*(.*?)\]?\s*$/i);
  if(!m) return null;
  
  const busNum = m[1] ? parseInt(m[1], 10) : 1; // Default to 1 if not specified
  const idHex = m[2].toUpperCase();
  const ext = (m[3].toUpperCase() === 'EXT');
  const rtr = (m[4].toUpperCase() === 'RTR');
  const dlc = parseInt(m[5], 10);
  const dataBytes = m[6].trim().length ? m[6].trim().split(/\s+/).map(b => b.toUpperCase()) : [];
  
  return { busNum, idHex, ext, rtr, dlc, dataBytes };
}

function hex8(idHex){ return idHex.padStart(8,'0').toUpperCase(); }

// Logging (SavvyCAN CSV compatible)
let logActive = false;
let logFrames = [];
let logStart_ms = null;
let logFilename = '';
const MAX_LOG_FRAMES = 500000; // cap to avoid unbounded RAM use

// ---- NEW: helper to log TX frames (same structure as RX) ----
function logTxFrame(idHex, ext, dlc, bytes) {
  if (!logActive) return;
  logFrames.push({
    ts_us_neg: rel_us_negative(),
    idHex8: hex8(idHex),
    ext: ext,
    dlc: dlc,
    bytes: bytes
  });
  if (logFrames.length > MAX_LOG_FRAMES) {
    logFrames.splice(0, logFrames.length - MAX_LOG_FRAMES);
    appendStatusRow('[log] capped (oldest dropped)');
  }
}
// ------------------------------------------------------------

function normalizeCsvName(s){
  s = (s||'').trim();
  if (!s) return '';
  s = s.replace(/[\\/:*?"<>|]+/g,'_');
  if (!/\.csv$/i.test(s)) s += '.csv';
  return s;
}
function updateStartEnabled(){
  logFilename = normalizeCsvName(logName.value);
  startBtn.disabled = !logFilename || (ws.readyState!==1);
}
logName.addEventListener('input', updateStartEnabled);

function rel_us_negative(){
  if (logStart_ms === null) logStart_ms = Date.now();
  const us = Math.round((Date.now() - logStart_ms) * 1000);
  return '-' + String(us);
}
function startLogging(){
  if (!logFilename) { updateStartEnabled(); return; }
  logActive = true; logFrames = []; logStart_ms = null;
  startBtn.disabled = true; stopBtn.disabled  = false;
  appendStatusRow('[logging] started \u2192 ' + logFilename);
}
function buildSavvyCanCSV(frames){
  const header = 'Time Stamp,ID,Extended,Dir,Bus,Len,D1,D2,D3,D4,D5,D6,D7,D8';
  const lines = [header];

  for (const f of frames){
    const raw = (f.bytes || []);
    const b = Array.from({length:8}, (_, i) => {
      let v = (raw[i] || '').toString().trim();
      if (!v) return '';
      v = v.replace(/^0x/i, '').toUpperCase();
      if (v.length === 1) v = '0' + v;
      if (v.length > 2)   v = v.slice(-2);
      return v;
    });
    const len = b.reduce((n, x) => n + (x !== '' ? 1 : 0), 0);
    const row = [
      String(f.ts_us_neg),
      String(f.idHex8),
      (f.ext ? 'TRUE' : 'FALSE'),
      'Rx',
      '0',
      String(len),
      ...b
    ];
    lines.push(row.join(','));
  }
  return lines.join('\r\n') + '\r\n';
}
function downloadCSV(csvText, filename){
  try {
    const blob = new Blob([csvText], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a'); a.style.display='none'; a.href=url; a.download=filename;
    document.body.appendChild(a); a.click();
    setTimeout(()=>{ document.body.removeChild(a); URL.revokeObjectURL(url); }, 0);
    return true;
  } catch(_) {
    try {
      const a = document.createElement('a');
      a.style.display='none'; a.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(csvText); a.download = filename;
      document.body.appendChild(a); a.click();
      setTimeout(()=>{ document.body.removeChild(a); }, 0);
      return true;
    } catch(e2) { alert('Could not trigger download.'); return false; }
  }
}
function stopLogging(){
  if (!logActive) return;
  logActive = false; startBtn.disabled = false; stopBtn.disabled  = true;
  appendStatusRow('[logging] stopped'); showToast('Logging stopped');
}
function currentCsvName(){ return (logFilename && logFilename.length) ? logFilename : ('savvycan_'+Date.now()+'.csv'); }
function getCsvTextAndName(){ const name=currentCsvName(); return { csvText: buildSavvyCanCSV(logFrames), name }; }
downloadBtn.addEventListener('click', ()=>{ const { csvText, name } = getCsvTextAndName(); const ok = downloadCSV(csvText, name); appendStatusRow(ok ? '[download] CSV saved' : '[download] failed'); });
async function uploadCsvFile(csvText, filename){
  const blob = new Blob([csvText], { type: 'text/csv' });
  try {
    const res = await fetch('https://yilmazyurdakul.com/data/upload_csv.php?name=' + encodeURIComponent(filename), {
      method: 'POST', headers: { 'X-Auth-Token': AUTH_TOKEN }, body: blob
    });
    const j = await res.json();
    if (j.ok) { showToast('Upload OK: ' + j.file); appendStatusRow('[upload] OK \u2192 ' + j.file + ' (' + (j.bytes||'?') + ' bytes)'); }
    else { showToast('Upload failed: ' + (j.err || 'unknown')); appendStatusRow('[upload] failed: ' + (j.err || 'unknown') ); }
  } catch(e){ showToast('Upload error'); appendStatusRow('[upload] error'); }
}
uploadBtn.addEventListener('click', ()=>{ const { csvText, name } = getCsvTextAndName(); uploadCsvFile(csvText, name); });

// WebSocket
let ws=new WebSocket((location.protocol==='https:'?'wss':'ws')+'://'+location.hostname+':81/');

function scrollBottom(){ termScroller.scrollTop = termScroller.scrollHeight; }
function showToast(msg){ toast.textContent = msg; toast.style.display='block'; setTimeout(()=>{ toast.style.display='none'; }, 1500); }

// ===== STREAMED WS HANDLER WITH BACKPRESSURE =====
ws.onopen = ()=> { statusEl.textContent='connected'; updateStartEnabled(); updateSendEnabled(); };
ws.onclose= ()=> { statusEl.textContent='disconnected'; updateStartEnabled(); updateSendEnabled(); };
ws.onerror= ()=> { statusEl.textContent='socket error'; updateStartEnabled(); updateSendEnabled(); };
ws.onmessage = (ev) => {
  if (ev.data === '{"type":"ka"}' || ev.data === 'KA' || ev.data === 'KA\n') return; // ignore keep-alive

  if (ev.data.startsWith('{')) {
    const msg = JSON.parse(ev.data);
    if (msg.type === 'status') {
      mqttDot.style.backgroundColor = msg.mqtt ? 'var(--accent2)' : 'var(--danger)';
      return;
    }
  }

  wsBuf += ev.data.replace(/\r/g, '');

  // Rough byte cap for pending buffer; trim to last full line
  if (wsBuf.length > 2_000_000) {
    const cut = wsBuf.lastIndexOf('\n', wsBuf.length - 1_000_000);
    wsBuf = cut > 0 ? wsBuf.slice(cut + 1) : wsBuf.slice(-1_000_000);
    appendStatusRow('[drop] throttling (client queue trimmed)');
  }

  if (!flushTimer) flushTimer = setTimeout(flushWsChunk, BATCH_FLUSH_MS);
};

function flushWsChunk(){
  flushTimer = 0;

  const lastNL = wsBuf.lastIndexOf('\n');
  let chunk = '';
  if (lastNL >= 0) {
    chunk = wsBuf.slice(0, lastNL);
    wsBuf  = wsBuf.slice(lastNL + 1);
  } else {
    return; // no full line yet
  }

  let lines = chunk.split('\n');
  if (lines.length > MAX_PENDING_LINES) {
    lines = lines.slice(lines.length - MAX_PENDING_LINES);
    appendStatusRow('[drop] too many lines (kept latest)');
  }

  const frag = document.createDocumentFragment();
  let processed = 0;

  nearBottomCached = (termScroller.scrollTop + termScroller.clientHeight >= termScroller.scrollHeight - SCROLL_SNAPPING_PX);

  for (let i = 0; i < lines.length && processed < MAX_PROCESS_PER_TICK; i++) {
    const line = lines[i];
    if (!line) continue;

    const f=tryParseFrame(line);
    if(f){
      const idDisp='0x'+f.idHex; incrementIdCountBatched(idDisp);
      const bytes = Array.from({length:8}, (_,i)=> f.dataBytes[i] ? f.dataBytes[i].toUpperCase().padStart(2,'0') : '');
      if(!filterActive() || selected.has(idDisp)){
        const rowObj = { index: ++frameCounter, timePretty: fmtTime(), idDisp, ext: f.ext, rtr: f.rtr, dlc: f.dlc, bytes };
        if (overrideMode) upsertFrameRowBatched(rowObj, frag); else appendFrameRowBatched(rowObj, frag);
      }
      if (logActive){
        logFrames.push({ ts_us_neg: rel_us_negative(), idHex8: hex8(f.idHex), ext: f.ext, dlc: f.dlc, bytes: f.dataBytes });
        if (logFrames.length > MAX_LOG_FRAMES) {
          logFrames.splice(0, logFrames.length - MAX_LOG_FRAMES);
          appendStatusRow('[log] capped (oldest dropped)');
        }
      }
      parseStatus(line);
    } else {
      appendStatusRowBatched(line, frag); parseStatus(line);
    }
    processed++;
  }

  if (frag.childNodes.length) {
    frameBody.appendChild(frag);
    pruneOldRows();
    if (nearBottomCached) scrollBottom();
  }

  if (processed < lines.length || wsBuf.length) {
    flushTimer = setTimeout(flushWsChunk, BATCH_FLUSH_MS);
  }
}

// Smooth CPU use when tab hidden: resume parsing on visible
document.addEventListener('visibilitychange', ()=>{
  if (!document.hidden && !flushTimer && wsBuf.length) {
    flushTimer = setTimeout(flushWsChunk, 0);
  }
});

// Utilities
function clearTerm(){ frameBody.innerHTML=''; frameCounter=0; rowMap.clear(); }
function setAck(on){
  const onBtn=document.getElementById('ack-on'); const offBtn=document.getElementById('ack-off');
  onBtn.classList.toggle('on',!!on); offBtn.classList.toggle('off',!on);
}
function parseStatus(line){
  if (/\bNORMAL\b/i.test(line)) setAck(true);
  if (/\bLISTEN\b/i.test(line)) setAck(false);
}

// ========== Web-only control (HTTP) ==========
let desiredKbps = 500;
let desiredMode = 'normal'; // or 'listen'
async function openNormal(){ desiredMode='normal'; await openCan(); }
async function openListen(){ desiredMode='listen'; await openCan(); }
async function applyBitrate(kbps){ desiredKbps = kbps; await openCan(); }
async function openCan(){
  try {
    const body = new URLSearchParams({ kbps: String(desiredKbps), mode: desiredMode });
    const r = await fetch('/api/can/open', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
    const j = await r.json();
    if (j.ok) { appendStatusRow('[can] opened @ '+desiredKbps+' kbps ('+desiredMode+')'); setAck(desiredMode==='normal'); }
    else { appendStatusRow('[can] open failed'); }
  } catch(e){ appendStatusRow('[can] open error'); }
}
async function closeCan(){
  try { const r = await fetch('/api/can/close', { method:'POST' }); const j = await r.json(); if (j.ok) appendStatusRow('[can] closed'); } catch(e){ appendStatusRow('[can] close error'); }
}

// ========== Bridge Control ==========
let bridgeActive = true;
async function toggleBridge() {
  bridgeActive = !bridgeActive;
  const btn = document.getElementById('bridgeBtn');
  btn.textContent = 'Bridge: ' + (bridgeActive ? 'ON' : 'OFF');
  btn.style.outline = bridgeActive ? '2px solid var(--accent2)' : '2px solid var(--danger)';
  
  try {
    const body = new URLSearchParams({ enable: bridgeActive ? '1' : '0' });
    const r = await fetch('/api/can/bridge', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
    const j = await r.json();
    if (j.ok) appendStatusRow('[bridge] ' + (bridgeActive ? 'enabled' : 'disabled'));
  } catch(e) {
    appendStatusRow('[bridge] toggle error');
  }
}

// ========== Dynamic Manipulation Control ==========
let manipActive = false;
async function toggleManip() {
  manipActive = !manipActive;
  const btn = document.getElementById('manipBtn');
  btn.textContent = manipActive ? 'Rule Active' : 'Enable Rule';
  btn.style.outline = manipActive ? '2px solid var(--accent2)' : '';

  // Get values from the inputs
  const filter = document.getElementById('m_filter').value || '00';
  const idx = document.getElementById('m_idx').value || '0';
  const val = document.getElementById('m_val').value || '00';

  try {
    const body = new URLSearchParams({ 
      enable: manipActive ? '1' : '0',
      filter: filter,
      index: idx,
      val: val
    });
    const r = await fetch('/api/can/manip', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
    const j = await r.json();
    
    if (j.ok) {
      appendStatusRow('[manip] ' + (manipActive ? `ON: If B[0]==0x${filter}, set B[${idx}]=0x${val}` : 'OFF'));
    }
  } catch(e) {
    appendStatusRow('[manip] toggle error');
  }
}


// Set initial button state outline
document.getElementById('bridgeBtn').style.outline = '2px solid var(--accent2)';

// Send panel
function parseIdField(v){
  v = (v||'').trim();
  if (v.startsWith('0x') || v.startsWith('0X')) v = v.slice(2);
  if (!/^[0-9a-fA-F]{1,8}$/.test(v)) return null;
  return v.toUpperCase();
}
function parseDataBytesFlexible(v){
  v = (v || '').toUpperCase().trim();
  v = v.replace(/0X/g, '');
  if (!v) return [];
  const hasSep = /[\s,]/.test(v);
  const out = [];
  if (hasSep) {
    const toks = v.split(/[\s,]+/).filter(Boolean);
    for (let t of toks) {
      if (!/^[0-9A-F]{1,2}$/.test(t)) return null;
      if (t.length === 1) t = '0' + t;
      out.push(t);
      if (out.length === 8) break;
    }
  } else {
    if (!/^[0-9A-F]+$/.test(v)) return null;
    if (v.length % 2 === 1) v = '0' + v;
    for (let i = 0; i < v.length && out.length < 8; i += 2) {
      out.push(v.slice(i, i + 2));
    }
  }
  return out;
}
function updateDLCandValidity(){
  const id = parseIdField(txId.value);
  const rtr = txRtr.checked;
  let bytes = [];
  if (!rtr) {
    bytes = parseDataBytesFlexible(txData.value);
    if (bytes === null) {
      dlcBadge.textContent = 'DLC: ?';
      dlcBadge.classList.remove('ok'); dlcBadge.classList.add('bad');
      updateSendEnabled(false);
      return { id, bytes: [], dlc: 0, valid: false };
    }
  }
  const dlc = rtr ? 0 : bytes.length;
  const valid = !!id && dlc >= 0 && dlc <= 8;
  dlcBadge.textContent = 'DLC: ' + dlc;
  dlcBadge.classList.toggle('bad', !valid);
  dlcBadge.classList.toggle('ok', valid);
  updateSendEnabled(valid);
  return { id, bytes, dlc, valid };
}
function updateSendEnabled(validNow){
  const wsOk = (ws && ws.readyState===1);
  if (typeof validNow === 'undefined'){ const tmp = updateDLCandValidity(); validNow = tmp.valid; }
  txSend.disabled = !(wsOk && validNow);
}
txId.addEventListener('input', ()=>{ txId.value = txId.value.toUpperCase(); updateDLCandValidity(); });
txData.addEventListener('input', ()=>{ txData.value = txData.value.toUpperCase(); updateDLCandValidity(); });
txData.addEventListener('paste', (e)=>{ setTimeout(()=>{ txData.value = txData.value.toUpperCase().replace(/,/g,' '); updateDLCandValidity(); }, 0); });
txExt.addEventListener('change', updateDLCandValidity);
txRtr.addEventListener('change', ()=>{ if (txRtr.checked) { txData.value=''; } updateDLCandValidity(); });
txData.addEventListener('keydown', (e)=>{ if (e.key==='Enter'){ e.preventDefault(); trySend(); } });
txId.addEventListener('keydown', (e)=>{ if (e.key==='Enter'){ e.preventDefault(); trySend(); } });
txSend.addEventListener('click', trySend);

async function trySend(){
  const { id, bytes, dlc, valid } = updateDLCandValidity();
  if (!valid) return;

  const extBool = txExt.checked;
  const rtrBool = txRtr.checked;
  const dataStr = (bytes||[]).join(' ');

  const body = new URLSearchParams({
    id,
    ext: extBool ? '1' : '0',
    rtr: rtrBool ? '1' : '0',
    dlc: String(dlc),
    data: dataStr
  });

  try {
    const r = await fetch('/api/can/send', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
    const j = await r.json();
    if (j.ok) {
      appendStatusRow('[TX OK] '+(extBool?'EXT ':'STD ')+id+' dlc='+dlc);
      rememberSend({ id, ext: extBool, rtr: rtrBool, data: dataStr, dlc });

      // ---- NEW: show TX frame in table + log it ----
      const idDisp = '0x' + id; // id is already sanitized hex from parseIdField
      const rowBytes = Array.from({length:8}, (_,i)=> bytes[i] ? bytes[i] : '');
      incrementIdCountBatched(idDisp);
      const rowObj = {
        index: ++frameCounter,
        timePretty: fmtTime(),
        idDisp,
        ext: extBool,
        rtr: rtrBool,
        dlc,
        bytes: rowBytes
      };
      const fragTx = document.createDocumentFragment();
      if (overrideMode) upsertFrameRowBatched(rowObj, fragTx); else appendFrameRowBatched(rowObj, fragTx);
      frameBody.appendChild(fragTx);
      pruneOldRows();
      scrollBottom();

      logTxFrame(id, extBool, dlc, bytes);
      // ----------------------------------------------
    } else {
      appendStatusRow('[TX FAIL]');
    }
  } catch(e){
    appendStatusRow('[TX ERROR]');
  }
}

// Settings UI
function openSettings(){ settingsModal.classList.add('open'); loadSettings(); }
function closeSettingsModal(){ settingsModal.classList.remove('open'); }
settingsBtn.addEventListener('click', openSettings);
closeSettings.addEventListener('click', closeSettingsModal);
settingsModal.addEventListener('click', (e)=>{ if(e.target===settingsModal) closeSettingsModal(); });

async function loadSettings(){
  try{
    // We now fetch 3 endpoints at once
    const [apRes, staRes, mqttRes] = await Promise.all([ fetch('/api/apcfg'), fetch('/api/stacfg'), fetch('/api/mqttcfg') ]);
    const ap = await apRes.json(); 
    const sta = await staRes.json();
    const mqtt = await mqttRes.json();

    ap_ssid.value = ap.ssid || ''; ap_pass.value = ap.pass || '';
    sta_enabled.checked = !!sta.enabled; sta_ssid.value = sta.ssid || ''; sta_pass.value = sta.pass || '';
    
    // Load MQTT values
    mqtt_enabled.checked = !!mqtt.enabled;
    mqtt_server.value = mqtt.server || '';
    mqtt_port.value = mqtt.port || 1883;
    mqtt_user.value = mqtt.user || '';
    mqtt_pass.value = mqtt.pass || '';
   mqtt_subTopic.value = mqtt.subTopic || 'webcan/tx'; 
   mqtt_pubTopic.value = mqtt.pubTopic || 'webcan/rx'; // NEW
    netBadge.textContent = (location.hostname === '192.168.4.1') ? 'AP mode' : 'STA mode';
  }catch(e){ showToast('Failed to load settings'); }
}


saveAp.addEventListener('click', async ()=>{
  const ssid = ap_ssid.value.trim(); const pass = ap_pass.value.trim();
  const body = new URLSearchParams({ ssid, pass });
  try{
    const r = await fetch('/api/apcfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    const j = await r.json(); showToast(j.ok ? 'AP saved. Reboot to apply.' : 'AP save failed');
  }catch(_){ showToast('AP save error'); }
});
saveSta.addEventListener('click', async ()=>{
  const ssid = sta_ssid.value.trim(); const pass = sta_pass.value.trim(); const enabled = sta_enabled.checked ? '1' : '0';
  const body = new URLSearchParams({ ssid, pass, enabled });
  try{
    const r = await fetch('/api/stacfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    const j = await r.json(); showToast(j.ok ? 'STA saved. Reboot to apply.' : 'STA save failed');
  }catch(_){ showToast('STA save error'); }
});

saveMqtt.addEventListener('click', async ()=>{
  const server = mqtt_server.value.trim(); 
  const port = mqtt_port.value || 1883;
  const user = mqtt_user.value.trim(); 
  const pass = mqtt_pass.value.trim(); 
  
  // 1. Grab the value from the HTML input
  const subTopic = mqtt_subTopic.value.trim(); 
  const pubTopic = mqtt_pubTopic.value.trim();
  const enabled = mqtt_enabled.checked ? '1' : '0';
  
  // 2. THIS IS THE CRITICAL LINE: Make sure subTopic is in this list!
const body = new URLSearchParams({ server, port, user, pass, subTopic, pubTopic, enabled });
  
  try{
    const r = await fetch('/api/mqttcfg', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body});
    const j = await r.json(); 
    showToast(j.ok ? 'MQTT saved. Reboot to apply.' : 'MQTT save failed');
  }catch(_){ showToast('MQTT save error'); }
});

// Local ping probe (works in AP, doesn’t require Internet)
let wsAlive = false;
function setOnlineUI(isUp) {
  // optional: implement a banner if desired
}
function onWSOpen()  { wsAlive = true;  setOnlineUI(true); }
function onWSClose() { wsAlive = false; setOnlineUI(false); }
async function checkLocalLink() {
  try { const r = await fetch('/api/ping', { cache: 'no-store' }); const ok = r.ok; setOnlineUI(ok || wsAlive); }
  catch (e) { setOnlineUI(wsAlive); }
}
setInterval(checkLocalLink, 4000);

// Enable start button when WS is ready
ws.addEventListener('open', ()=>{ updateStartEnabled(); });

// ===== SEQUENCER LOGIC =====
sequencerBtn.onclick = () => sequencerModal.classList.add('open');
closeSequencer.onclick = () => sequencerModal.classList.remove('open');
sequencerModal.onclick = (e) => { if(e.target === sequencerModal) sequencerModal.classList.remove('open'); }

let seqFrames = [];
let seqRunning = false;
seqFile.addEventListener('change', ()=>{
  const f = seqFile.files[0];
  if(!f) return;
  const r = new FileReader();
  r.onload = (e) => {
    const lines = e.target.result.split(/\r?\n/);
    seqFrames = [];
    // Skip header (i=1)
    for(let i=1; i<lines.length; i++){
      const cols = lines[i].split(',');
      if(cols.length < 6) continue;
      // c4bsilog format: Time, ID, Ext, Dir, Bus, Len, Data...
      // ID is often 00000752 or 752. Parse as hex.
      let idStr = cols[1];
      if(idStr.length > 8) idStr = idStr.substring(idStr.length - 8); 
      // Ensure hex format
      try { idStr = parseInt(idStr, 16).toString(16).toUpperCase(); } catch(e){ continue; }
      
      const ext = (cols[2].toUpperCase() === 'TRUE');
      const dlc = parseInt(cols[5]);
      let data = "";
      for(let j=0; j<dlc; j++) {
        if(cols[6+j]) data += cols[6+j].trim() + " ";
      }
      seqFrames.push({id: idStr, ext, rtr:false, dlc, data: data.trim()});
    }
    seqStatus.innerText = `Loaded ${seqFrames.length} frames. Ready.`;
    startSeqBtn.disabled = false;
  };
  r.readAsText(f);
});

startSeqBtn.addEventListener('click', async ()=>{
  if(!seqFrames.length) return;
  seqRunning = true;
  startSeqBtn.disabled = true;
  stopSeqBtn.disabled = false;
  const delay = parseInt(seqDelay.value) || 50;

  for(let i=0; i<seqFrames.length; i++){
    if(!seqRunning) break;
    const f = seqFrames[i];
    seqStatus.innerText = `Sending ${i+1}/${seqFrames.length}: ID ${f.id}`;
    
    // Use the existing send API helper
    try {
        const body = new URLSearchParams({ 
            id: f.id, 
            ext: f.ext?'1':'0', 
            rtr: '0', 
            data: f.data, 
            dlc: f.dlc 
        });
        await fetch('/api/can/send', { method:'POST', body });
    } catch(err){}
    
    await new Promise(r => setTimeout(r, delay));
  }
  seqRunning = false;
  startSeqBtn.disabled = false;
  stopSeqBtn.disabled = true;
  seqStatus.innerText = "Sequence Finished.";
});

stopSeqBtn.addEventListener('click', ()=>{
  seqRunning = false;
  seqStatus.innerText = "Stopped by user.";
});

</script>
</body></html>
)HTML";