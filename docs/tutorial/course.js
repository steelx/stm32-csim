/* Local behavior only; no telemetry or network storage. */
(() => {
  const $ = id => document.getElementById(id);
  const key = 'stm32-breadboard-module1-v1';
  let saved = {};
  let storageOK = true;
  try { saved = JSON.parse(localStorage.getItem(key) || '{}'); } catch { storageOK = false; }
  if (!saved || typeof saved !== 'object' || Array.isArray(saved)) saved = {};
  const checks = [...document.querySelectorAll('[data-progress]')];
  function update() {
    const n = checks.filter(c => c.checked).length;
    if ($('progress-text')) $('progress-text').textContent = `${n} / ${checks.length} checkpoints`;
    if ($('progress-bar')) $('progress-bar').value = n;
    if ($('storage-status')) $('storage-status').textContent = storageOK
      ? 'Progress and notes save in this browser. Export a copy before moving files or clearing browser data.'
      : 'Browser storage unavailable. Use Export notebook to keep your work.';
  }
  function persist() {
    try { localStorage.setItem(key, JSON.stringify(saved)); } catch { storageOK = false; }
    update();
  }
  checks.forEach(c => {
    c.checked = saved[c.id] === true;
    c.addEventListener('change', () => { saved[c.id] = c.checked; persist(); });
  });
  document.querySelectorAll('[data-note]').forEach(n => {
    n.value = typeof saved[n.id] === 'string' ? saved[n.id] : '';
    n.addEventListener('input', () => { saved[n.id] = n.value; persist(); });
  });
  update();
  $('export-notes')?.addEventListener('click', () => {
    const lines = ['STM32 breadboard course — Module 1 notebook', new Date().toISOString(), ''];
    checks.forEach(c => lines.push(`${c.checked ? '[x]' : '[ ]'} ${c.parentElement.textContent.trim()}`));
    document.querySelectorAll('[data-note]').forEach(n => lines.push('\n' + n.labels[0].textContent, n.value || '(No notes)'));
    const url = URL.createObjectURL(new Blob([lines.join('\n')], {type:'text/plain;charset=utf-8'}));
    const a = document.createElement('a'); a.href = url; a.download = 'module-01-notebook.txt'; a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });
  $('print-guide')?.addEventListener('click', () => {
    document.querySelectorAll('details').forEach(d => d.open = true);
    window.print();
  });
  function led() {
    if (!$('led-r')) return;
    const r = Number($('led-r').value), vf = Number($('led-vf').value);
    if (!Number.isFinite(r) || r <= 0 || !Number.isFinite(vf) || vf < 0 || vf >= 3.3) {
      $('led-result').textContent = 'Use R > 0 and an LED forward voltage from 0 to below 3.3 V.'; return;
    }
    const i = (3.3-vf)/r;
    $('led-result').textContent = `Ideal estimate: ${(i*1000).toFixed(2)} mA; resistor ${(i*i*r*1000).toFixed(2)} mW. ${i > .003 ? 'Above this course’s 3 mA external-LED target; increase R.' : 'Within the 3 mA external-LED target.'} Actual current depends on the LED and output voltage.`;
  }
  ['led-r','led-vf'].forEach(id => $(id)?.addEventListener('input',led)); led();
  function rc() {
    if (!$('rc-r')) return;
    const r = Number($('rc-r').value), c = Number($('rc-c').value);
    if (!Number.isFinite(r) || !Number.isFinite(c) || r<=0 || c<=0) {
      $('rc-result').textContent='Enter positive resistance and capacitance.'; return;
    }
    const tau = r*c; // kΩ × µF = ms
    $('rc-result').textContent = `τ = ${tau.toFixed(2)} ms. 50% crossing ≈ ${(tau*Math.log(2)).toFixed(2)} ms. 5τ ≈ ${(5*tau).toFixed(2)} ms (99.3%). Ideal model, not a measurement.`;
    const pts = Array.from({length:101},(_,i)=>`${50+i*4.8},${190-150*(1-Math.exp(-5*i/100))}`).join(' ');
    $('rc-curve').setAttribute('points',pts);
    $('rc-axis').textContent=`Time: 0 → ${(5*tau).toFixed(1)} ms`;
  }
  ['rc-r','rc-c'].forEach(id => $(id)?.addEventListener('input',rc)); rc();
})();
