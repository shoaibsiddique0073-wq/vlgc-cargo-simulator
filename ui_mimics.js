const tripNames = ['OK', 'TRIP: DRY-RUN (<35A)', 'TRIP: OVERLOAD (>185A)', 'TRIP: NPSH CAVITATION'];
const gShort = ['C3 PROPANE', 'C4 BUTANE', 'NH3 AMMONIA'];

// --- VIEW 1-4: INSIDE CARGO TANK MIMIC (MASS-CONSERVING + AIR/SW HEAT INGRESS) ---
function renderTankView(d) {
  const i = activeTank, t = d.ct[i], dp = t.dp, ds = t.ds, vr = t.vr;
  const marvsLift = d.man.marvs[i];
  document.getElementById('tkTitle').innerHTML =
    `CT${i+1} [${gShort[t.g]}] | Cargo Mass: <b>${t.mt} MT</b> (ρ=${t.rho} kg/m³) | Dome: <span style="color:${t.pr>220?'#f44':'#0f0'}">${t.pr} mbarg</span> ${marvsLift?'🚨MARVS':''} | MaxFL:${t.fl}%`;

  const pH = (t.p / 100) * 130, sH = (t.s / 100) * 130;
  const pCol = dp[0] ? (dp[5] ? '#ff0' : '#0f0') : (dp[6]>0 ? '#f33' : '#666');
  const sCol = ds[0] ? (ds[5] ? '#ff0' : '#0f0') : (ds[6]>0 ? '#f33' : '#666');
  const flowCol = t.lact ? '#0ff' : '#444';

  document.getElementById('tkSvg').innerHTML = `
    <line x1="80" y1="2" x2="80" y2="52" stroke="${t.vsuc?'#f66':'#444'}" stroke-width="3" stroke-dasharray="4,2"/>
    <rect x="48" y="5" width="64" height="15" rx="2" fill="${t.vsuc?'#711':'#333'}" stroke="#f88" onclick="cmd('vsuc=${i}','valve_toggle')"/>
    <text x="80" y="15" fill="#fff" font-size="7.5" text-anchor="middle" onclick="cmd('vsuc=${i}','valve_toggle')">VAP SUC:${t.vsuc?'OPEN':'SHUT'}</text>

    <line x1="175" y1="2" x2="175" y2="52" stroke="${marvsLift?'#f33':'#555'}" stroke-width="3"/>
    <rect x="138" y="5" width="74" height="15" rx="2" fill="${marvsLift?'#a00':'#222'}" stroke="#fa0"/>
    <text x="175" y="15" fill="#fff" font-size="7" text-anchor="middle">MARVS->MAST#${i+1}: ${marvsLift?'LIFTED!':'250mbar'}</text>

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
    <line x1="322" y1="24" x2="322" y2="195" stroke="${vr[5]?(t.lact?'#fa0':'#650'):'#444'}" stroke-width="2"/>
    ${(t.lact && vr[0]) ? '<path d="M115,68 L95,92 M115,68 L115,95 M115,68 L135,92" stroke="#0ff" stroke-dasharray="3,2"/>' : ''}
    ${(t.lact && vr[3]) ? '<path d="M265,68 L245,92 M265,68 L265,95 M265,68 L285,92" stroke="#0ff" stroke-dasharray="3,2"/>' : ''}

    <g class="info-tag">
      <text x="190" y="45" fill="${t.lact?'#0f0':'#fa0'}" font-size="8" font-weight="bold" text-anchor="middle">
        ${t.lact ? '⚡ SPRAY/SPARGE ACTIVE (dT/dt: '+t.cr+'°C/hr | Air:'+d.air+'°C SW:'+d.sw+'°C)' : '⚠️ NO FLOW: ALIGN VAP SUC + COMP + COND IN + SPRAY/SPARGE'}
      </text>
      <text x="115" y="108" fill="#ff0" font-size="8.5" text-anchor="middle">TT-TOP: ${t.ts}°C</text>
      <text x="115" y="122" fill="#fff" font-size="8.5" text-anchor="middle">TT-VAP: ${t.tv}°C</text>
      <text x="265" y="108" fill="#0f8" font-size="8.5" text-anchor="middle">HC:${t.hc}% IG:${t.ig}% O2:${t.o2}%</text>
      <text x="115" y="162" fill="#0ff" font-size="10" font-weight="bold" text-anchor="middle">PORT: ${t.p}%</text>
      <text x="265" y="162" fill="#0ff" font-size="10" font-weight="bold" text-anchor="middle">STBD: ${t.s}%</text>
      <text x="190" y="222" fill="#fa0" font-size="8.5" text-anchor="middle">TT-BOT: ${t.bs}°C | TT-LIQ: ${t.tl}°C | P:${tripNames[dp[6]]}</text>
    </g>`;

  document.getElementById('tkControls').innerHTML = `
    <div class="card">
      <div class="row">
        <b>GRADE IN CT${i+1} (${t.mt} MT Conserved):</b>
        <button class="sm ${t.g===0?'open':''}" onclick="cmd('grade=${i}&g=0')">C3 PROPANE</button>
        <button class="sm ${t.g===1?'open':''}" onclick="cmd('grade=${i}&g=1')">C4 BUTANE</button>
        <button class="sm ${t.g===2?'open':''}" onclick="cmd('grade=${i}&g=2')">NH3 AMMONIA</button>
        <button class="sm" onclick="cmd('sys=${i}')">SYS ${t.sys+1}</button>
      </div>
      <div class="row">
        <b>DOME LINEUP:</b>
        <button class="sm ${t.vsuc?'open':'shut'}" onclick="cmd('vsuc=${i}')">VAP SUC:${t.vsuc?'OPEN':'SHUT'}</button>
        <button class="sm ${t.cin?'open':'shut'}" onclick="cmd('cin=${i}')">COND IN:${t.cin?'OPEN':'SHUT'}</button>
        <button class="sm ${t.fv?'open':'shut'}" onclick="cmd('fvlv=${i}')">LIQ FILL:${t.fv?'OPEN':'SHUT'}</button>
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

// --- VIEW 5: VAPOUR/CONDENSATE HEADERS & MAST RISERS ---
function renderHeadersView(d) {
  const m = d.man;
  let branches = '';
  for (let i = 0; i < 4; i++) {
    const x = 58 + i * 86, t = d.ct[i], venting = m.mast[i] > 1.0;
    const vy = t.sys === 0 ? 55 : 80, cy = t.sys === 0 ? 120 : 145;
    branches += `
      <line x1="${x+18}" y1="8" x2="${x+18}" y2="170" stroke="${venting?'#f44':'#555'}" stroke-width="${venting?3:1.5}"/>
      <rect x="${x-6}" y="2" width="48" height="16" rx="2" fill="${venting?'#a00':'#123'}" stroke="${venting?'#ff0':'#08a'}"/>
      <text x="${x+18}" y="13" fill="#fff" font-size="7" text-anchor="middle">MAST#${i+1}:${m.mast[i]}kg/h</text>
      <rect x="${x-32}" y="170" width="64" height="56" rx="4" fill="#08182c" stroke="#28f" onclick="setView(${i+1})"/>
      <text x="${x}" y="185" fill="#fff" font-size="9" font-weight="bold" text-anchor="middle">CT${i+1} (S${t.sys+1})</text>
      <text x="${x}" y="198" fill="#0ff" font-size="8" text-anchor="middle">${t.mt} MT | ${t.pr}mbar</text>
      <text x="${x}" y="211" fill="${t.lact?'#0f0':'#fa0'}" font-size="7.5" text-anchor="middle">${t.lact?'FLOW ON':'NO FLOW'}</text>
      <line x1="${x-12}" y1="${vy}" x2="${x-12}" y2="170" stroke="${t.vsuc?'#f55':'#444'}" stroke-width="2.5" stroke-dasharray="3,2"/>
      <line x1="${x+2}" y1="${cy}" x2="${x+2}" y2="170" stroke="${t.lact?'#0f0':'#444'}" stroke-width="2.5"/>`;
  }
  document.getElementById('hdrSvg').innerHTML = `
    <g class="info-tag">
      <text x="10" y="45" fill="#f88" font-size="8.5" font-weight="bold">VAPOUR HDR S1 [PT:${m.vpt[0]}bar TT:${m.vtt[0]}°C] | S2 [PT:${m.vpt[1]}bar TT:${m.vtt[1]}°C]</text>
      <text x="10" y="110" fill="#0ff" font-size="8.5" font-weight="bold">COND HDR S1 [PT:${m.cpt[0]}bar TT:${m.ctt[0]}°C] | S2 [PT:${m.cpt[1]}bar TT:${m.ctt[1]}°C]</text>
    </g>
    <line x1="15" y1="55" x2="365" y2="55" stroke="#f55" stroke-width="3"/>
    <line x1="15" y1="80" x2="365" y2="80" stroke="#fa5" stroke-width="3"/>
    <line x1="190" y1="55" x2="190" y2="80" stroke="${d.xo[0]?'#0f0':'#555'}" stroke-width="3"/>
    <line x1="15" y1="120" x2="365" y2="120" stroke="#0ff" stroke-width="3"/>
    <line x1="15" y1="145" x2="365" y2="145" stroke="#08f" stroke-width="3"/>
    <line x1="190" y1="120" x2="190" y2="145" stroke="${d.xo[1]?'#0f0':'#555'}" stroke-width="3"/>
    ${branches}`;
  document.getElementById('hdrControls').innerHTML = `
    <div class="card">
      <div class="row"><b>HEADER CROSSOVER VALVES:</b>
        <button class="sm ${d.xo[0]?'open':'shut'}" onclick="cmd('xo=0')">VAPOUR X-OVER</button>
        <button class="sm ${d.xo[1]?'open':'shut'}" onclick="cmd('xo=1')">CONDENSATE X-OVER</button>
        <button class="sm ${d.xo[2]?'open':'shut'}" onclick="cmd('xo=2')">LIQUID X-OVER</button>
      </div>
    </div>`;
}

// --- VIEW 6: SHORE MANIFOLD FLANGES, PT/TT, TRV & 8 DEEPWELL PUMPS ---
function renderPumpsView(d) {
  const m = d.man;
  const s1Open = m.sc[0] && m.ev[0] && m.gv[0], s2Open = m.sc[1] && m.ev[1] && m.gv[1];
  const totCargo = (d.ct[0].mt + d.ct[1].mt + d.ct[2].mt + d.ct[3].mt).toFixed(1);
  document.getElementById('draftBanner').innerHTML =
    `<div class="row"><b>DRAFTS:</b> <span>Tf:<b>${d.tf}m</b> Tm:<b>${d.tm}m</b> Ta:<b>${d.ta}m</b></span> | ` +
    `<span>TOTAL CARGO: <b style="color:#0f0">${totCargo} MT</b></span> | ` +
    `<span>LIQ S1: <b>${m.lpt[0]}bar / ${m.ltt[0]}°C</b> ${m.trv[0]?'<span style="color:#f44">🚨TRV->MAST#1</span>':''}</span></div>`;

  let svgPumps = '', ctrlRows = '';
  for (let i = 0; i < 4; i++) {
    const x = 68 + i * 68, t = d.ct[i], dp = t.dp, ds = t.ds;
    const ly = t.sys === 0 ? 44 : 74;
    const pCol = dp[0] ? (dp[5] ? '#ff0' : '#0f0') : (dp[6]>0 ? '#f33' : '#555');
    const sCol = ds[0] ? (ds[5] ? '#ff0' : '#0f0') : (ds[6]>0 ? '#f33' : '#555');

    svgPumps += `
      <rect x="${x-28}" y="118" width="56" height="84" rx="4" fill="#071526" stroke="#28f"/>
      <text x="${x}" y="131" fill="#fff" font-size="8" font-weight="bold" text-anchor="middle">CT${i+1}(${gShort[t.g].split(' ')[0]})</text>
      <text x="${x}" y="142" fill="#fa0" font-size="7" class="info-tag" text-anchor="middle">${t.mt}t | ${t.fl}%</text>
      <line x1="${x-12}" y1="${ly}" x2="${x-12}" y2="184" stroke="${pCol}" stroke-width="2.5"/>
      <circle cx="${x-12}" cy="184" r="6" fill="${pCol}" onclick="cmd('pstart=${i}&side=0','pump_start')"/>
      <line x1="${x+12}" y1="${ly}" x2="${x+12}" y2="184" stroke="${sCol}" stroke-width="2.5"/>
      <circle cx="${x+12}" cy="184" r="6" fill="${sCol}" onclick="cmd('pstart=${i}&side=1','pump_start')"/>`;

    ctrlRows += `<div class="card">
      <div class="row">
        <b>CT${i+1} (${t.mt} MT):</b>
        <button class="sm ${t.g===0?'open':''}" onclick="cmd('grade=${i}&g=0')">C3</button>
        <button class="sm ${t.g===1?'open':''}" onclick="cmd('grade=${i}&g=1')">C4</button>
        <button class="sm ${t.g===2?'open':''}" onclick="cmd('grade=${i}&g=2')">NH3</button>
        <button class="sm ${t.fv?'open':'shut'}" onclick="cmd('fvlv=${i}')">FILL VLV:${t.fv?'OPEN':'SHUT'}</button>
      </div>
      <div class="row">
        <button class="sm ${dp[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=0','pump_start')">P:${dp[0]?'RUN':'STOP'}</button>
        <input type="range" min="5" max="100" value="${dp[1]}" onchange="cmd('pthr=${i}&side=0&v='+this.value)">
        <span>${dp[2]}m³/h|${dp[4]}A</span>
        <button class="sm ${ds[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=1','pump_start')">S:${ds[0]?'RUN':'STOP'}</button>
        <input type="range" min="5" max="100" value="${ds[1]}" onchange="cmd('pthr=${i}&side=1&v='+this.value)">
        <span>${ds[2]}m³/h|${ds[4]}A</span>
      </div>
    </div>`;
  }

  document.getElementById('pmpSvg').innerHTML = `
    <rect x="4" y="28" width="38" height="22" rx="2" fill="${s1Open?'#062':'#611'}" stroke="#fff" onclick="cmd('sconn=0')"/>
    <text x="23" y="42" fill="#fff" font-size="7" text-anchor="middle">${m.sc[0]?'S1 CONN':'S1 DISC'}</text>
    <rect x="4" y="58" width="38" height="22" rx="2" fill="${s2Open?'#062':'#611'}" stroke="#fff" onclick="cmd('sconn=1')"/>
    <text x="23" y="72" fill="#fff" font-size="7" text-anchor="middle">${m.sc[1]?'S2 CONN':'S2 DISC'}</text>
    <g class="info-tag">
      <text x="48" y="20" fill="#0fa" font-size="8" font-weight="bold">LIQ S1: PT=${m.lpt[0]}bar TT=${m.ltt[0]}°C (${m.q[0]}m³/h) ${m.trv[0]?'[TRV LIFT->MAST#1!]':''}</text>
      <text x="48" y="96" fill="#0af" font-size="8" font-weight="bold">LIQ S2: PT=${m.lpt[1]}bar TT=${m.ltt[1]}°C (${m.q[1]}m³/h) ${m.trv[1]?'[TRV LIFT->MAST#2!]':''}</text>
    </g>
    <line x1="42" y1="44" x2="310" y2="44" stroke="${s1Open?'#0fa':'#f44'}" stroke-width="3.5"/>
    <line x1="42" y1="74" x2="310" y2="74" stroke="${s2Open?'#0af':'#f44'}" stroke-width="3.5"/>
    <rect x="312" y="26" width="62" height="34" rx="4" fill="${m.htr?'#730':'#081828'}" stroke="#fa0" onclick="cmd('htr=1')"/>
    <text x="343" y="41" fill="#fff" font-size="7.5" text-anchor="middle">SW HEATER</text>
    <text x="343" y="53" fill="#fa0" font-size="7.5" text-anchor="middle">${m.et}°C</text>
    <rect x="312" y="64" width="62" height="34" rx="4" fill="${m.bst?'#062':'#081828'}" stroke="#0f0" onclick="cmd('bst=1','pump_start')"/>
    <text x="343" y="79" fill="#fff" font-size="7.5" text-anchor="middle">BOOSTER</text>
    <text x="343" y="91" fill="#0f0" font-size="7.5" text-anchor="middle">${m.eb} bar</text>
    ${svgPumps}`;

  document.getElementById('pmpControls').innerHTML = `
    <div class="card">
      <div class="row">
        <b>SYS 1 MANIFOLD:</b>
        <button class="sm ${m.sc[0]?'open':'shut'}" onclick="cmd('sconn=0')">ARM:${m.sc[0]?'CONN':'DISC'}</button>
        <button class="sm ${m.ev[0]?'open':'shut'}" onclick="cmd('mesd=0')">ESD:${m.ev[0]?'OPEN':'SHUT'}</button>
        <button class="sm ${m.gv[0]?'open':'shut'}" onclick="cmd('mgate=0')">GATE:${m.gv[0]?'OPEN':'SHUT'}</button>
        <button class="sm ${m.op[0]===1?'open':''}" onclick="cmd('mop=0&m=1')">DISCH</button>
        <button class="sm ${m.op[0]===2?'hot':''}" onclick="cmd('mop=0&m=2')">LOAD</button>
      </div>
      <div class="row">
        <b>SYS 2 MANIFOLD:</b>
        <button class="sm ${m.sc[1]?'open':'shut'}" onclick="cmd('sconn=1')">ARM:${m.sc[1]?'CONN':'DISC'}</button>
        <button class="sm ${m.ev[1]?'open':'shut'}" onclick="cmd('mesd=1')">ESD:${m.ev[1]?'OPEN':'SHUT'}</button>
        <button class="sm ${m.gv[1]?'open':'shut'}" onclick="cmd('mgate=1')">GATE:${m.gv[1]?'OPEN':'SHUT'}</button>
        <button class="sm ${m.op[1]===1?'open':''}" onclick="cmd('mop=1&m=1')">DISCH</button>
        <button class="sm ${m.op[1]===2?'hot':''}" onclick="cmd('mop=1&m=2')">LOAD</button>
      </div>
    </div>${ctrlRows}`;
}

// --- VIEW 7: COMPRESSOR ROOM + AMBIENT AIR & SEAWATER COOLING EFFICIENCY ---
function renderCompView(d) {
  let units = '', cards = '';
  const mNames = ['STOPPED', 'RELIQ MODE', 'HOT GAS'];
  for (let c = 0; c < 4; c++) {
    const x = 52 + c * 90, m = d.comp[c][0], sys = d.comp[c][1], eff = d.comp[c][2], cbar = d.comp[c][3];
    const col = m === 1 ? '#0f8' : (m === 2 ? '#fa0' : '#555');
    units += `
      <rect x="${x-34}" y="26" width="68" height="62" rx="5" fill="#091a2e" stroke="${col}" stroke-width="2"/>
      <text x="${x}" y="40" fill="#fff" font-size="8.5" font-weight="bold" text-anchor="middle">COMP #${c+1} (S${sys+1})</text>
      <text x="${x}" y="52" fill="${col}" font-size="8" text-anchor="middle">${mNames[m]}</text>
      <text x="${x}" y="64" fill="#0ff" font-size="7.5" text-anchor="middle">SW Eff: ${eff}%</text>
      <text x="${x}" y="76" fill="#fa0" font-size="7.5" text-anchor="middle">Cond: ${cbar} bar</text>
      <line x1="${x}" y1="88" x2="${x}" y2="124" stroke="${col}" stroke-width="3"/>
      <circle cx="${x}" cy="105" r="5" fill="#ff0"/>
      <rect x="${x-26}" y="124" width="52" height="38" rx="4" fill="#071320" stroke="#08a"/>
      <text x="${x}" y="140" fill="#0ff" font-size="8" text-anchor="middle">MOTOR #${c+1}</text>
      <text x="${x}" y="153" fill="#8f8" font-size="8" text-anchor="middle">${m>0?'142A':'0A'}</text>`;
    cards += `<div class="card">
      <div><b>COMP #${c+1} (SYS ${sys+1} | Eff: ${eff}%)</b></div>
      <div class="row" style="margin-top:4px;">
        <button class="sm ${m===0?'shut':''}" onclick="cmd('comp=${c}&m=0')">STOP</button>
        <button class="sm ${m===1?'open':''}" onclick="cmd('comp=${c}&m=1','pump_start')">RELIQ</button>
        <button class="sm ${m===2?'hot':''}" onclick="cmd('comp=${c}&m=2','pump_start')">HOT GAS</button>
        <button class="sm" onclick="cmd('csys=${c}')">SYS ${sys+1}</button>
      </div>
    </div>`;
  }
  document.getElementById('compSvg').innerHTML = `
    <text x="190" y="15" fill="#0ff" font-size="8.5" font-weight="bold" text-anchor="middle">COMPRESSOR ROOM | AIR: ${d.air}°C | SEAWATER COOLING: ${d.sw}°C</text>
    <line x1="10" y1="105" x2="370" y2="105" stroke="#f44" stroke-width="3" stroke-dasharray="6,3"/>
    <text x="190" y="182" fill="#0ff" font-size="8.5" font-weight="bold" text-anchor="middle">ELECTRIC MOTOR ROOM (+dP: ${d.mdp} Pa)</text>${units}`;

  document.getElementById('compControls').innerHTML = `
    <div class="card" style="grid-column: span 2; border-color:#0ff;">
      <div class="row">
        <b>AMBIENT AIR TEMP (${d.air}°C — Deck Heat Ingress):</b>
        <input type="range" min="-10" max="45" step="1" value="${d.air}" onchange="cmd('air='+this.value)">
      </div>
      <div class="row">
        <b>SEAWATER TEMP (${d.sw}°C — Condenser Cooling Efficiency):</b>
        <input type="range" min="0" max="36" step="1" value="${d.sw}" onchange="cmd('sw='+this.value)">
      </div>
    </div>` + cards;
}

// --- VIEW 8: BALLAST PUMPS (SINGULAR vs PARALLEL), RING MAIN & 10 WBT BRANCH VALVES ---
function renderBallastView(d) {
  const pair = d.wtgt, b = d.bal;
  const pPct = d.wbt[pair*2][0], sPct = d.wbt[pair*2+1][0];
  const pH = (pPct / 100) * 110, sH = (sPct / 100) * 110;
  const nRun = b.pr[0] + b.pr[1];
  const modeCap = nRun === 0 ? '0 m³/h (STOPPED)' : (nRun === 1 ? 'SINGULAR (Max 1,250 m³/h)' : 'PARALLEL (Max 2,320 m³/h)');

  document.getElementById('wbtSvg').innerHTML = `
    <defs>
      <clipPath id="wP"><polygon points="20,20 75,20 75,54 44,54 44,102 145,102 145,130 20,130"/></clipPath>
      <clipPath id="wS"><polygon points="270,20 215,20 215,54 246,54 246,102 150,102 150,130 270,130"/></clipPath>
    </defs>
    <rect x="20" y="${130-pH}" width="130" height="${pH}" fill="rgba(0,150,255,0.65)" clip-path="url(#wP)"/>
    <rect x="148" y="${130-sH}" width="130" height="${sH}" fill="rgba(0,150,255,0.65)" clip-path="url(#wS)"/>
    <polygon points="20,20 75,20 75,54 44,54 44,102 145,102 145,130 20,130" fill="none" stroke="#0cf" stroke-width="2"/>
    <polygon points="270,20 215,20 215,54 246,54 246,102 150,102 150,130 270,130" fill="none" stroke="#0cf" stroke-width="2"/>
    <text x="82" y="122" fill="#fff" font-size="8.5" text-anchor="middle">PORT:${pPct}% (${d.wbt[pair*2][3]}m³/h)</text>
    <text x="208" y="122" fill="#fff" font-size="8.5" text-anchor="middle">STBD:${sPct}% (${d.wbt[pair*2+1][3]}m³/h)</text>

    <!-- Ballast Pump #1 & Pump #2 Singular/Parallel Schematic on Right -->
    <rect x="282" y="22" width="92" height="46" rx="4" fill="#071828" stroke="${b.pr[0]?'#0f0':'#555'}" stroke-width="2" onclick="cmd('bpr=0','pump_start')"/>
    <text x="328" y="36" fill="#fff" font-size="8" font-weight="bold" text-anchor="middle">WBT PUMP #1</text>
    <text x="328" y="48" fill="#0f0" font-size="8" text-anchor="middle">${b.pq[0]} m³/h | ${b.pb[0]}bar</text>
    <text x="328" y="60" fill="#fa0" font-size="7.5" text-anchor="middle">${b.pa[0]} Amps</text>

    <rect x="282" y="80" width="92" height="46" rx="4" fill="#071828" stroke="${b.pr[1]?'#0f0':'#555'}" stroke-width="2" onclick="cmd('bpr=1','pump_start')"/>
    <text x="328" y="94" fill="#fff" font-size="8" font-weight="bold" text-anchor="middle">WBT PUMP #2</text>
    <text x="328" y="106" fill="#0f0" font-size="8" text-anchor="middle">${b.pq[1]} m³/h | ${b.pb[1]}bar</text>
    <text x="328" y="118" fill="#fa0" font-size="7.5" text-anchor="middle">${b.pa[1]} Amps</text>

    <!-- Ring Main & Crossover Line -->
    <line x1="20" y1="152" x2="365" y2="152" stroke="${nRun>0?'#0ff':'#444'}" stroke-width="3"/>
    <text x="190" y="170" fill="#0ff" font-size="8" font-weight="bold" text-anchor="middle">TOTAL BALLAST LINE FLOW: ${b.q} m³/hr [${modeCap}]</text>
  `;

  const wNames = ['WBT1P','WBT1S','WBT2P','WBT2S','WBT3P','WBT3S','WBT4P','WBT4S','FPK','APT'];
  let rows = '';
  for (let j = 0; j < 10; j += 2) {
    const wP = d.wbt[j], wS = d.wbt[j+1];
    rows += `<div class="row">
      <button class="sm ${wP[2]?'open':'shut'}" onclick="cmd('bvlv=${j}')">${wNames[j]} VLV:${wP[2]?'OPEN':'SHUT'}</button>
      <span style="width:82px">${wP[0]}% (${wP[3]}m³/h)</span>
      <input type="range" min="0" max="100" value="${wP[0]}" onchange="cmd('wbt=${j}&v='+this.value)">
      <input type="range" min="0" max="100" value="${wS[0]}" onchange="cmd('wbt=${j+1}&v='+this.value)">
      <span style="width:82px;text-align:right">${wS[0]}% (${wS[3]}m³/h)</span>
      <button class="sm ${wS[2]?'open':'shut'}" onclick="cmd('bvlv=${j+1}')">${wNames[j+1]} VLV:${wS[2]?'OPEN':'SHUT'}</button>
    </div>`;
  }
  document.getElementById('wbtControls').innerHTML = `
    <div class="card">
      <div class="row">
        <b>PUMPS (1,250 m³/h each):</b>
        <button class="sm ${b.pr[0]?'open':'shut'}" onclick="cmd('bpr=0','pump_start')">PUMP #1:${b.pr[0]?'RUN':'STOP'}</button>
        <button class="sm ${b.pr[1]?'open':'shut'}" onclick="cmd('bpr=1','pump_start')">PUMP #2:${b.pr[1]?'RUN':'STOP'}</button>
        <button class="sm ${b.xo?'open':'shut'}" onclick="cmd('bxo=1')">RING X-OVER:${b.xo?'OPEN':'SHUT'}</button>
      </div>
      <div class="row">
        <b>LINEUP MODE:</b>
        <button class="sm ${b.op===1?'open':''}" onclick="cmd('bop=1')">BALLAST IN</button>
        <button class="sm ${b.op===2?'open':''}" onclick="cmd('bop=2')">DEBALLAST OUT</button>
        <button class="sm ${b.op===3?'hot':''}" onclick="cmd('bop=3')">HEEL P→S</button>
        <button class="sm ${b.op===4?'hot':''}" onclick="cmd('bop=4')">HEEL S→P</button>
        <button class="sm ${b.op===0?'shut':''}" onclick="cmd('bop=0')">HOLD</button>
      </div>
      <div class="row">
        <span>Inspect Section:</span>
        <button class="sm ${pair===0?'open':''}" onclick="cmd('wtgt=0')">WBT1</button>
        <button class="sm ${pair===1?'open':''}" onclick="cmd('wtgt=1')">WBT2</button>
        <button class="sm ${pair===2?'open':''}" onclick="cmd('wtgt=2')">WBT3</button>
        <button class="sm ${pair===3?'open':''}" onclick="cmd('wtgt=3')">WBT4</button>
        <button class="sm ${pair===4?'open':''}" onclick="cmd('wtgt=4')">FPK/APT</button>
      </div>
      ${rows}
    </div>`;
}

// --- VIEW 9: FGD & ESD-1 SAFETY MATRIX ---
function renderSafetyView(d) {
  const zNames = ['HOLD SPACE 1', 'HOLD SPACE 2', 'HOLD SPACE 3', 'HOLD SPACE 4', 'COMPRESSOR ROOM', 'E-MOTOR ROOM'];
  let rows = '';
  d.fgd.forEach((z, idx) => {
    const col = z[4] === 2 ? '#f33' : (z[4] === 1 ? '#fa0' : '#0f8');
    rows += `<div class="card" style="border-color:${col}">
      <div class="row">
        <b style="color:${col}">${zNames[idx]}</b>
        <span>HC: <b>${z[0]}% LEL</b> | O2: <b>${z[1]}%</b> | NH3: <b>${z[2]} ppm</b></span>
        <button class="sm ${z[3]?'shut':'open'}" onclick="cmd('leak=${idx}','alarm')">${z[3]?'STOP LEAK':'SIMULATE LEAK'}</button>
      </div>
    </div>`;
  });
  document.getElementById('fgdControls').innerHTML = `
    <div class="card" style="border-color:${d.esd?'#f22':'#0aa'};background:${d.esd?'#300':'var(--bg-card)'}">
      <div class="row">
        <b style="font-size:13px;color:${d.esd?'#ff0':'#0f0'}">ESD-1 STATUS: ${d.esd ? '🚨 TRIPPED!' : '✅ ARMED & NORMAL'}</b>
        <span>Last Cause: <b>${d.cause}</b></span>
      </div>
      <div class="row" style="margin-top:6px;">
        <button class="sm shut" onclick="cmd('esd=1','alarm_buzzer')">MANUAL ESD-1 TRIP</button>
        <button class="sm open" onclick="cmd('esd=0','success')">RESET ESD-1 & TRIPS</button>
        <button class="sm ${d.cvent?'open':'shut'}" onclick="cmd('esd=2')">COMP VENT: ${d.cvent?'ON':'FAIL'}</button>
        <button class="sm ${d.mdp>20?'open':'shut'}" onclick="cmd('esd=3')">MOTOR RM +dP: ${d.mdp} Pa</button>
        <button class="sm ${d.floop?'open':'shut'}" onclick="cmd('esd=4')">FIRE LOOP (104°C)</button>
      </div>
    </div>${rows}`;
}

// --- VIEW 10: LOADICATOR ---
function drawStationCurve(canvasId, arr, peakVal) {
  const cv = document.getElementById(canvasId);
  if (!cv || !arr) return;
  const cx = cv.getContext('2d'), w = cv.width, h = cv.height, midY = h / 2;
  cx.clearRect(0, 0, w, h);
  const limY = (100 / 140) * (midY - 6);
  cx.strokeStyle = '#622'; cx.setLineDash([4, 3]);
  cx.beginPath(); cx.moveTo(0, midY - limY); cx.lineTo(w, midY - limY);
  cx.moveTo(0, midY + limY); cx.lineTo(w, midY + limY); cx.stroke();
  cx.setLineDash([]);
  cx.strokeStyle = '#257'; cx.beginPath(); cx.moveTo(0, midY); cx.lineTo(w, midY); cx.stroke();
  cx.strokeStyle = Math.abs(peakVal) > 100 ? '#ff3355' : '#00ffaa';
  cx.lineWidth = 2.2; cx.beginPath();
  arr.forEach((v, s) => {
    const x = (s / 19) * (w - 20) + 10, y = midY - (v / 140) * (midY - 6);
    s === 0 ? cx.moveTo(x, y) : cx.lineTo(x, y);
  });
  cx.stroke();
}

function renderLoadicatorView(d) {
  const totCargo = (d.ct[0].mt + d.ct[1].mt + d.ct[2].mt + d.ct[3].mt).toFixed(1);
  document.getElementById('loadTables').innerHTML = `
    <div class="grid2">
      <div class="card">
        <b>DRAFTS & DISPLACEMENT</b>
        <div class="row"><span>Displacement (Δ) / Cargo:</span><b>${d.disp} MT / ${totCargo} MT</b></div>
        <div class="row"><span>Tf / Tm / Ta:</span><b>${d.tf}m / ${d.tm}m / ${d.ta}m</b></div>
        <div class="row"><span>Trim:</span><b>${d.trim>0?'+':''}${d.trim} m</b></div>
        <div class="row"><span>TPC / MCTC:</span><b>${d.tpc} t/cm | ${d.mctc}</b></div>
      </div>
      <div class="card">
        <b>STABILITY & STRENGTH</b>
        <div class="row"><span>KMt / KG / FSC:</span><b>${d.kmt}m / ${d.kg}m / -${d.fsc}m</b></div>
        <div class="row"><span>Fluid GM0 / List:</span><b style="color:#0f0">${d.gm}m | ${d.heel}°</b></div>
        <div class="row"><span>Seagoing SF / BM:</span><b>${d.sf}% / ${d.bm}%</b></div>
        <div class="row"><span>Harbor SF / BM:</span><b>${d.hsf}% / ${d.hbm}%</b></div>
      </div>
    </div>`;
  drawStationCurve('bmCurveCv', d.bm_c, d.bm);
  drawStationCurve('sfCurveCv', d.sf_c, d.sf);
}

// --- VIEW 11: PRESETS ENGINE ---
function renderPresetsView(d) {
  let html = `<div class="card" style="border-color:#0f0">
    <div class="row">
      <b>SNAPSHOT CURRENT SHIP STATE AS NEW PRESET:</b>
      <button class="sm open" onclick="cmd('preset=-1','success')">➕ SAVE CURRENT STATE AS PRESET</button>
    </div>
  </div><div class="grid2">`;
  d.presets.forEach((p, idx) => {
    html += `<div class="card">
      <div style="color:#fa0;font-size:9px;font-weight:bold">[${p.c}]</div>
      <div style="margin:4px 0;font-weight:bold;color:#fff">${p.n}</div>
      <button class="sm open" style="width:100%" onclick="cmd('preset=${idx}','success')">⚡ LOAD PRESET</button>
    </div>`;
  });
  html += `</div>`;
  document.getElementById('presetList').innerHTML = html;
}

window.renderActiveView = function(d) {
  if (curView >= 1 && curView <= 4) renderTankView(d);
  else if (curView === 5) renderHeadersView(d);
  else if (curView === 6) renderPumpsView(d);
  else if (curView === 7) renderCompView(d);
  else if (curView === 8) renderBallastView(d);
  else if (curView === 9) renderSafetyView(d);
  else if (curView === 10) renderLoadicatorView(d);
  else if (curView === 11) renderPresetsView(d);
};

async function sync(q = '', sfx = '') {
  if (sfx) playSfx(sfx);
  else if (q) playSfx('valve_toggle');
  const r = await fetch('/state' + (q ? '?' + q : ''));
  const d = await r.json();
  if (d.esd && (!lastData || !lastData.esd)) playSfx('alarm_buzzer');
  lastData = d;
  update3DShip(d);

  const anyVent = d.man.mast.some(v => v > 1.0);
  const heelStr = d.heel < -0.05 ? `${Math.abs(d.heel).toFixed(1)}°P` : (d.heel > 0.05 ? `${d.heel.toFixed(1)}°S` : '0.0°');
  document.getElementById('hudTop').innerHTML =
    `<b>${d.spd}x</b> | <b>Tf:${d.tf}m Tm:${d.tm}m Ta:${d.ta}m</b> | List:<b>${heelStr}</b> | Air:${d.air}°C SW:${d.sw}°C | ` +
    `${anyVent?'<span style="color:#fa0;font-weight:bold">💨MAST VENT</span> ':''}` +
    `${d.esd?'<span style="color:#f44;font-weight:bold">🚨ESD-1</span>':'OK'}`;
  window.renderActiveView(d);
}

function cmd(q, sfx = '') { sync(q, sfx); }
setInterval(() => sync(), 300);
sync();
