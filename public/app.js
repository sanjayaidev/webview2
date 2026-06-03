// app.js — Frontend WebSocket client

const WS_URL = `ws://127.0.0.1:3721`;
let ws = null;
let pendingCallbacks = {};
let msgId = 0;
let currentRecording = null;

// ── WebSocket ─────────────────────────────────────────────────────────────────
function connect() {
    ws = new WebSocket(WS_URL);

    ws.onopen = () => {
        setWsStatus(true);
        log('info', 'Connected to backend');
    };

    ws.onclose = () => {
        setWsStatus(false);
        log('err', 'Disconnected — retrying in 2s…');
        setTimeout(connect, 2000);
    };

    ws.onerror = () => {
        setWsStatus(false);
    };

    ws.onmessage = (e) => {
        try {
            const msg = JSON.parse(e.data);
            const cb = pendingCallbacks[msg.id];
            if (cb) {
                delete pendingCallbacks[msg.id];
                cb(msg.result);
            }
        } catch {}
    };
}

function send(action, payload) {
    return new Promise((resolve) => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            log('err', `WS not connected — cannot send: ${action}`);
            resolve({ error: 'not connected' });
            return;
        }
        const id = ++msgId;
        pendingCallbacks[id] = (result) => {
            if (result?.error) {
                log('err', `${action} → ${result.error}`);
            } else {
                log('ok', `${action} → OK`);
            }
            resolve(result);
        };
        ws.send(JSON.stringify({ id, action, payload }));
    });
}

function setWsStatus(connected) {
    const dot   = document.getElementById('ws-dot');
    const label = document.getElementById('ws-label');
    dot.className   = 'ws-dot' + (connected ? ' connected' : '');
    label.textContent = connected ? 'Connected' : 'Disconnected';
}

// ── Logging ───────────────────────────────────────────────────────────────────
function log(type, msg, targetId = 'log') {
    const container = document.getElementById(targetId);
    if (!container) return;
    const now = new Date();
    const time = now.toTimeString().slice(0,8);
    const line = document.createElement('div');
    line.className = 'log-line';
    line.innerHTML = `<span class="log-time">${time}</span><span class="log-${type}">${escHtml(msg)}</span>`;
    container.appendChild(line);
    container.scrollTop = container.scrollHeight;
}

function escHtml(str) {
    return String(str).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// ── Navigation ────────────────────────────────────────────────────────────────
document.querySelectorAll('.nav-item').forEach(item => {
    item.addEventListener('click', () => {
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
        item.classList.add('active');
        const panel = document.getElementById('panel-' + item.dataset.panel);
        if (panel) panel.classList.add('active');

        // Topbar title
        document.querySelector('.topbar-title').textContent = item.textContent.trim().toUpperCase();
    });
});

// ── Dashboard actions ─────────────────────────────────────────────────────────
function quickClick()   { navTo('mouse'); }
function quickType()    { navTo('keyboard'); }
function quickHotkey()  { navTo('keyboard'); }
function quickWindows() { navTo('windows'); loadWindows(); }

function navTo(panel) {
    document.querySelector(`[data-panel="${panel}"]`).click();
}

function doQuickClick() {
    send('mouse.click', {
        x: +id('d-x').value,
        y: +id('d-y').value,
        button: id('d-btn').value,
    });
}

function doQuickType() {
    send('keyboard.type', {
        text: id('d-text').value,
        delayMs: +id('d-delay').value,
    });
}

// ── Mouse actions ─────────────────────────────────────────────────────────────
function doMouseClick() {
    send('mouse.click', {
        x: +id('m-cx').value,
        y: +id('m-cy').value,
        button: id('m-cbtn').value,
        double: id('m-cdbl').value === 'true',
    });
}

function doScroll() {
    send('mouse.scroll', {
        x: +id('m-sx').value,
        y: +id('m-sy').value,
        direction: id('m-sdir').value,
        amount: +id('m-samt').value,
    });
}

// ── Keyboard actions ──────────────────────────────────────────────────────────
function doHotkey() {
    const keys = id('k-hotkey').value.split(',').map(k => k.trim()).filter(Boolean);
    if (!keys.length) return;
    send('keyboard.hotkey', { keys });
}

// ── Windows ───────────────────────────────────────────────────────────────────
async function loadWindows() {
    const result = await send('window.list', {});
    const list = document.getElementById('win-list');
    list.innerHTML = '';
    if (!result || result.error || !result.length) {
        list.innerHTML = '<span style="color:var(--muted);font-size:12px;">No windows found.</span>';
        return;
    }
    result.forEach(w => {
        const el = document.createElement('div');
        el.className = 'win-item';
        el.innerHTML = `
            <div class="win-title" title="${escHtml(w.title)}">${escHtml(w.title)}</div>
            <div class="win-id">#${w.id}</div>
            <button class="btn btn-sm" onclick="send('window.focus',{id:${w.id}})">Focus</button>
            <button class="btn btn-sm btn-danger" onclick="send('window.close',{id:${w.id}})">Close</button>
        `;
        list.appendChild(el);
    });
}

// ── Recorder ──────────────────────────────────────────────────────────────────
function recStart() {
    send('recorder.start', {}).then(() => {
        id('rec-badge').classList.add('active');
        id('rec-start').disabled = true;
        id('rec-stop').disabled = false;
        id('rec-result').style.display = 'none';
        currentRecording = null;
    });
}

function recStop() {
    send('recorder.stop', {}).then(result => {
        id('rec-badge').classList.remove('active');
        id('rec-start').disabled = false;
        id('rec-stop').disabled = true;
        currentRecording = result?.events || [];
        id('rec-result').style.display = 'block';
        log('info', `Recorded ${currentRecording.length} events`);
    });
}

function recSave() {
    const name = id('rec-name').value.trim();
    if (!name) { alert('Enter a macro name.'); return; }
    send('recorder.save', { name, events: currentRecording }).then(() => {
        log('ok', `Saved macro: ${name}`);
        loadMacros();
    });
}

function recPlayCurrent() {
    if (!currentRecording?.length) return;
    send('recorder.play', { events: currentRecording });
}

async function loadMacros() {
    const result = await send('recorder.list', {});
    const list = document.getElementById('macro-list');
    list.innerHTML = '';
    if (!result?.length) {
        list.innerHTML = '<span style="color:var(--muted);font-size:12px;">No macros saved.</span>';
        return;
    }
    result.forEach(m => {
        const el = document.createElement('div');
        el.className = 'macro-item';
        el.innerHTML = `
            <div class="macro-name">${escHtml(m.name)}</div>
            <button class="btn btn-sm btn-success" onclick="playMacro('${escHtml(m.name)}')">▶ Play</button>
        `;
        list.appendChild(el);
    });
}

async function playMacro(name) {
    const macro = await send('recorder.load', { name });
    if (macro?.events) {
        send('recorder.play', { events: macro.events });
    }
}

// ── Script editor ─────────────────────────────────────────────────────────────
const snippets = {
    click:  '{"action":"mouse.click","payload":{"x":500,"y":400,"button":"left"}}',
    type:   '{"action":"keyboard.type","payload":{"text":"Hello, World!"}}',
    hotkey: '{"action":"keyboard.hotkey","payload":{"keys":["ctrl","c"]}}',
    sleep:  '{"action":"sleep","payload":{"ms":1000}}',
};

function insertSnippet(name) {
    const editor = id('script-editor');
    const snip = snippets[name];
    const pos = editor.selectionStart;
    const val = editor.value;
    editor.value = val.slice(0, pos) + (pos && val[pos-1] !== '\n' ? '\n' : '') + snip + '\n' + val.slice(pos);
    editor.focus();
}

async function runScript() {
    const lines = id('script-editor').value
        .split('\n')
        .map(l => l.trim())
        .filter(l => l && !l.startsWith('//'));

    const logEl = document.getElementById('script-log');
    logEl.innerHTML = '';

    for (const line of lines) {
        let cmd;
        try { cmd = JSON.parse(line); }
        catch { log('err', `Parse error: ${line}`, 'script-log'); continue; }

        if (cmd.action === 'sleep') {
            const ms = cmd.payload?.ms || 1000;
            log('info', `Sleeping ${ms}ms…`, 'script-log');
            await new Promise(r => setTimeout(r, ms));
            continue;
        }

        const result = await send(cmd.action, cmd.payload);
        log(result?.error ? 'err' : 'ok', `${cmd.action}`, 'script-log');
    }

    log('info', 'Script finished.', 'script-log');
}

// ── Helpers ───────────────────────────────────────────────────────────────────
function id(x) { return document.getElementById(x); }

// ── Boot ──────────────────────────────────────────────────────────────────────
connect();
