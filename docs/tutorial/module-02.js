/* Original teaching simulations. No network requests, no hardware control. */
(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const clamp = (x, lo, hi) => Math.max(lo, Math.min(hi, x));
  const number = (id, lo, hi, fallback) => {
    const el = $(id), value = Number(el.value);
    if (!el.value.trim() || !Number.isFinite(value)) { el.value = fallback; return fallback; }
    const bounded = clamp(value, lo, hi); el.value = bounded; return bounded;
  };
  const ns = 'http://www.w3.org/2000/svg';
  function element(tag, attrs, text) {
    const e = document.createElementNS(ns, tag);
    for (const [k,v] of Object.entries(attrs || {})) e.setAttribute(k, String(v));
    if (text !== undefined) e.textContent = text;
    return e;
  }
  function plot(id, series, options = {}) {
    const svg = $(id), {xmin=0,xmax=1,ymin=0,ymax=1,xlabel='Time (s)',ylabel='Value',log=false} = options;
    svg.querySelector('[data-plot-layer]')?.remove();
    const g = element('g', {'data-plot-layer':'', 'font-family':'Segoe UI, sans-serif','font-size':13});
    const x = v => 72 + 720 * (log ? Math.log(v/xmin)/Math.log(xmax/xmin) : (v-xmin)/(xmax-xmin));
    const y = v => 238 - 180 * (v-ymin)/(ymax-ymin);
    for(let i=0;i<=4;i++) {
      const v=ymin+(ymax-ymin)*i/4, py=y(v);
      g.append(element('path',{d:`M72 ${py}H792`,stroke:'#e2e8f0'}),element('text',{x:64,y:py+4,'text-anchor':'end',fill:'#475569'},v.toFixed(Math.abs(ymax-ymin)<3?2:0)));
      const xv=log?xmin*Math.pow(xmax/xmin,i/4):xmin+(xmax-xmin)*i/4;
      g.append(element('text',{x:x(xv),y:260,'text-anchor':'middle',fill:'#475569'},log?xv.toPrecision(2):xv.toFixed(xmax<1?3:1)));
    }
    series.forEach((s,i) => {
      const points=s.data.filter(p=>Number.isFinite(p[0])&&Number.isFinite(p[1]));
      const d=points.map((p,j)=>`${j?'L':'M'}${x(p[0]).toFixed(2)},${y(clamp(p[1],ymin,ymax)).toFixed(2)}`).join(' ');
      g.append(element('path',{d,stroke:s.color,fill:'none','stroke-width':2,'stroke-dasharray':s.dash||''}));
      g.append(element('text',{x:80+i*225,y:24,fill:s.color,'font-weight':700},s.label));
    });
    g.append(element('text',{x:440,y:293,'text-anchor':'middle',fill:'#334155'},xlabel),element('text',{x:72,y:48,fill:'#334155'},ylabel));
    svg.append(g);
  }
  const hallCodes = ['001','101','100','110','010','011'];
  const phasePairs = [['A','B'],['A','C'],['B','C'],['B','A'],['C','A'],['C','B']];
  let sector=0, hallFault=false;
  function hall() {
    const pairs=number('m02-pairs',1,20,4), rpm=number('m02-hall-rpm',0,10000,600);
    document.querySelectorAll('[data-sector]').forEach(e=>e.classList.toggle('live-cell',!hallFault&&Number(e.dataset.sector)===sector));
    document.querySelectorAll('[data-switch]').forEach(e=>{e.setAttribute('stroke','#94a3b8');e.setAttribute('stroke-width','3');});
    if(!hallFault) for(const name of [phasePairs[sector][0]+'H',phasePairs[sector][1]+'L']) {
      document.querySelector(`[data-switch="${name}"]`)?.setAttribute('stroke','#0d9488');
      document.querySelector(`[data-switch="${name}"]`)?.setAttribute('stroke-width','7');
    }
    $('m02-hall-result').textContent=hallFault?'Injected 000: invalid for this 120° convention. All six simulated commands OFF. Reset before continuing.':
      `Sector ${sector+1} · H1H2H3=${hallCodes[sector]} · ${phasePairs[sector][0]}+ / ${phasePairs[sector][1]}− · other phase floating. ${pairs} pole pairs at ${rpm} RPM: ${(pairs*rpm/60).toFixed(2)} electrical Hz; ${rpm? (1000/(6*pairs*rpm/60)).toFixed(4)+' ms/sector':'stopped; no finite edge period'}.`;
  }
  function pwm() {
    const d=number('m02-duty',0,100,50)/100,f=number('m02-frequency',100,20000,1000),v=number('m02-bus',0,12,6);
    const data=[];
    for(let i=0;i<=600;i++){let t=i/200;data.push([t,((t%1)<d)?3.3:0]);}
    plot('m02-pwm-plot',[{label:'STM32 logic output',color:'#0891b2',data}],{xmax:3,ymax:3.6,xlabel:'Time / PWM period (3 periods)',ylabel:'Logic voltage (V)'});
    $('m02-pwm-result').textContent=`Period ${(1e6/f).toFixed(1)} µs; high ${(1e6*d/f).toFixed(1)} µs; duty ${(d*100).toFixed(0)}%. Ideal winding average: +V/0 scheme ${(d*v).toFixed(2)} V; bipolar ±V scheme ${((2*d-1)*v).toFixed(2)} V. Bus ${v} V; GPIO pulse amplitude remains 3.3 V. These are voltage averages, not RPM predictions.`;
  }
  function bridge() {
    const mode=$('m02-bridge-mode').value;
    const states={forward:['AH','BL'],reverse:['BH','AL'],brake:['AL','BL'],coast:[]};
    document.querySelectorAll('[data-bridge-switch]').forEach(e=>{const on=states[mode].includes(e.dataset.bridgeSwitch);e.setAttribute('stroke',on?'#0d9488':'#94a3b8');e.setAttribute('stroke-width',on?'7':'3');});
    const text={forward:'AH and BL ON: supply → AH → motor A-to-B → BL → ground.',reverse:'BH and AL ON: supply → BH → motor B-to-A → AL → ground.',brake:'Both low switches ON: both motor ends near ground. Existing current circulates and decays; a spinning rotor can generate braking current.',coast:'All switches commanded OFF. Winding current initially commutates into body diodes; it does not instantly vanish. After current decays the shaft coasts.'};
    $('m02-bridge-result').textContent=text[mode]+' Never enable the high and low switch in the same leg together.';
  }
  function encoder() {
    const c=number('m02-counts',4,10000,80),rpm=number('m02-encoder-rpm',0,6000,600),win=number('m02-window',10,1000,100)/1000;
    const rate=c*rpm/60,n=rate*win,edge=rate?1/rate:0;
    $('m02-encoder-result').textContent=rpm?`Expected count ${n.toFixed(3)} per window. Neighboring integer counts ${Math.floor(n)} / ${Math.ceil(n)} → ${(60*Math.floor(n)/c/win).toFixed(3)} / ${(60*Math.ceil(n)/c/win).toFixed(3)} RPM. T method: ${(edge*1e6).toFixed(1)} µs per x4 edge → ${rpm.toFixed(3)} RPM. A 1 µs timer has about ${(100*1e-6/edge).toFixed(3)}% single-tick relative period quantization. Stop timeout must exceed expected edge interval.`:'No edges: report stopped only after your declared timeout; 1/T is undefined at standstill.';
    const q=[[0,0],[1,0],[1,1],[0,1]],a=[],b=[];
    for(let i=0;i<8;i++){a.push([i,q[i%4][0]],[i+0.999,q[i%4][0]]);b.push([i,q[i%4][1]-1.5],[i+.999,q[i%4][1]-1.5]);}
    plot('m02-encoder-plot',[{label:'A (upper track)',color:'#2563eb',data:a},{label:'B (lower, offset)',color:'#0d9488',data:b}],{xmax:8,ymin:-1.7,ymax:1.2,xlabel:'Count intervals (forward: 00 → 10 → 11 → 01)',ylabel:'Logic tracks, arbitrary vertical offset'});
  }
  function model() {
    const tau=number('m02-tau',.02,2,.2),u=number('m02-model-input',.1,1,.5),step=[];
    for(let i=0;i<=200;i++){const t=i*5*tau/200;step.push([t,u*(1-Math.exp(-t/tau))]);}
    plot('m02-step-plot',[{label:'Output y (normalized)',color:'#0891b2',data:step}],{xmax:5*tau,ymax:1,xlabel:'Time (s)',ylabel:'Normalized speed'});
    const f=number('m02-sine-hz',.01,20,1/(2*Math.PI*.2)),w=2*Math.PI*f,gain=1/Math.sqrt(1+(w*tau)**2),phase=-Math.atan(w*tau),input=[],output=[];
    for(let i=0;i<=300;i++){const t=i*3/f/300;input.push([t,.5+.2*Math.sin(w*t)]);output.push([t,.5+.2*gain*Math.sin(w*t+phase)]);}
    plot('m02-sine-plot',[{label:'Input: 0.5 + 0.2 sin(ωt)',color:'#6366f1',data:input},{label:'Steady sine response',color:'#0891b2',data:output}],{xmax:3/f,ymin:.2,ymax:.8,xlabel:'Time (s)',ylabel:'Normalized value'});
    $('m02-model-result').textContent=`At τ=${tau} s: step output ${(u*(1-Math.exp(-1))).toFixed(4)} (63.2% of ${u}). Corner ${(1/(2*Math.PI*tau)).toFixed(4)} Hz. At ${f.toFixed(4)} Hz: amplitude ratio ${gain.toFixed(4)}, ${(20*Math.log10(gain)).toFixed(3)} dB, phase ${(phase*180/Math.PI).toFixed(2)}°. Sine plot shows steady state; startup transient omitted.`;
  }
  function margins(kp,ki,delay,tau=.2) {
    if(kp===0&&ki===0) return {wc:null,pm:null,gm:null};
    const x=(-(1-kp*kp)+Math.sqrt((1-kp*kp)**2+4*tau*tau*ki*ki))/(2*tau*tau);
    const wc=x>0?Math.sqrt(x):null;
    const phase=w=>-Math.atan2(ki/w,kp)-Math.atan(tau*w)-w*delay;
    const mag=w=>Math.hypot(kp,ki/w)/Math.hypot(1,tau*w);
    let wp=null;
    if(delay>0){let a=1e-5;for(let b=a*1.1;b<1e6;b*=1.1){if(phase(b)<=-Math.PI){for(let j=0;j<50;j++){const m=(a+b)/2;if(phase(m)>-Math.PI)a=m;else b=m;}wp=(a+b)/2;break;}a=b;}}
    return {wc,wp,pm:wc?(Math.PI+phase(wc))*180/Math.PI:null,gm:wp?-20*Math.log10(mag(wp)):delay===0?Infinity:null};
  }
  function simulate({kp=1,ki=5,delay=0,anti=true,scenario='load',dt=.001}={}) {
    const n=Math.round(8/dt),lag=Math.round(delay/dt),history=new Array(n+1).fill(0),a=Math.exp(-dt/.2);
    let y=0,I=0;const speed=[],ref=[],drive=[],integral=[];
    for(let k=0;k<=n;k++){
      const t=k*dt,r=scenario==='saturation'?(t<3?1.2:.5):.5,d=scenario==='load'&&t>=3?.15:0,e=r-y;
      const candidate=I+ki*dt*e,raw=kp*e+candidate;
      if(!anti||!((raw>1&&e>0)||(raw<0&&e<0))) I=candidate;
      const u=clamp(kp*e+I,0,1);history[k]=u;
      if(k%10===0){speed.push([t,y]);ref.push([t,r]);drive.push([t,u]);integral.push([t,I]);}
      const delayed=k>=lag?history[k-lag]:0;
      y=a*y+(1-a)*(delayed-d);
    }
    return {speed,ref,drive,integral,final:y,I};
  }
  function pi() {
    const kp=number('m02-kp',0,10,1),ki=number('m02-ki',0,100,5),delay=number('m02-delay',0,.5,0),anti=$('m02-antiwindup').checked,scenario=$('m02-scenario').value;
    const r=simulate({kp,ki,delay,anti,scenario}),ints=r.integral.map(v=>v[1]);
    plot('m02-pi-speed',[{label:'Reference',color:'#6366f1',data:r.ref,dash:'6 4'},{label:'Speed y',color:'#0891b2',data:r.speed}],{xmax:8,ymin:-.3,ymax:1.4,ylabel:'Normalized reference / speed'});
    plot('m02-pi-drive',[{label:'Limited drive u',color:'#0d9488',data:r.drive},{label:'Integral state I',color:'#d97706',data:r.integral}],{xmax:8,ymin:Math.min(-.1,...ints),ymax:Math.max(1.2,...ints),ylabel:'Drive / integral contribution'});
    const m=margins(kp,ki,delay);
    $('m02-pi-result').textContent=`Sampled simulation: Ts=1 ms, input delay=${delay} s; y(8 s)≈${r.final.toFixed(4)}, I≈${r.I.toFixed(4)}. Continuous unsaturated model: crossover ${m.wc===null?'none':m.wc.toFixed(3)+' rad/s'}; PM ${m.pm===null?'undefined':m.pm.toFixed(2)+'°'}; GM ${m.gm===Infinity?'∞ (ideal model)':m.gm===null?'undefined':m.gm.toFixed(3)+' dB'}. Margins describe the linear loop, not saturation recovery.`;
    const mag=[],phase=[];
    for(let i=0;i<=220;i++){const w=Math.pow(10,-1+3*i/220),amp=Math.hypot(kp,ki/w)/Math.hypot(1,.2*w);mag.push([w,amp?20*Math.log10(amp):-100]);phase.push([w,(-Math.atan2(ki/w,kp)-Math.atan(.2*w)-w*delay)*180/Math.PI]);}
    const gainSeries=[{label:'Loop |C·G| in dB',color:'#2563eb',data:mag},{label:'0 dB reference',color:'#64748b',data:[[.1,0],[100,0]],dash:'5 4'}];
    if(m.wc>=.1&&m.wc<=100) gainSeries.push({label:'Gain crossover',color:'#d97706',data:[[m.wc,-50],[m.wc,60]],dash:'3 5'});
    const phaseMin=Math.min(-270,phase.at(-1)[1]);
    const phaseSeries=[{label:'Loop phase (unwrapped)',color:'#0d9488',data:phase},{label:'−180° reference',color:'#64748b',data:[[.1,-180],[100,-180]],dash:'5 4'}];
    if(m.wp>=.1&&m.wp<=100) phaseSeries.push({label:'Phase crossover',color:'#d97706',data:[[m.wp,phaseMin],[m.wp,0]],dash:'3 5'});
    plot('m02-bode-gain',gainSeries,{xmin:.1,xmax:100,ymin:-50,ymax:60,xlabel:'Angular frequency ω (rad/s), logarithmic',ylabel:'Gain (dB)',log:true});
    plot('m02-bode-phase',phaseSeries,{xmin:.1,xmax:100,ymin:phaseMin,ymax:0,xlabel:'Angular frequency ω (rad/s), logarithmic',ylabel:'Phase (degrees)',log:true});
  }
  function current() {
    const i=number('m02-current',0,3,.3),r=number('m02-shunt',.01,1,.1),g=number('m02-gain',1,100,20),off=number('m02-offset',0,3.3,0),v=i*r,out=off+g*v;
    $('m02-current-result').textContent=`Vsh=${(v*1000).toFixed(2)} mV; shunt heat=${(i*i*r).toFixed(4)} W; ideal amplifier=${out.toFixed(3)} V. ADC endpoint estimate=${Math.round(clamp(out,0,3.3)/3.3*4095)} / 4095. ${out>3.3?'CLIPPED: reduce gain/shunt or use appropriate scaling.':'Inside nominal ADC voltage span; real amplifier output headroom must also be checked.'} This is a calculation, not a live ADC reading.`;
  }
  function ripple() {
    const f=number('m02-ripple-hz',500,20000,1000),dt=1/f/100,R=2,L=.002,E=2,a=Math.exp(-R*dt/L);let i=.5;const volts=[],amps=[];
    for(let k=0;k<20300;k++){const on=k%100<50,v=on?6:0;i=Math.max(0,a*i+(1-a)*(v-E)/R);if(k>=20000){const t=(k-20000)*dt*1000;volts.push([t,v]);amps.push([t,i]);}}
    plot('m02-ripple-voltage',[{label:'Ideal winding voltage',color:'#6366f1',data:volts}],{xmax:300*dt*1000,ymax:7,xlabel:'Time (ms)',ylabel:'Voltage (V)'});
    plot('m02-ripple-current',[{label:'Winding current',color:'#0891b2',data:amps}],{xmax:300*dt*1000,ymax:1.1,xlabel:'Time (ms)',ylabel:'Current (A)'});
    const values=amps.map(p=>p[1]);$('m02-ripple-result').textContent=`Synthetic RL winding: R=2 Ω, L=2 mH, fixed back EMF=2 V; 6 V bus, 50% duty, ideal 0 V recirculation. Peak-to-peak current ${(Math.max(...values)-Math.min(...values)).toFixed(4)} A at ${f} Hz. No switching losses, commutation or cogging included.`;
  }
  const updates={pwm,bridge,hall,encoder,model,pi,current,ripple};
  document.querySelectorAll('[data-update]').forEach(el=>el.addEventListener('change',()=>updates[el.dataset.update]()));
  $('m02-hall-next').addEventListener('click',()=>{if(!hallFault)sector=(sector+1)%6;hall();});
  $('m02-hall-back').addEventListener('click',()=>{if(!hallFault)sector=(sector+5)%6;hall();});
  $('m02-hall-fault').addEventListener('click',()=>{hallFault=true;hall();});
  $('m02-hall-reset').addEventListener('click',()=>{hallFault=false;sector=0;hall();});
  document.querySelectorAll('[data-preset]').forEach(el=>el.addEventListener('click',()=>{
    const p={p:[1,0,0,'load'],pi:[1,5,0,'load'],delay:[1,5,.1,'load'],unstable:[1,5,.35,'load'],windup:[1,5,0,'saturation']}[el.dataset.preset];
    ['m02-kp','m02-ki','m02-delay','m02-scenario'].forEach((id,i)=>$(id).value=p[i]);pi();
  }));
  Object.values(updates).forEach(fn=>fn());
  const key='stm32-breadboard-module2-v1';let saved={},storageOK=true;
  try {saved=JSON.parse(localStorage.getItem(key)||'{}');if(!saved||typeof saved!=='object'||Array.isArray(saved))saved={};}catch{storageOK=false;}
  const checks=[...document.querySelectorAll('[data-progress]')],notes=[...document.querySelectorAll('[data-note]')];
  function status(){const n=checks.filter(e=>e.checked).length;$('m02-progress').value=n;$('m02-progress').max=checks.length;$('m02-progress-text').textContent=`${n} / ${checks.length} checkpoints`;$('m02-storage').textContent=storageOK?'Notes save in this browser. Export before moving files.':'Browser storage unavailable. Export your notebook.';}
  function save(){try{localStorage.setItem(key,JSON.stringify(saved));}catch{storageOK=false;}status();}
  checks.forEach(e=>{e.checked=saved[e.id]===true;e.addEventListener('change',()=>{saved[e.id]=e.checked;save();});});
  notes.forEach(e=>{e.value=typeof saved[e.id]==='string'?saved[e.id]:'';e.addEventListener('input',()=>{saved[e.id]=e.value;save();});});status();
  $('m02-export').addEventListener('click',()=>{const lines=['Module 2 notebook',new Date().toISOString(),...checks.map(e=>`${e.checked?'[x]':'[ ]'} ${e.parentElement.textContent.trim()}`),...notes.map(e=>`${e.labels[0].textContent}\n${e.value||'(No notes)'}`)];const url=URL.createObjectURL(new Blob([lines.join('\n\n')],{type:'text/plain;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download='module-02-notebook.txt';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
  $('m02-print').addEventListener('click',()=>{document.querySelectorAll('details').forEach(e=>e.open=true);window.print();});
  window.M02=Object.freeze({margins,simulate,hallCodes,phasePairs});
})();
