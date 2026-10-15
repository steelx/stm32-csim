'use strict';
// Presentation only. All register semantics, PA5 and LED state arrive from C.
const $ = id => document.getElementById(id);
const hex = n => '0x' + (n >>> 0).toString(16).toUpperCase().padStart(8, '0');
let latest, traceKey = '';
function showError(message) { $('error').textContent = message; $('error').hidden = false; }
function render(state) {
  latest = state;
  $('status').textContent = state.fault ? 'FAULT' : state.running ? 'RUNNING' : 'PAUSED';
  $('status').classList.toggle('running', state.running);
  $('led').classList.toggle('on', state.led);
  $('led-label').textContent = state.led ? 'ON · driven by ARM firmware' : 'OFF';
  $('led-label').setAttribute('fill', state.led ? '#65e5b0' : '#91a1b8');
  $('signal-wire').classList.toggle('on-wire', state.pa5 === 2);
  const signalNames = ['HIGH IMPEDANCE', 'LOW', 'HIGH', 'CONFLICT'];
  $('pin-label').textContent = 'PA5 · ' + signalNames[state.pa5];
  $('circuit-status').textContent = state.led ? 'D1 illuminated · PA5 HIGH' : 'D1 off · PA5 ' + signalNames[state.pa5];
  $('led-dot').style.background = state.led ? '#65e5b0' : '#657689';
  for (const name of ['rcc', 'moder', 'odr']) $(name).textContent = hex(state[name]);
  $('access-count').textContent = `${state.accesses} accesses`;
  $('execution').textContent = `PC ${hex(state.pc)} · ${state.slices} slices`;
  if (state.fault) showError(state.error);
  else $('error').hidden = true;
  const key = `${state.generation}:${state.accesses}:${$('reads').checked}`;
  if (key !== traceKey) {
    traceKey = key;
    $('trace').replaceChildren();
    const events = state.trace.filter(e => e.write || $('reads').checked).reverse();
    if (!events.length) {
      const p = document.createElement('p'); p.className = 'empty';
      p.textContent = 'Press Run to follow the firmware’s register accesses.'; $('trace').append(p);
    }
    for (const event of events) {
      const row = document.createElement('div'); row.className = 'event';
      const top = document.createElement('div'); top.className = 'event-top';
      const badge = document.createElement('span'); badge.className = 'badge' + (event.write ? ' write' : '');
      badge.textContent = event.write ? 'WRITE' : 'READ';
      const name = document.createElement('span'); name.textContent = event.name;
      const value = document.createElement('span'); value.className = 'value'; value.textContent = hex(event.value);
      top.append(badge, name, value);
      const address = document.createElement('p'); address.className = 'address';
      address.textContent = `#${event.sequence} · ${hex(event.address)} · ${event.accepted ? 'accepted' : 'clock gated'}`;
      const meaning = document.createElement('p'); meaning.textContent = event.meaning;
      row.append(top, address, meaning); $('trace').append(row);
    }
  }
}
for (const button of document.querySelectorAll('[data-command]')) {
  button.addEventListener('click', () => window.sim_command(button.dataset.command).catch(showError));
}
$('reads').addEventListener('change', () => { if (latest) render(latest); });
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function poll() {
  try { render(await window.sim_state()); } catch (error) { showError(String(error)); }
  setTimeout(poll, 60); // Only one state request at a time; no unbounded UI queue.
}
async function smokeTest() {
  // Runs inside the actual native WebView: tests C bindings and SVG output.
  async function until(predicate, timeout = 20000) {
    const end = Date.now() + timeout;
    while (Date.now() < end) {
      const s = await window.sim_state(); render(s);
      if (s.fault) throw new Error(s.error);
      if (predicate(s)) return s;
      await delay(30);
    }
    throw new Error('Smoke test timed out');
  }
  try {
    await until(s => s.ready);
    await window.sim_command('run');
    await until(s => s.led && $('led').classList.contains('on') && $('led-label').textContent.startsWith('ON'));
    await until(s => s.pa5 === 1 && !s.led && !$('led').classList.contains('on'));
    await window.sim_command('pause');
    const paused = await until(s => !s.running);
    await delay(100);
    if ((await window.sim_state()).slices !== paused.slices) throw new Error('Pause did not stop execution');
    await window.sim_command('step');
    await until(s => s.slices === paused.slices + 1 && !s.running);
    await window.sim_command('reset');
    await until(s => s.generation > paused.generation && s.slices === 0 && s.accesses === 0 && s.pa5 === 0 && !s.led);
    await window.sim_smoke_done(true);
  } catch (error) { showError(String(error)); await window.sim_smoke_done(String(error)); }
}
window.sim_source().then(source => { $('source').value = source; }).catch(showError);
poll();
if (window.CSIM_SMOKE) smokeTest();
