const fs   = require('fs');
const path = require('path');

const MACROS_DIR = path.join(__dirname, '..', '..', 'macros');
if (!fs.existsSync(MACROS_DIR)) fs.mkdirSync(MACROS_DIR, { recursive: true });

let recording   = false;
let recordStart = 0;
let events      = [];

function start() {
    events      = [];
    recordStart = Date.now();
    recording   = true;
    return { ok: true, recording: true };
}

function stop() {
    recording = false;
    return { ok: true, recording: false, eventCount: events.length, events };
}

// Called internally by mouse/keyboard hooks (future integration point)
function record(action, payload) {
    if (!recording) return;
    events.push({ t: Date.now() - recordStart, action, payload });
}

async function play({ events: evts, speed = 1 }) {
    const mouse    = require('./mouse');
    const keyboard = require('./keyboard');
    const wins     = require('./window');

    let lastT = 0;
    for (const ev of evts) {
        const delay = (ev.t - lastT) / speed;
        if (delay > 0) await sleep(delay);
        lastT = ev.t;

        switch (ev.action) {
            case 'mouse.move':   await mouse.move(ev.payload);   break;
            case 'mouse.click':  await mouse.click(ev.payload);  break;
            case 'mouse.scroll': await mouse.scroll(ev.payload); break;
            case 'keyboard.type':   await keyboard.type(ev.payload);   break;
            case 'keyboard.hotkey': await keyboard.hotkey(ev.payload); break;
            case 'keyboard.press':  await keyboard.press(ev.payload);  break;
        }
    }
    return { ok: true, played: evts.length };
}

function save({ name, events: evts }) {
    const file = path.join(MACROS_DIR, `${sanitize(name)}.json`);
    fs.writeFileSync(file, JSON.stringify({ name, events: evts }, null, 2));
    return { ok: true, file };
}

function load({ name }) {
    const file = path.join(MACROS_DIR, `${sanitize(name)}.json`);
    if (!fs.existsSync(file)) throw new Error(`Macro not found: ${name}`);
    return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function listMacros() {
    const files = fs.readdirSync(MACROS_DIR).filter(f => f.endsWith('.json'));
    return files.map(f => ({
        name: path.basename(f, '.json'),
        file: path.join(MACROS_DIR, f),
    }));
}

function sanitize(name) {
    return name.replace(/[^a-zA-Z0-9_\-]/g, '_');
}

function sleep(ms) {
    return new Promise(r => setTimeout(r, ms));
}

module.exports = { start, stop, record, play, save, load, listMacros };
