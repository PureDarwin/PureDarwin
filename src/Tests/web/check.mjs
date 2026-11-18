// check.mjs - load an AppKit page in headless Chromium, click its button, and check the app saw the click
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// node check.mjs CHROMIUM PAGE_DIR OUT_DIR: serves PAGE_DIR, writes before.png and after.png to OUT_DIR,
// and exits 0 only when the page printed PD-READY and then PD-CLICK 1 for a click on the button.
import { spawn } from 'node:child_process';
import { writeFileSync, mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const [chromium, pageDir, outDir] = process.argv.slice(2);
const httpPort = 8000 + Math.floor(Math.random() * 1000);
const devtoolsPort = 9300 + Math.floor(Math.random() * 600);
const fail = (why) => { console.error('FAIL ' + why); cleanup(); process.exit(1); };

// the page needs a web server: wasm and its preloaded data are fetched, file:// will not do
const server = spawn('python3', ['-m', 'http.server', String(httpPort), '--bind', '127.0.0.1'], { cwd: pageDir, stdio: 'ignore' });
const browser = spawn(chromium, ['--headless=new', '--no-sandbox', '--disable-gpu', '--no-first-run',
    '--window-size=1024,768', `--remote-debugging-port=${devtoolsPort}`,
    `--user-data-dir=${mkdtempSync(join(tmpdir(), 'pd-chromium-'))}`, 'about:blank'], { stdio: 'ignore' });
function cleanup() { browser.kill(); server.kill(); }
setTimeout(() => fail('timed out'), 120000);

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function pageTarget() {
    for (let i = 0; i < 100; i++) {
        try {
            const list = await (await fetch(`http://127.0.0.1:${devtoolsPort}/json`)).json();
            const page = list.find((t) => t.type === 'page');
            if (page) return page.webSocketDebuggerUrl;
        } catch { }
        await sleep(200);
    }
    fail('no DevTools endpoint');
}

const ws = new WebSocket(await pageTarget());
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let nextId = 1;
const pending = new Map();
const lines = [];
const waiters = [];
ws.addEventListener('message', (m) => {
    const msg = JSON.parse(m.data);
    if (msg.id && pending.has(msg.id)) {
        pending.get(msg.id)(msg);
        pending.delete(msg.id);
    } else if (msg.method === 'Runtime.consoleAPICalled') {
        const text = msg.params.args.map((a) => a.value ?? a.description ?? '').join(' ');
        console.log('page: ' + text);
        lines.push(text);
        waiters.forEach((w) => w());
    } else if (msg.method === 'Runtime.exceptionThrown') {
        console.log('page exception: ' + (msg.params.exceptionDetails.exception?.description ?? msg.params.exceptionDetails.text));
    }
});
const send = (method, params = {}) => new Promise((resolve) => {
    const id = nextId++;
    pending.set(id, resolve);
    ws.send(JSON.stringify({ id, method, params }));
});
async function waitForLine(prefix, ms) {
    const end = Date.now() + ms;
    while (Date.now() < end) {
        const hit = lines.find((l) => l.startsWith(prefix));
        if (hit) return hit;
        await new Promise((r) => { waiters.push(r); setTimeout(r, 250); });
    }
    return null;
}
async function screenshot(name) {
    const shot = await send('Page.captureScreenshot', { format: 'png' });
    writeFileSync(join(outDir, name), Buffer.from(shot.result.data, 'base64'));
}

await send('Runtime.enable');
await send('Page.enable');
await send('Page.navigate', { url: `http://127.0.0.1:${httpPort}/index.html` });
if (!(await waitForLine('PD-READY', 90000)))
    fail('the app never printed PD-READY');
await sleep(500);
await screenshot('before.png');

// the button sits 20 points in from the window's left edge and 40 up from its bottom, 160 by 32
const box = (await send('Runtime.evaluate', {
    expression: 'JSON.stringify((() => { const c = document.querySelector("canvas"); const r = c.getBoundingClientRect(); return { left: r.left, bottom: r.bottom }; })())',
    returnByValue: true,
})).result.result.value;
const { left, bottom } = JSON.parse(box);
const x = left + 20 + 80, y = bottom - (40 + 16);
for (const type of ['mousePressed', 'mouseReleased'])
    await send('Input.dispatchMouseEvent', { type, x, y, button: 'left', buttons: type === 'mousePressed' ? 1 : 0, clickCount: 1 });
if (!(await waitForLine('PD-CLICK 1', 10000)))
    fail(`no PD-CLICK after a click at ${x},${y}`);
await sleep(300);
await screenshot('after.png');
console.log('PASS');
cleanup();
process.exit(0);
