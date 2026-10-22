if (typeof window.activeTank === 'undefined') window.activeTank = 0;
if (typeof window.curView === 'undefined') window.curView = 1;
if (typeof window.lastData === 'undefined') window.lastData = null;
if (typeof window.playSfx === 'undefined') window.playSfx = function(){};
if (typeof window.update3DShip === 'undefined') window.update3DShip = function(){};

const tripNames = ['OK', 'TRIP: DRY-RUN (<35A)', 'TRIP: OVERLOAD (>185A)', 'TRIP: NPSH CAVITATION'];
const gShort = ['C3 PROPANE', 'C4 BUTANE', 'NH3 AMMONIA'];

// --- VIEW 1-4: DECLUTTERED CARGO TANK MIMIC (NO FLOATING TEXT CLUTTER INSIDE TANK) ---
function renderTankView(d) {
  const i = activeTank, t = d.ct[i], dp = t.dp, ds = t.ds, vr = t.vr;
  const marvsLift = d.man.marvs[i];
  document.getElementById('tkTitle').innerHTML =
    `CT${i+1} [${gShort[t.g]}] &mdash; Clean Mimic View (See <b>OPS DATA</b> tab for full Temp/Pressure telemetry)`;

  const pH = (t.p / 100) * 130, sH = (t.s / 100) * 130;
  const pCol = dp[0] ? (dp[5] ? '#ff0' : '#0f0') : (dp[6]>0 ? '#f33' : '#666');
  const sCol = ds[0] ? (ds[5] ? '#ff0' : '#0f0') : (ds[6]>0 ? '#f33' : '#666');
  const flowCol = t.lact ? '#0ff' : '#444';

  document.getElementById('tkSvg').innerHTML = `
    <line x1="80" y1="2" x2="80" y2="52" stroke="${t.vsuc?'#f66':'#444'}" stroke-width="3" stroke-dasharray="4,2"/>
    <rect x="48" y="5" width="64" height="15" rx="2" fill="${t.vsuc?'#711':'#333'}" stroke="#f88" onclick="cmd('vsuc=${i}','valve_toggle')"/>
    <text x="80" y="15" fill="#fff" font-size="7.5" text-anchor="middle" onclick="cmd('vsuc=${i}','valve_toggle')">VAP SUC:${t.vsuc?'OPEN':'SHUT'}</text>
    <line x1="175" y1="2" x2="175" y2="52" stroke="${marvsLift?'#f33':'#555'}" stroke-width="3"/>
    <rect x="132" y="5" width="86" height="15" rx="2" fill="${marvsLift?'#a00':'#222'}" stroke="#fa0"/>
    <text x="175" y="15" fill="#fff" font-size="7" text-anchor="middle">MARVS: ${marvsLift?'LIFTED!':'25.0 kPa'}</text>
    <line x1="275" y1="2" x2="275" y2="24" stroke="${t.cin?'#0ff':'#444'}" stroke-width="3"/>
    <rect x="240" y="5" width="70" height="15" rx="2" fill="${t.cin?'#066':'#333'}" stroke="#0ff" onclick="cmd('cin=${i}','valve_toggle')"/>
    <text x="275" y="15" fill="#fff" font-size="7.5" text-anchor="middle" onclick="cmd('cin=${i}','valve_toggle')">COND IN:${t.cin?'OPEN':'SHUT'}</text>
    <line x1="58" y1="24" x2="322" y2="24" stroke="${flowCol}" stroke-width="2"/>
    <polygon points="75,52 188,52 188,202 65,202 45,178 45,82" fill="#06101e" stroke="#28f" stroke-width="2"/>
    <polygon points="192,52 305,52 335,82 335,178 315,202 192,202" fill="#06101e" stroke="#28f" stroke-width="2"/>
    <rect x="48" y="${201-pH}" width="139" height="${pH}" fill="rgba(0,255,170,0.42)"/>
    <rect x="193" y="${201-sH}" width="139" height="${sH}" fill="rgba(0,210,255,0.42)"/>
    <line x1="190" y1="52" x2="190" y2="180" stroke="#aaa" stroke-width="3"/>
    <rect x="174" y="181" width="32" height="16" rx="3" fill="${t.bhd?'#082':'#811'}" stroke="#fff" onclick="cmd('bhd=${i}','valve_toggle')"/>
    <text x="190" y="192" fill="#fff" font-size="8" text-anchor="middle" onclick="cmd('bhd=${i}','valve_toggle')">${t.bhd?'OPEN':'SHUT'}</text>
    <line x1="150" y1="32" x2="150" y2="196" stroke="${pCol}" stroke-width="4"/>
    <circle cx="150" cy="196" r="7" fill="${pCol}" stroke="#fff" onclick="cmd('pstart=${i}&side=0','pump_start')"/>
    <line x1="230" y1="32" x2="230" y2="196" stroke="${sCol}" stroke-width="4"/>
    <circle cx="230" cy="196" r="7" fill="${sCol}" stroke="#fff" onclick="cmd('pstart=${i}&side=1','pump_start')"/>
    <line x1="115" y1="24" x2="115" y2="68" stroke="${vr[0]?flowCol:'#444'}" stroke-width="2"/>
    <line x1="85" y1="24" x2="85" y2="140" stroke="${vr[1]?flowCol:'#444'}" stroke-width="2"/>
    <line x1="58" y1="24" x2="58" y2="195" stroke="${vr[2]?(t.lact?'#fa0':'#650'):'#444'}" stroke-width="2"/>
    <line x1="265" y1="24" x2="265" y2="68" stroke="${vr[3]?flowCol:'#444'}" stroke-width="2"/>
    <line x1="295" y1="24" x2="295" y2="140" stroke="${vr[4]?flowCol:'#444'}" stroke-width="2"/>
    <line x1="322" y1="24" x2="322" y2="195" stroke="${vr[5]?(t.lact?'#fa0':'#650'):'#444'}" stroke-width="2"/>`;

  document.getElementById('tkControls').innerHTML = `
    <div class="card">
      <div class="row">
        <b>CT${i+1} SYSTEM &amp; GRADE:</b>
        <button class="sm ${t.g===0?'open':''}" onclick="cmd('grade=${i}&g=0')">C3</button>
        <button class="sm ${t.g===1?'open':''}" onclick="cmd('grade=${i}&g=1')">C4</button>
        <button class="sm ${t.g===2?'open':''}" onclick="cmd('grade=${i}&g=2')">NH3</button>
        <button class="sm" onclick="cmd('sys=${i}')">SYS ${t.sys+1}</button>
        <button class="sm open" onclick="setView(12)">VIEW OPS TELEMETRY</button>
      </div>
      <div class="row">
        <b>FILLING VALVE (${t.fv}% | ${t.lq} m&sup3;/h):</b>
        <button class="sm ${t.fv>5?'open':'shut'}" onclick="cmd('fvlv=${i}')">${t.fv>5?'SHUT':'100%'}</button>
        <input type="range" min="0" max="100" step="5" value="${t.fv}" onchange="cmd('fvlv=${i}&v='+this.value)">
        <button class="sm ${t.vsuc?'open':'shut'}" onclick="cmd('vsuc=${i}')">VAP:${t.vsuc?'OPEN':'SHUT'}</button>
        <button class="sm ${t.cin?'open':'shut'}" onclick="cmd('cin=${i}')">COND:${t.cin?'OPEN':'SHUT'}</button>
      </div>
      <div class="row"><b>PORT RETURN:</b>
        <button class="sm ${vr[0]?'open':'shut'}" onclick="cmd('vret=${i}&side=0&br=0')">P-TOP SPRAY</button>
        <button class="sm ${vr[1]?'open':'shut'}" onclick="cmd('vret=${i}&side=0&br=1')">P-BOT SPRAY</button>
        <button class="sm ${vr[2]?'open':'shut'}" onclick="cmd('vret=${i}&side=0&br=2')">P-SPARGE</button>
      </div>
      <div class="row"><b>STBD RETURN:</b>
        <button class="sm ${vr[3]?'open':'shut'}" onclick="cmd('vret=${i}&side=1&br=0')">S-TOP SPRAY</button>
        <button class="sm ${vr[4]?'open':'shut'}" onclick="cmd('vret=${i}&side=1&br=1')">S-BOT SPRAY</button>
        <button class="sm ${vr[5]?'open':'shut'}" onclick="cmd('vret=${i}&side=1&br=2')">S-SPARGE</button>
      </div>
      <div class="row">
        <span>Port(${t.p}%):</span><input type="range" min="0" max="98" step="0.5" value="${t.p}" onchange="cmd('ct=${i}&side=0&v='+this.value)">
        <button class="sm ${t.bhd?'open':'shut'}" onclick="cmd('bhd=${i}')">BHD:${t.bhd?'OPEN':'SHUT'}</button>
        <span>Stbd(${t.s}%):</span><input type="range" min="0" max="98" step="0.5" value="${t.s}" onchange="cmd('ct=${i}&side=1&v='+this.value)">
      </div>
    </div>`;
}

function renderHeadersView(d) {
  const m = d.man; let branches = '';
  for (let i = 0; i < 4; i++) {
    const x = 58 + i * 86, t = d.ct[i], venting = m.mast[i] > 1.0;
    const vy = t.sys === 0 ? 55 : 80, cy = t.sys === 0 ? 120 : 145;
    branches += `
      <line x1="${x+18}" y1="8" x2="${x+18}" y2="170" stroke="${venting?'#f44':'#555'}" stroke-width="${venting?3:1.5}"/>
      <rect x="${x-6}" y="2" width="48" height="16" rx="2" fill="${venting?'#a00':'#123'}" stroke="${venting?'#ff0':'#08a'}"/>
      <text x="${x+18}" y="13" fill="#fff" font-size="7" text-anchor="middle">MAST#${i+1}:${m.mast[i]}kg/h</text>
      <rect x="${x-32}" y="170" width="64" height="56" rx="4" fill="#08182c" stroke="#28f" onclick="setView(${i+1})"/>
      <text x="${x}" y="185" fill="#fff" font-size="9" font-weight="bold" text-anchor="middle">CT${i+1} (S${t.sys+1})</text>
      <text x="${x}" y="198" fill="#0ff" font-size="8" text-anchor="middle">${t.pr} kPa</text>
      <text x="${x}" y="211" fill="${t.lact?'#0f0':'#fa0'}" font-size="7.5" text-anchor="middle">${t.lact?'SPRAY ON':'IDLE'}</text>
      <line x1="${x-12}" y1="${vy}" x2="${x-12}" y2="170" stroke="${t.vsuc?'#f55':'#444'}" stroke-width="2.5" stroke-dasharray="3,2"/>
      <line x1="${x+2}" y1="${cy}" x2="${x+2}" y2="170" stroke="${t.lact?'#0f0':'#444'}" stroke-width="2.5"/>`;
  }
  document.getElementById('hdrSvg').innerHTML = `
    <text x="10" y="45" fill="#f88" font-size="8.5" font-weight="bold">VAPOUR HDR S1 [PT:${m.vpt[0]}bar TT:${m.vtt[0]}&deg;C] | S2 [PT:${m.vpt[1]}bar TT:${m.vtt[1]}&deg;C]</text>
    <text x="10" y="110" fill="#0ff" font-size="8.5" font-weight="bold">COND HDR S1 [PT:${m.cpt[0]}bar TT:${m.ctt[0]}&deg;C] | S2 [PT:${m.cpt[1]}bar TT:${m.ctt[1]}&deg;C]</text>
    <line x1="15" y1="55" x2="365" y2="55" stroke="#f55" stroke-width="3"/><line x1="15" y1="80" x2="365" y2="80" stroke="#fa5" stroke-width="3"/>
    <line x1="190" y1="55" x2="190" y2="80" stroke="${d.xo[0]?'#0f0':'#555'}" stroke-width="3"/>
    <line x1="15" y1="120" x2="365" y2="120" stroke="#0ff" stroke-width="3"/><line x1="15" y1="145" x2="365" y2="145" stroke="#08f" stroke-width="3"/>
    <line x1="190" y1="120" x2="190" y2="145" stroke="${d.xo[1]?'#0f0':'#555'}" stroke-width="3"/>${branches}`;
  document.getElementById('hdrControls').innerHTML = `<div class="card"><div class="row"><b>HEADER CROSSOVER VALVES:</b>
    <button class="sm ${d.xo[0]?'open':'shut'}" onclick="cmd('xo=0')">VAPOUR X-OVER</button>
    <button class="sm ${d.xo[1]?'open':'shut'}" onclick="cmd('xo=1')">CONDENSATE X-OVER</button>
    <button class="sm ${d.xo[2]?'open':'shut'}" onclick="cmd('xo=2')">LIQUID X-OVER</button></div></div>`;
}
function renderPumpsView(d) {
  const m = d.man, s1Open = m.sc[0] && m.ev[0] && (m.gv[0] > 1), s2Open = m.sc[1] && m.ev[1] && (m.gv[1] > 1);
  const totCargo = (d.ct[0].mt + d.ct[1].mt + d.ct[2].mt + d.ct[3].mt).toFixed(1);
  document.getElementById('draftBanner').innerHTML = `<div class="row"><b>DRAFTS:</b> <span>Tf:<b>${d.tf}m</b> Tm:<b>${d.tm}m</b> Ta:<b>${d.ta}m</b></span> | <span>CARGO: <b style="color:#0f0">${totCargo} MT</b></span> | <span>LIQ S1: <b>${m.lpt[0]} bar</b> | S2: <b>${m.lpt[1]} bar</b></span></div>`;
  let svgPumps = '', ctrlRows = '';
  for (let i = 0; i < 4; i++) {
    const x = 68 + i * 68, t = d.ct[i], dp = t.dp, ds = t.ds, ly = t.sys === 0 ? 44 : 74;
    const pCol = dp[0] ? (dp[5] ? '#ff0' : '#0f0') : (dp[6]>0 ? '#f33' : '#555'), sCol = ds[0] ? (ds[5] ? '#ff0' : '#0f0') : (ds[6]>0 ? '#f33' : '#555');
    svgPumps += `<rect x="${x-28}" y="118" width="56" height="84" rx="4" fill="#071526" stroke="#28f"/>
      <text x="${x}" y="131" fill="#fff" font-size="8" font-weight="bold" text-anchor="middle">CT${i+1}</text>
      <line x1="${x-12}" y1="${ly}" x2="${x-12}" y2="184" stroke="${pCol}" stroke-width="2.5"/><circle cx="${x-12}" cy="184" r="6" fill="${pCol}" onclick="cmd('pstart=${i}&side=0','pump_start')"/>
      <line x1="${x+12}" y1="${ly}" x2="${x+12}" y2="184" stroke="${sCol}" stroke-width="2.5"/><circle cx="${x+12}" cy="184" r="6" fill="${sCol}" onclick="cmd('pstart=${i}&side=1','pump_start')"/>`;
    ctrlRows += `<div class="card"><div class="row"><b>CT${i+1} (${t.mt}t | ${t.pr} kPa):</b><span>Fill(${t.fv}% -> ${t.lq}m&sup3;/h):</span><input type="range" min="0" max="100" step="5" value="${t.fv}" onchange="cmd('fvlv=${i}&v='+this.value)"></div>
      <div class="row"><button class="sm ${dp[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=0','pump_start')">P:${dp[0]?'RUN':'STOP'}</button><input type="range" min="5" max="100" value="${dp[1]}" onchange="cmd('pthr=${i}&side=0&v='+this.value)"><span>${dp[2]}m&sup3;/h|${dp[3]}bar|${dp[4]}A</span>
      <button class="sm ${ds[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=1','pump_start')">S:${ds[0]?'RUN':'STOP'}</button><input type="range" min="5" max="100" value="${ds[1]}" onchange="cmd('pthr=${i}&side=1&v='+this.value)"><span>${ds[2]}m&sup3;/h|${ds[3]}bar|${ds[4]}A</span></div></div>`;
  }
  document.getElementById('pmpSvg').innerHTML = `<rect x="4" y="28" width="38" height="22" rx="2" fill="${s1Open?'#062':'#611'}" stroke="#fff" onclick="cmd('sconn=0')"/><text x="23" y="42" fill="#fff" font-size="7" text-anchor="middle">${m.sc[0]?'S1 CONN':'S1 DISC'}</text>
    <rect x="4" y="58" width="38" height="22" rx="2" fill="${s2Open?'#062':'#611'}" stroke="#fff" onclick="cmd('sconn=1')"/><text x="23" y="72" fill="#fff" font-size="7" text-anchor="middle">${m.sc[1]?'S2 CONN':'S2 DISC'}</text>
    <text x="48" y="20" fill="#0fa" font-size="8" font-weight="bold">LIQ S1: BackP=${m.lpt[0]} bar (${m.q[0]} m&sup3;/h | Gate:${m.gv[0]}%)</text>
    <text x="48" y="96" fill="#0af" font-size="8" font-weight="bold">LIQ S2: BackP=${m.lpt[1]} bar (${m.q[1]} m&sup3;/h | Gate:${m.gv[1]}%)</text>
    <line x1="42" y1="44" x2="310" y2="44" stroke="${s1Open?'#0fa':'#f44'}" stroke-width="3.5"/><line x1="42" y1="74" x2="310" y2="74" stroke="${s2Open?'#0af':'#f44'}" stroke-width="3.5"/>
    <rect x="312" y="26" width="62" height="34" rx="4" fill="${m.htr?'#730':'#081828'}" stroke="#fa0" onclick="cmd('htr=1')"/><text x="343" y="41" fill="#fff" font-size="7.5" text-anchor="middle">SW HEATER</text><text x="343" y="53" fill="#fa0" font-size="7.5" text-anchor="middle">${m.et}&deg;C</text>
    <rect x="312" y="64" width="62" height="34" rx="4" fill="${m.bst?'#062':'#081828'}" stroke="#0f0" onclick="cmd('bst=1','pump_start')"/><text x="343" y="79" fill="#fff" font-size="7.5" text-anchor="middle">BOOSTER</text><text x="343" y="91" fill="#0f0" font-size="7.5" text-anchor="middle">${m.eb} bar</text>${svgPumps}`;
  document.getElementById('pmpControls').innerHTML = `<div class="card">
    <div class="row"><b>SYS 1:</b><button class="sm ${m.sc[0]?'open':'shut'}" onclick="cmd('sconn=0')">ARM</button><button class="sm ${m.ev[0]?'open':'shut'}" onclick="cmd('mesd=0')">ESD</button><span>Gate(${m.gv[0]}%):</span><input type="range" min="0" max="100" step="5" value="${m.gv[0]}" onchange="cmd('mgate=0&v='+this.value)"><button class="sm ${m.op[0]===1?'open':''}" onclick="cmd('mop=0&m=1')">DISCH</button><button class="sm ${m.op[0]===2?'hot':''}" onclick="cmd('mop=0&m=2')">LOAD</button></div>
    <div class="row"><b>SYS 2:</b><button class="sm ${m.sc[1]?'open':'shut'}" onclick="cmd('sconn=1')">ARM</button><button class="sm ${m.ev[1]?'open':'shut'}" onclick="cmd('mesd=1')">ESD</button><span>Gate(${m.gv[1]}%):</span><input type="range" min="0" max="100" step="5" value="${m.gv[1]}" onchange="cmd('mgate=1&v='+this.value)"><button class="sm ${m.op[1]===1?'open':''}" onclick="cmd('mop=1&m=1')">DISCH</button><button class="sm ${m.op[1]===2?'hot':''}" onclick="cmd('mop=1&m=2')">LOAD</button></div></div>${ctrlRows}`;
}

function renderCompView(d) {
  let units = '', cards = ''; const mNames = ['STOPPED', 'RELIQ MODE', 'HOT GAS'];
  for (let c = 0; c < 4; c++) {
    const x = 52 + c * 90, m = d.comp[c][0], sys = d.comp[c][1], eff = d.comp[c][2], cbar = d.comp[c][3];
    const loadPct = d.comp[c][4] !== undefined ? d.comp[c][4] : (m>0?100:0);
    const sucKpa = d.comp[c][5] !== undefined ? d.comp[c][5] : d.ct[sys*2].pr;
    const col = m === 1 ? (loadPct > 0 ? '#0f8' : '#ff0') : (m === 2 ? '#fa0' : '#555');
    units += `<rect x="${x-34}" y="24" width="68" height="66" rx="5" fill="#091a2e" stroke="${col}" stroke-width="2"/>
      <text x="${x}" y="37" fill="#fff" font-size="8" font-weight="bold" text-anchor="middle">COMP #${c+1} (S${sys+1})</text>
      <text x="${x}" y="49" fill="${col}" font-size="7.5" text-anchor="middle">${m>0 && loadPct===0 ? 'AUTO CUTOUT' : mNames[m]}</text>
      <text x="${x}" y="61" fill="#0ff" font-size="7" text-anchor="middle">Suc:${sucKpa}kPa | Load:${loadPct}%</text>
      <text x="${x}" y="73" fill="#fa0" font-size="7" text-anchor="middle">Disch:${cbar}bar | SW:${eff}%</text>
      <line x1="${x}" y1="90" x2="${x}" y2="124" stroke="${col}" stroke-width="3"/><circle cx="${x}" cy="105" r="5" fill="#ff0"/>
      <rect x="${x-26}" y="124" width="52" height="38" rx="4" fill="#071320" stroke="#08a"/>
      <text x="${x}" y="140" fill="#0ff" font-size="8" text-anchor="middle">MOTOR #${c+1}</text>
      <text x="${x}" y="153" fill="#8f8" font-size="8" text-anchor="middle">${Math.round(142*(loadPct/100))}A</text>`;
    cards += `<div class="card"><div><b>COMP #${c+1} (S${sys+1} | Suc:${sucKpa} kPa | Disch:${cbar} bar | Load:${loadPct}%)</b></div>
      <div class="row" style="margin-top:4px;"><button class="sm ${m===0?'shut':''}" onclick="cmd('comp=${c}&m=0')">STOP</button><button class="sm ${m===1?'open':''}" onclick="cmd('comp=${c}&m=1','pump_start')">RELIQ</button><button class="sm ${m===2?'hot':''}" onclick="cmd('comp=${c}&m=2','pump_start')">HOT GAS</button><button class="sm" onclick="cmd('csys=${c}')">SYS ${sys+1}</button></div></div>`;
  }
  document.getElementById('compSvg').innerHTML = `<text x="190" y="15" fill="#0ff" font-size="8.5" font-weight="bold" text-anchor="middle">COMPRESSOR ROOM | AIR: ${d.air}&deg;C | SEAWATER: ${d.sw}&deg;C (Low Suction Cutout: 1.5 kPa)</text><line x1="10" y1="105" x2="370" y2="105" stroke="#f44" stroke-width="3" stroke-dasharray="6,3"/><text x="190" y="182" fill="#0ff" font-size="8.5" font-weight="bold" text-anchor="middle">ELECTRIC MOTOR ROOM (+dP: ${d.mdp} Pa)</text>${units}`;
  document.getElementById('compControls').innerHTML = `<div class="card" style="grid-column: span 2; border-color:#0ff;"><div class="row"><b>AMBIENT AIR TEMP (${d.air}&deg;C):</b><input type="range" min="-10" max="45" step="1" value="${d.air}" onchange="cmd('air='+this.value)"></div><div class="row"><b>SEAWATER TEMP (${d.sw}&deg;C):</b><input type="range" min="0" max="36" step="1" value="${d.sw}" onchange="cmd('sw='+this.value)"></div></div>` + cards;
}

// --- VIEW 12: DEDICATED OPS DATA BULLET PANEL (CARGO TANKS 1-4 & HOLD SPACES 1-4) ---
function renderOpsDataView(d) {
  const el = document.getElementById('opsDataContent'); if (!el) return;
  const tFloors = [-41.7, -0.1, -32.9];
  let ctHtml = '', hsHtml = '';
  for (let i = 0; i < 4; i++) {
    const t = d.ct[i], z = d.fgd[i], hTemp = t.ht !== undefined ? t.ht : (0.6*d.sw + 0.2*d.air + 0.2*t.bs).toFixed(1);
    ctHtml += `<div class="card" style="border-color:#0af">
      <b style="color:#0ff;font-size:12px">CARGO TANK #${i+1} (${gShort[t.g]} &mdash; SYS ${t.sys+1})</b>
      <ul style="margin:6px 0 2px 18px;padding:0;font-size:11px;line-height:1.55;color:#ddd">
        <li><b>Dome Pressure:</b> <span style="color:${t.pr>22?'#f44':'#0f0'}">${t.pr} kPaG</span> (MARVS Setpoint: 25.0 kPaG)</li>
        <li><b>Liquid / Vapour Temp:</b> <b>${t.tl}&deg;C</b> / <b>${t.tv}&deg;C</b> (Comp Cooling Floor: ${tFloors[t.g]}&deg;C)</li>
        <li><b>Shell Top / Bottom Temp:</b> <b>${t.ts}&deg;C</b> / <b>${t.bs}&deg;C</b> (Rate: ${t.cr}&deg;C/hr)</li>
        <li><b>Port / Stbd Fill Level:</b> <b>${t.p}%</b> / <b>${t.s}%</b> (IGC Max Limit: ${t.fl}%)</li>
        <li><b>Cargo Mass &amp; Density:</b> <b>${t.mt} MT</b> (&rho; = ${t.rho} kg/m&sup3;) | Centerline BHD: <b>${t.bhd?'OPEN':'SHUT'}</b></li>
        <li><b>Tank Atmosphere:</b> HC: <b>${t.hc}%</b> | Inert Gas: <b>${t.ig}%</b> | O2: <b>${t.o2}%</b></li>
      </ul></div>`;
    hsHtml += `<div class="card" style="border-color:#fa0">
      <b style="color:#fa0;font-size:12px">HOLD SPACE #${i+1} (Secondary Barrier Around CT${i+1})</b>
      <ul style="margin:6px 0 2px 18px;padding:0;font-size:11px;line-height:1.55;color:#ddd">
        <li><b>Hold Space Pressure:</b> <b style="color:#0f0">${t.hpr} kPaG</b></li>
        <li><b>Hold Space Temperature:</b> <b>${hTemp}&deg;C</b></li>
        <li><b>Gas Detection:</b> HC: <b>${z[0]}% LEL</b> | O2: <b>${z[1]}%</b> | NH3: <b>${z[2]} ppm</b></li>
        <li><b>Barrier Status:</b> <span style="color:${z[4]===0?'#0f0':'#f44'}">${z[4]===0?'NORMAL (NO LEAK)':'GAS ALARM ACTIVE'}</span></li>
      </ul></div>`;
  }
  el.innerHTML = `<div style="margin-bottom:6px;color:#0ff;font-weight:bold">CARGO TANKS 1&ndash;4 TELEMETRY</div><div class="grid2">${ctHtml}</div>
    <div style="margin:10px 0 6px;color:#fa0;font-weight:bold">HOLD SPACES 1&ndash;4 TELEMETRY</div><div class="grid2">${hsHtml}</div>`;
}
function renderBallastView(d) {
  const pair = d.wtgt, b = d.bal, pPct = d.wbt[pair*2][0], sPct = d.wbt[pair*2+1][0], pH = (pPct/100)*110, sH = (sPct/100)*110, nRun = b.pr[0]+b.pr[1];
  const modeCap = b.op===5 ? 'GRAVITATE SEA' : (nRun===0 ? 'STOPPED' : (nRun===1 ? 'SINGULAR (1250 m\u00b3/h)' : 'PARALLEL (2320 m\u00b3/h)'));
  document.getElementById('wbtSvg').innerHTML = `<rect x="20" y="${130-pH}" width="125" height="${pH}" fill="rgba(0,150,255,0.6)"/><rect x="150" y="${130-sH}" width="125" height="${sH}" fill="rgba(0,150,255,0.6)"/>
    <rect x="20" y="20" width="125" height="110" fill="none" stroke="#0cf" stroke-width="2"/><rect x="150" y="20" width="125" height="110" fill="none" stroke="#0cf" stroke-width="2"/>
    <text x="82" y="122" fill="#fff" font-size="8" text-anchor="middle">PORT:${pPct}% (${d.wbt[pair*2][3]}m&sup3;/h)</text><text x="212" y="122" fill="#fff" font-size="8" text-anchor="middle">STBD:${sPct}% (${d.wbt[pair*2+1][3]}m&sup3;/h)</text>
    <rect x="282" y="22" width="92" height="46" rx="4" fill="#071828" stroke="${b.pr[0]?'#0f0':'#555'}" stroke-width="2" onclick="cmd('bpr=0','pump_start')"/><text x="328" y="38" fill="#fff" font-size="8" text-anchor="middle">WBT PUMP #1</text><text x="328" y="52" fill="#0f0" font-size="8" text-anchor="middle">${b.pq[0]}m&sup3;/h | ${b.pb[0]}bar</text>
    <rect x="282" y="80" width="92" height="46" rx="4" fill="#071828" stroke="${b.pr[1]?'#0f0':'#555'}" stroke-width="2" onclick="cmd('bpr=1','pump_start')"/><text x="328" y="96" fill="#fff" font-size="8" text-anchor="middle">WBT PUMP #2</text><text x="328" y="110" fill="#0f0" font-size="8" text-anchor="middle">${b.pq[1]}m&sup3;/h | ${b.pb[1]}bar</text>
    <text x="190" y="168" fill="#0ff" font-size="8" font-weight="bold" text-anchor="middle">BALLAST FLOW: ${b.q} m&sup3;/hr [${modeCap}]</text>`;
  const wNames = ['WBT1P','WBT1S','WBT2P','WBT2S','WBT3P','WBT3S','WBT4P','WBT4S','FPK','APT']; let rows = '';
  for (let j = 0; j < 10; j += 2) {
    const wP = d.wbt[j], wS = d.wbt[j+1];
    rows += `<div class="row"><button class="sm ${wP[2]>5?'open':'shut'}" onclick="cmd('bvlv=${j}')">${wNames[j]}:${wP[2]}%</button><input type="range" min="0" max="100" step="10" value="${wP[2]}" onchange="cmd('bvlv=${j}&v='+this.value)"><span>${wP[0]}%</span><span>${wS[0]}%</span><input type="range" min="0" max="100" step="10" value="${wS[2]}" onchange="cmd('bvlv=${j+1}&v='+this.value)"><button class="sm ${wS[2]>5?'open':'shut'}" onclick="cmd('bvlv=${j+1}')">${wNames[j+1]}:${wS[2]}%</button></div>`;
  }
  document.getElementById('wbtControls').innerHTML = `<div class="card"><div class="row"><button class="sm ${b.pr[0]?'open':'shut'}" onclick="cmd('bpr=0','pump_start')">P1:${b.pr[0]?'RUN':'STOP'}</button><button class="sm ${b.pr[1]?'open':'shut'}" onclick="cmd('bpr=1','pump_start')">P2:${b.pr[1]?'RUN':'STOP'}</button><button class="sm ${b.op===1?'open':''}" onclick="cmd('bop=1')">BALLAST IN</button><button class="sm ${b.op===2?'open':''}" onclick="cmd('bop=2')">DEBALLAST</button><button class="sm ${b.op===5?'open':''}" onclick="cmd('bop=5')">GRAVITATE</button><button class="sm ${b.op===0?'shut':''}" onclick="cmd('bop=0')">HOLD</button></div>
    <div class="row"><button class="sm ${pair===0?'open':''}" onclick="cmd('wtgt=0')">WBT1</button><button class="sm ${pair===1?'open':''}" onclick="cmd('wtgt=1')">WBT2</button><button class="sm ${pair===2?'open':''}" onclick="cmd('wtgt=2')">WBT3</button><button class="sm ${pair===3?'open':''}" onclick="cmd('wtgt=3')">WBT4</button><button class="sm ${pair===4?'open':''}" onclick="cmd('wtgt=4')">FPK/APT</button><span>P:</span><input type="range" min="0" max="100" value="${pPct}" onchange="cmd('wbt=${pair*2}&v='+this.value)"><span>S:</span><input type="range" min="0" max="100" value="${sPct}" onchange="cmd('wbt=${pair*2+1}&v='+this.value)"></div>${rows}</div>`;
}

function renderSafetyView(d) {
  const zNames = ['HOLD SPACE 1', 'HOLD SPACE 2', 'HOLD SPACE 3', 'HOLD SPACE 4', 'COMPRESSOR ROOM', 'E-MOTOR ROOM']; let rows = '';
  d.fgd.forEach((z, idx) => {
    const col = z[4] === 2 ? '#f33' : (z[4] === 1 ? '#fa0' : '#0f8');
    rows += `<div class="card" style="border-color:${col}"><div class="row"><b style="color:${col}">${zNames[idx]}</b><span>HC:<b>${z[0]}%LEL</b> | O2:<b>${z[1]}%</b> | NH3:<b>${z[2]}ppm</b></span><button class="sm ${z[3]?'shut':'open'}" onclick="cmd('leak=${idx}','alarm')">${z[3]?'STOP LEAK':'SIMULATE LEAK'}</button></div></div>`;
  });
  document.getElementById('fgdControls').innerHTML = `<div class="card" style="border-color:${d.esd?'#f22':'#0aa'}"><div class="row"><b style="color:${d.esd?'#ff0':'#0f0'}">ESD-1: ${d.esd ? 'TRIPPED!' : 'ARMED'}</b><span>Cause: <b>${d.cause}</b></span></div><div class="row"><button class="sm shut" onclick="cmd('esd=1','alarm_buzzer')">MANUAL ESD-1</button><button class="sm open" onclick="cmd('esd=0','success')">RESET ESD-1</button></div></div>${rows}`;
}

function drawStationCurve(canvasId, arr, peakVal) {
  const cv = document.getElementById(canvasId); if (!cv || !arr) return;
  const cx = cv.getContext('2d'), w = cv.width, h = cv.height, midY = h / 2, limY = (100 / 140) * (midY - 6);
  cx.clearRect(0, 0, w, h); cx.strokeStyle = '#622'; cx.setLineDash([4, 3]);
  cx.beginPath(); cx.moveTo(0, midY - limY); cx.lineTo(w, midY - limY); cx.moveTo(0, midY + limY); cx.lineTo(w, midY + limY); cx.stroke();
  cx.setLineDash([]); cx.strokeStyle = Math.abs(peakVal) > 100 ? '#ff3355' : '#00ffaa'; cx.lineWidth = 2.2; cx.beginPath();
  arr.forEach((v, s) => { const x = (s / 19) * (w - 20) + 10, y = midY - (v / 140) * (midY - 6); s === 0 ? cx.moveTo(x, y) : cx.lineTo(x, y); });
  cx.stroke();
}

function renderLoadicatorView(d) {
  const totCargo = (d.ct[0].mt + d.ct[1].mt + d.ct[2].mt + d.ct[3].mt).toFixed(1);
  const heelStr = d.heel < -0.05 ? `${Math.abs(d.heel).toFixed(2)}&deg; Port` : (d.heel > 0.05 ? `${d.heel.toFixed(2)}&deg; Stbd` : '0.00&deg; Upright');
  document.getElementById('loadTables').innerHTML = `<div class="grid2"><div class="card"><b>DRAFTS &amp; DISPLACEMENT</b><div class="row"><span>Disp / Cargo:</span><b>${d.disp} MT / ${totCargo} MT</b></div><div class="row"><span>Tf / Tm / Ta:</span><b>${d.tf}m / ${d.tm}m / ${d.ta}m</b></div><div class="row"><span>Trim / TPC / MCTC:</span><b>${d.trim}m | ${d.tpc} | ${d.mctc}</b></div></div>
    <div class="card"><b>STABILITY &amp; STRENGTH</b><div class="row"><span>KMt / KG / FSC:</span><b>${d.kmt}m / ${d.kg}m / -${d.fsc}m</b></div><div class="row"><span>Fluid GM0 / List:</span><b style="color:#0f0">${d.gm}m | ${heelStr}</b></div><div class="row"><span>Seagoing SWSF / SWBM:</span><b>${d.sf}% / ${d.bm}%</b></div></div></div>`;
  drawStationCurve('bmCurveCv', d.bm_c, d.bm); drawStationCurve('sfCurveCv', d.sf_c, d.sf);
}

function renderPresetsView(d) {
  let html = `<div class="card"><div class="row"><b>SNAPSHOT STATE:</b><button class="sm open" onclick="cmd('preset=-1','success')">+ SAVE SNAPSHOT</button></div></div><div class="grid2">`;
  d.presets.forEach((p, idx) => { html += `<div class="card"><div style="color:#fa0;font-size:9px">[${p.c}]</div><div style="margin:4px 0;font-weight:bold">${p.n}</div><button class="sm open" style="width:100%" onclick="cmd('preset=${idx}','success')">LOAD PRESET</button></div>`; });
  document.getElementById('presetList').innerHTML = html + `</div>`;
}

window.renderActiveView = function(d) {
  try {
    if (curView >= 1 && curView <= 4) renderTankView(d); else if (curView === 5) renderHeadersView(d);
    else if (curView === 6) renderPumpsView(d); else if (curView === 7) renderCompView(d);
    else if (curView === 8) renderBallastView(d); else if (curView === 9) renderSafetyView(d);
    else if (curView === 10) renderLoadicatorView(d); else if (curView === 11) renderPresetsView(d);
    else if (curView === 12) renderOpsDataView(d);
  } catch (e) { console.error(e); }
};

async function sync(q = '', sfx = '') {
  try {
    if (sfx) playSfx(sfx); else if (q) playSfx('valve_toggle');
    let d;
    try { const r = await fetch('/state' + (q ? '?' + q : '')); d = await r.json(); }
    catch (e) { d = window.vlgcStepLocal ? window.vlgcStepLocal(q) : lastData; }
    if (!d) return;
    lastData = d;
    try { if (typeof update3DShip === 'function') update3DShip(d); } catch (e) {}
    const heelStr = d.heel < -0.05 ? `${Math.abs(d.heel).toFixed(1)}&deg;P` : (d.heel > 0.05 ? `${d.heel.toFixed(1)}&deg;S` : '0.0&deg;');
    document.getElementById('hudTop').innerHTML = `<b>${d.spd}x</b> | <b>Tf:${d.tf}m Tm:${d.tm}m Ta:${d.ta}m</b> | List:<b>${heelStr}</b> | Air:${d.air}&deg;C SW:${d.sw}&deg;C | ${d.esd?'<span style="color:#f44">[ESD-1]</span>':'OK'}`;
    window.renderActiveView(d);
  } catch (err) {}
}
function cmd(q, sfx = '') { sync(q, sfx); }
setInterval(() => sync(), 300); sync();
// --- SELF-HEALING STAGE 2 DOM & 3D OVERLAP FIX ---
(function initStage2Layout() {
  const st = document.createElement('style');
  st.textContent = `
    #v3d, #view0, .view3d-wrap { position: relative !important; margin-top: 6px !important; }
    #hud3d, .hud3d, #v3dInfo, #threeHud, #v3d > div:first-child {
      top: 12px !important; margin-top: 4px !important; z-index: 5 !important;
      background: rgba(4, 12, 24, 0.85) !important; padding: 6px 10px !important; border-radius: 4px;
    }
  `;
  document.head.appendChild(st);

  function ensureOpsDom() {
    if (!document.getElementById('opsDataContent')) {
      const refView = document.getElementById('presetList') ? document.getElementById('presetList').parentElement : document.body;
      const v12 = document.createElement('div');
      v12.id = 'view12'; v12.className = refView.className || 'view';
      v12.style.display = 'none'; v12.style.padding = '8px';
      v12.innerHTML = '<div id="opsDataContent"></div>';
      refView.parentNode.appendChild(v12);
    }
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', ensureOpsDom);
  else ensureOpsDom();

  const origSetView = window.setView;
  window.setView = function(v) {
    ensureOpsDom();
    const v12 = document.getElementById('view12');
    const t12 = document.getElementById('tab12');
    if (v === 12) {
      window.curView = 12;
      document.querySelectorAll('.view, [id^="view"], #v3d, #vTank, #vHdr, #vPmp, #vComp, #vWbt, #vFgd, #vLoad, #vPre').forEach(el => {
        if (el.id !== 'view12') el.style.display = 'none';
      });
      document.querySelectorAll('.tab, nav button').forEach(b => b.classList.remove('active'));
      if (t12) t12.classList.add('active');
      if (v12) v12.style.display = 'block';
      if (window.lastData) renderOpsDataView(window.lastData);
    } else {
      if (v12) v12.style.display = 'none';
      if (t12) t12.classList.remove('active');
      if (typeof origSetView === 'function') origSetView(v);
      window.curView = v;
      if (window.lastData) window.renderActiveView(window.lastData);
    }
  };
})();
