// Minimal UI: fetch ui/snapshots/manifest.json, then fetch each snapshot and animate them.
let manifestUrl = 'snapshots/manifest.json';
let snapshots = [];
let current = 0;
let timer = null;

const statusEl = document.getElementById('status');
const playBtn = document.getElementById('playBtn');
const pauseBtn = document.getElementById('pauseBtn');
const intervalInput = document.getElementById('interval');
const tableEl = document.getElementById('table');
const statsEl = document.getElementById('stats');

playBtn.onclick = () => { if (!timer) start(); };
pauseBtn.onclick = () => { stop(); };

async function loadManifest() {
  try {
    const r = await fetch(manifestUrl + '?_=' + Date.now());
    if (!r.ok) { statusEl.textContent = 'No manifest yet (run the C program to create snapshots)'; return; }
    const j = await r.json();
    snapshots = j.snapshots || [];
    statusEl.textContent = `Found ${snapshots.length} snapshots`;
  } catch (e) {
    statusEl.textContent = 'Error loading manifest: ' + e;
  }
}

async function loadSnapshot(idx) {
  if (idx < 0 || idx >= snapshots.length) return null;
  const url = 'snapshots/' + snapshots[idx] + '?_=' + Date.now();
  try {
    const r = await fetch(url);
    if (!r.ok) return null;
    return await r.json();
  } catch (e) {
    console.error('Failed to load snapshot', e);
    return null;
  }
}

function renderSnapshot(data) {
  if (!data) return;
  tableEl.innerHTML = '';
  const bs = data.buckets;
  if (!Array.isArray(bs)) return;
  const cols = Math.min(32, Math.max(8, Math.ceil(Math.sqrt(bs.length))));
  tableEl.style.gridTemplateColumns = `repeat(${cols}, 40px)`;
  for (let i = 0; i < bs.length; ++i) {
    const v = bs[i];
    const div = document.createElement('div');
    div.className = 'bucket';
    if (data.strategy === 'CHAINING') {
      if (v === null) { div.classList.add('empty'); div.textContent = ''; }
      else { div.classList.add('chaining-bucket'); div.textContent = v.join(','); }
    } else {
      if (v === null) { div.classList.add('empty'); div.textContent = ''; }
      else if (typeof v === 'object' && v.deleted) { div.classList.add('deleted'); div.textContent = 'DEL'; }
      else { div.classList.add('occupied'); div.textContent = String(v); }
    }
    tableEl.appendChild(div);
  }
  statsEl.textContent = `Strategy: ${data.strategy}  | size: ${data.table_size}  | inserted: ${data.inserted}  | collisions: ${data.collisions}  | probes: ${data.probes}`;
}

async function step() {
  if (current >= snapshots.length) {
    // try reload manifest in case new snapshots were added
    await loadManifest();
    if (current >= snapshots.length) { stop(); return; }
  }
  const s = await loadSnapshot(current);
  if (s) renderSnapshot(s);
  current++;
}

function start() {
  if (snapshots.length === 0) { loadManifest().then(()=>{ if (snapshots.length>0) start(); else statusEl.textContent = 'No snapshots yet'; }); return; }
  const ms = Math.max(50, parseInt(intervalInput.value) || 500);
  timer = setInterval(step, ms);
  statusEl.textContent = 'Playing...';
}

function stop() {
  if (timer) { clearInterval(timer); timer = null; statusEl.textContent = 'Paused'; }
}

loadManifest();