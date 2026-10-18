let curView = 0, activeTank = 0, lastData = null;

function setView(v) {
  curView = v;
  for (let i = 0; i <= 8; i++) document.getElementById('nb'+i).className = (i === v) ? 'act' : '';
  document.getElementById('view3d').style.display = (v === 0) ? 'block' : 'none';
  document.getElementById('viewTank').style.display = (v >= 1 && v <= 4) ? 'block' : 'none';
  document.getElementById('viewHeaders').style.display = (v === 5) ? 'block' : 'none';
  document.getElementById('viewPumps').style.display = (v === 6) ? 'block' : 'none';
  document.getElementById('viewComp').style.display = (v === 7) ? 'block' : 'none';
  document.getElementById('viewBallast').style.display = (v === 8) ? 'block' : 'none';
  if (v >= 1 && v <= 4) activeTank = v - 1;
  if (lastData) renderActiveView(lastData);
}

// --- 3D SHIP VIEWPORT ---
const v3d = document.getElementById('view3d');
const scene = new THREE.Scene();
const cam = new THREE.PerspectiveCamera(45, innerWidth/(innerHeight-82), 1, 1500);
cam.position.set(165, 95, 195);
const ren = new THREE.WebGLRenderer({antialias: true});
ren.setSize(innerWidth, innerHeight - 82);
v3d.appendChild(ren.domElement);
new THREE.OrbitControls(cam, ren.domElement);

const shipGroup = new THREE.Group();
scene.add(shipGroup);

function makePrismaticHalf(len, halfW, h, isStbd) {
  const s = new THREE.Shape();
  if (isStbd) {
    s.moveTo(0.15, 0); s.lineTo(halfW*0.76, 0); s.lineTo(halfW, h*0.18);
    s.lineTo(halfW, h*0.72); s.lineTo(halfW*0.64, h); s.lineTo(0.15, h);
  } else {
    s.moveTo(-0.15, 0); s.lineTo(-0.15, h); s.lineTo(-halfW*0.64, h);
    s.lineTo(-halfW, h*0.72); s.lineTo(-halfW, h*0.18); s.lineTo(-halfW*0.76, 0);
  }
  s.closePath();
  const g = new THREE.ExtrudeGeometry(s, {depth: len, bevelEnabled: false});
  g.rotateY(Math.PI/2); g.translate(-len/2, 0, 0);
  return g;
}

const hullShape = new THREE.Shape();
hullShape.moveTo(-14,0); hullShape.lineTo(14,0); hullShape.lineTo(18.3,4);
hullShape.lineTo(18.3,22.2); hullShape.lineTo(-18.3,22.2); hullShape.lineTo(-18.3,4); hullShape.closePath();
const hullGeo = new THREE.ExtrudeGeometry(hullShape, {depth: 222, bevelEnabled: false});
hullGeo.rotateY(Math.PI/2); hullGeo.translate(-111, 0, 0);
shipGroup.add(new THREE.LineSegments(new THREE.EdgesGeometry(hullGeo), new THREE.LineBasicMaterial({color: 0x007799})));

const ctMeshes = [[], [], [], []];
const xPos = [64.2, 22.8, -19.5, -61.0], halfWidths = [14.2, 16.4, 16.4, 15.8];
for (let i = 0; i < 4; i++) {
  for (let side = 0; side < 2; side++) {
    const frameGeo = makePrismaticHalf(36, halfWidths[i], 18.8, side === 1);
    const wire = new THREE.LineSegments(new THREE.EdgesGeometry(frameGeo), new THREE.LineBasicMaterial({color: 0x2266cc}));
    wire.position.set(xPos[i], 1.6, 0);
    shipGroup.add(wire);
    const liqGeo = makePrismaticHalf(35.2, halfWidths[i]*0.98, 18.5, side === 1);
    const liqMesh = new THREE.Mesh(liqGeo, new THREE.MeshBasicMaterial({color: side === 0 ? 0x00ffaa : 0x00ddff, transparent: true, opacity: 0.68}));
    liqMesh.position.set(xPos[i], 1.7, 0);
    shipGroup.add(liqMesh);
    ctMeshes[i].push(liqMesh);
  }
}
const sea = new THREE.Mesh(new THREE.PlaneGeometry(550, 550), new THREE.MeshBasicMaterial({color: 0x003377, transparent: true, opacity: 0.42, side: THREE.DoubleSide}));
sea.rotation.x = Math.PI/2;
scene.add(sea);

const tripNames = ['OK', 'TRIP: DRY-RUN (<35A)', 'TRIP: OVERLOAD (>185A)', 'TRIP: NPSH CAVITATION'];

// --- VIEW 1-4: INSIDE CARGO TANK (WITH DEEPWELL PUMPS + SPRAY/SPARGE + BULKHEAD VALVE) ---
function renderTankView(d) {
  const i = activeTank, t = d.ct[i], dp = t.dp, ds = t.ds, vr = t.vr;
  document.getElementById('tkTitle').innerHTML =
    `CT${i+1} [${t.g===0?'PROPANE':'BUTANE'}] | SYS ${t.sys+1} | Dome: <span style="color:${t.pr>220?'#f44':'#0f0'}">${t.pr} mbarg</span> | MaxFL:${t.fl}%`;

  const pH = (t.p / 100) * 130, sH = (t.s / 100) * 130;
  const pCol = dp[0] ? (dp[5] ? '#ff0' : '#0f0') : (dp[6]>0 ? '#f33' : '#666');
  const sCol = ds[0] ? (ds[5] ? '#ff0' : '#0f0') : (ds[6]>0 ? '#f33' : '#666');

  document.getElementById('tkSvg').innerHTML = `
    <line x1="190" y1="2" x2="190" y2="18" stroke="#0ff" stroke-width="2"/>
    <line x1="60" y1="18" x2="320" y2="18" stroke="#0ff" stroke-width="2"/>
    <polygon points="75,52 188,52 188,202 65,202 45,178 45,82" fill="#06101e" stroke="#28f" stroke-width="2"/>
    <polygon points="192,52 305,52 335,82 335,178 315,202 192,202" fill="#06101e" stroke="#28f" stroke-width="2"/>
    <rect x="48" y="${201-pH}" width="139" height="${pH}" fill="rgba(0,255,170,0.42)"/>
    <rect x="193" y="${201-sH}" width="139" height="${sH}" fill="rgba(0,210,255,0.42)"/>

    <!-- Centerline Bulkhead & Valve -->
    <line x1="190" y1="52" x2="190" y2="180" stroke="#aaa" stroke-width="3"/>
    <rect x="176" y="182" width="28" height="15" rx="3" fill="${t.bhd?'#082':'#811'}" stroke="#fff" onclick="cmd('bhd=${i}')"/>
    <text x="190" y="193" fill="#fff" font-size="8" text-anchor="middle" onclick="cmd('bhd=${i}')">${t.bhd?'OPEN':'SHUT'}</text>

    <!-- Port & Stbd Deepwell Cargo Pumps in Sumps -->
    <line x1="150" y1="30" x2="150" y2="196" stroke="${pCol}" stroke-width="4"/>
    <circle cx="150" cy="196" r="7" fill="${pCol}" stroke="#fff" onclick="cmd('pstart=${i}&side=0')"/>
    <text x="135" y="42" fill="${pCol}" font-size="8" text-anchor="end">P-DWP: ${dp[2]}m³/h (${dp[4]}A)</text>

    <line x1="230" y1="30" x2="230" y2="196" stroke="${sCol}" stroke-width="4"/>
    <circle cx="230" cy="196" r="7" fill="${sCol}" stroke="#fff" onclick="cmd('pstart=${i}&side=1')"/>
    <text x="245" y="42" fill="${sCol}" font-size="8">S-DWP: ${ds[2]}m³/h (${ds[4]}A)</text>

    <!-- Spray & Sparge Lines -->
    <line x1="115" y1="18" x2="115" y2="65" stroke="${vr[0]?'#0f0':'#444'}" stroke-width="2"/>
    <line x1="85" y1="18" x2="85" y2="140" stroke="${vr[1]?'#0f0':'#444'}" stroke-width="2"/>
    <line x1="58" y1="18" x2="58" y2="195" stroke="${vr[2]?'#fa0':'#444'}" stroke-width="2"/>
    <line x1="265" y1="18" x2="265" y2="65" stroke="${vr[3]?'#0f0':'#444'}" stroke-width="2"/>
    <line x1="295" y1="18" x2="295" y2="140" stroke="${vr[4]?'#0f0':'#444'}" stroke-width="2"/>
    <line x1="322" y1="18" x2="322" y2="195" stroke="${vr[5]?'#fa0':'#444'}" stroke-width="2"/>

    <text x="115" y="105" fill="#fff" font-size="9" text-anchor="middle">VAP:${t.tv}°C | TOP:${t.ts}°C</text>
    <text x="265" y="105" fill="#0f8" font-size="9" text-anchor="middle">HC:${t.hc}% IG:${t.ig}% O2:${t.o2}%</text>
    <text x="115" y="160" fill="#0ff" font-size="10" font-weight="bold" text-anchor="middle">PORT: ${t.p}%</text>
    <text x="265" y="160" fill="#0ff" font-size="10" font-weight="bold" text-anchor="middle">STBD: ${t.s}%</text>
    <text x="190" y="222" fill="#fa0" font-size="9" text-anchor="middle">LIQ:${t.tl}°C | BOT SHELL:${t.bs}°C | P:${tripNames[dp[6]]} | S:${tripNames[ds[6]]}</text>
  `;

  document.getElementById('tkControls').innerHTML = `
    <div class="card">
      <div class="row">
        <button class="sm ${dp[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=0')">CT${i+1}P PUMP: ${dp[0]?'RUN':'STOP'}</button>
        <span>Throttle (${dp[1]}%):</span>
        <input type="range" min="5" max="100" value="${dp[1]}" onchange="cmd('pthr=${i}&side=0&v='+this.value)">
        <span>${dp[3]} bar | ${dp[4]}A ${dp[5]?'⚠️CAV':''}</span>
      </div>
      <div class="row">
        <button class="sm ${ds[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=1')">CT${i+1}S PUMP: ${ds[0]?'RUN':'STOP'}</button>
        <span>Throttle (${ds[1]}%):</span>
        <input type="range" min="5" max="100" value="${ds[1]}" onchange="cmd('pthr=${i}&side=1&v='+this.value)">
        <span>${ds[3]} bar | ${ds[4]}A ${ds[5]?'⚠️CAV':''}</span>
      </div>
    </div>
    <div class="card">
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
        <button class="sm ${t.fv?'open':'shut'}" onclick="cmd('fvlv=${i}')">FILL VLV: ${t.fv?'OPEN':'SHUT'}</button>
        <button class="sm ${t.bhd?'open':'shut'}" onclick="cmd('bhd=${i}')">BHD: ${t.bhd?'OPEN':'SHUT'}</button>
        <button class="sm" onclick="cmd('tshell=${i}&v=20')">WARM (+20°C)</button>
        <button class="sm" onclick="cmd('grade=${i}&g=${1-t.g}')">C3/C4</button>
        <button class="sm" onclick="cmd('sys=${i}')">SYS ${t.sys+1}</button>
      </div>
    </div>`;
}

// --- VIEW 5: VAPOUR & CONDENSATE HEADERS ---
function renderHeadersView(d) {
  let branches = '';
  for (let i = 0; i < 4; i++) {
    const x = 60 + i * 85, t = d.ct[i];
    const vy = t.sys === 0 ? 35 : 60, cy = t.sys === 0 ? 105 : 130;
    const anyRet = t.vr.some(v => v === 1);
    branches += `
      <rect x="${x-28}" y="165" width="56" height="50" rx="4" fill="#08182c" stroke="#28f" onclick="setView(${i+1})"/>
      <text x="${x}" y="183" fill="#fff" font-size="10" font-weight="bold" text-anchor="middle">CT${i+1} (S${t.sys+1})</text>
      <text x="${x}" y="196" fill="#0ff" font-size="8" text-anchor="middle">${t.pr} mbarg</text>
      <text x="${x}" y="209" fill="#0fa" font-size="8" text-anchor="middle">P:${t.p}% S:${t.s}%</text>
      <line x1="${x-10}" y1="${vy}" x2="${x-10}" y2="165" stroke="#f66" stroke-width="2" stroke-dasharray="3,2"/>
      <line x1="${x+10}" y1="${cy}" x2="${x+10}" y2="165" stroke="${anyRet?'#0f0':'#466'}" stroke-width="${anyRet?3:1.5}"/>
    `;
  }
  document.getElementById('hdrSvg').innerHTML = `
    <text x="10" y="25" fill="#f88" font-size="9" font-weight="bold">VAPOUR SUCTION HEADERS SYS 1 & SYS 2</text>
    <line x1="20" y1="35" x2="360" y2="35" stroke="#f55" stroke-width="3"/>
    <line x1="20" y1="60" x2="360" y2="60" stroke="#fa5" stroke-width="3"/>
    <line x1="190" y1="35" x2="190" y2="60" stroke="${d.xo[0]?'#0f0':'#555'}" stroke-width="3"/>
    <text x="10" y="95" fill="#0ff" font-size="9" font-weight="bold">CONDENSATE RETURN HEADERS SYS 1 & SYS 2</text>
    <line x1="20" y1="105" x2="360" y2="105" stroke="#0ff" stroke-width="3"/>
    <line x1="20" y1="130" x2="360" y2="130" stroke="#08f" stroke-width="3"/>
    <line x1="190" y1="105" x2="190" y2="130" stroke="${d.xo[1]?'#0f0':'#555'}" stroke-width="3"/>
    ${branches}
  `;
  document.getElementById('hdrControls').innerHTML = `
    <div class="card">
      <div class="row"><b>CROSSOVER VALVES:</b>
        <button class="sm ${d.xo[0]?'open':'shut'}" onclick="cmd('xo=0')">VAP X-OVER</button>
        <button class="sm ${d.xo[1]?'open':'shut'}" onclick="cmd('xo=1')">COND X-OVER</button>
        <button class="sm ${d.xo[2]?'open':'shut'}" onclick="cmd('xo=2')">LIQ X-OVER</button>
      </div>
    </div>`;
}

// --- VIEW 6: 8 DEEPWELL CARGO PUMPS, LIQUID HEADERS & MANIFOLD HEATER/BOOSTER ---
function renderPumpsView(d) {
  const m = d.man;
  let svgPumps = '', ctrlRows = '';
  for (let i = 0; i < 4; i++) {
    const x = 52 + i * 74, t = d.ct[i], dp = t.dp, ds = t.ds;
    const ly = t.sys === 0 ? 40 : 68;
    const pCol = dp[0] ? (dp[5] ? '#ff0' : '#0f0') : (dp[6]>0 ? '#f33' : '#555');
    const sCol = ds[0] ? (ds[5] ? '#ff0' : '#0f0') : (ds[6]>0 ? '#f33' : '#555');

    svgPumps += `
      <rect x="${x-30}" y="115" width="60" height="82" rx="4" fill="#071526" stroke="#28f"/>
      <text x="${x}" y="128" fill="#fff" font-size="9" font-weight="bold" text-anchor="middle">CT${i+1} (S${t.sys+1})</text>
      <line x1="${x-14}" y1="${ly}" x2="${x-14}" y2="180" stroke="${pCol}" stroke-width="2.5"/>
      <circle cx="${x-14}" cy="180" r="6" fill="${pCol}" onclick="cmd('pstart=${i}&side=0')"/>
      <text x="${x-14}" y="165" fill="#0ff" font-size="7" text-anchor="middle">${dp[2]}</text>
      <line x1="${x+14}" y1="${ly}" x2="${x+14}" y2="180" stroke="${sCol}" stroke-width="2.5"/>
      <circle cx="${x+14}" cy="180" r="6" fill="${sCol}" onclick="cmd('pstart=${i}&side=1')"/>
      <text x="${x+14}" y="165" fill="#0ff" font-size="7" text-anchor="middle">${ds[2]}</text>
    `;

    ctrlRows += `<div class="card">
      <div class="row">
        <b>CT${i+1} (SYS ${t.sys+1})</b>
        <button class="sm ${t.fv?'open':'shut'}" onclick="cmd('fvlv=${i}')">LOAD VLV:${t.fv?'OPEN':'SHUT'}</button>
        <span style="color:#fa0">${dp[6]>0?tripNames[dp[6]]:(ds[6]>0?tripNames[ds[6]]:'')}</span>
      </div>
      <div class="row">
        <button class="sm ${dp[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=0')">CT${i+1}P:${dp[0]?'RUN':'STOP'}</button>
        <span>Thr:${dp[1]}%</span><input type="range" min="5" max="100" value="${dp[1]}" onchange="cmd('pthr=${i}&side=0&v='+this.value)">
        <span>${dp[2]}m³/h | ${dp[4]}A</span>
      </div>
      <div class="row">
        <button class="sm ${ds[0]?'open':'shut'}" onclick="cmd('pstart=${i}&side=1')">CT${i+1}S:${ds[0]?'RUN':'STOP'}</button>
        <span>Thr:${ds[1]}%</span><input type="range" min="5" max="100" value="${ds[1]}" onchange="cmd('pthr=${i}&side=1&v='+this.value)">
        <span>${ds[2]}m³/h | ${ds[4]}A</span>
      </div>
    </div>`;
  }

  document.getElementById('pmpSvg').innerHTML = `
    <text x="12" y="26" fill="#0fa" font-size="9" font-weight="bold">LIQUID HEADER SYS 1 (${m.q[0]} m³/hr) & SYS 2 (${m.q[1]} m³/hr)</text>
    <line x1="15" y1="40" x2="310" y2="40" stroke="#0fa" stroke-width="3.5"/>
    <line x1="15" y1="68" x2="310" y2="68" stroke="#0af" stroke-width="3.5"/>
    <!-- Cargo Heater & Booster Pump on Manifold Export -->
    <rect x="312" y="25" width="60" height="34" rx="4" fill="${m.htr?'#730':'#081828'}" stroke="#fa0" onclick="cmd('htr=1')"/>
    <text x="342" y="40" fill="#fff" font-size="8" text-anchor="middle">SW HEATER</text>
    <text x="342" y="52" fill="#fa0" font-size="8" text-anchor="middle">${m.et}°C</text>
    <rect x="312" y="65" width="60" height="34" rx="4" fill="${m.bst?'#062':'#081828'}" stroke="#0f0" onclick="cmd('bst=1')"/>
    <text x="342" y="80" fill="#fff" font-size="8" text-anchor="middle">BOOSTER</text>
    <text x="342" y="92" fill="#0f0" font-size="8" text-anchor="middle">${m.eb} bar</text>
    ${svgPumps}
  `;

  const opNames = ['IDLE', 'DISCHARGING', 'SHORE LOADING'];
  document.getElementById('pmpControls').innerHTML = `
    <div class="card">
      <div class="row">
        <b>SYS 1 MANIFOLD (${opNames[m.op[0]]}):</b>
        <button class="sm ${m.op[0]===1?'open':''}" onclick="cmd('mop=0&m=1')">DISCHARGE</button>
        <button class="sm ${m.op[0]===2?'hot':''}" onclick="cmd('mop=0&m=2')">SHORE LOAD</button>
        <button class="sm ${m.op[0]===0?'shut':''}" onclick="cmd('mop=0&m=0')">ISOLATE</button>
      </div>
      <div class="row">
        <b>SYS 2 MANIFOLD (${opNames[m.op[1]]}):</b>
        <button class="sm ${m.op[1]===1?'open':''}" onclick="cmd('mop=1&m=1')">DISCHARGE</button>
        <button class="sm ${m.op[1]===2?'hot':''}" onclick="cmd('mop=1&m=2')">SHORE LOAD</button>
        <button class="sm ${m.op[1]===0?'shut':''}" onclick="cmd('mop=1&m=0')">ISOLATE</button>
      </div>
      <div class="row">
        <span>Shore Backpress (${m.sb[0]} bar):</span>
        <input type="range" min="1" max="10" step="0.5" value="${m.sb[0]}" onchange="cmd('sbar=0&v='+this.value);cmd('sbar=1&v='+this.value)">
        <button class="sm ${m.htr?'hot':''}" onclick="cmd('htr=1')">HEATER</button>
        <button class="sm ${m.bst?'open':''}" onclick="cmd('bst=1')">BOOSTER</button>
      </div>
    </div>
    ${ctrlRows}`;
}

// --- VIEW 7: COMPRESSOR & MOTOR ROOM ---
function renderCompView(d) {
  let units = '', cards = '';
  const mNames = ['STOPPED', 'RELIQ MODE', 'HOT GAS'];
  for (let c = 0; c < 4; c++) {
    const x = 52 + c * 90, m = d.comp[c][0], sys = d.comp[c][1];
    const col = m === 1 ? '#0f8' : (m === 2 ? '#fa0' : '#555');
    units += `
      <rect x="${x-32}" y="26" width="64" height="55" rx="5" fill="#091a2e" stroke="${col}" stroke-width="2"/>
      <text x="${x}" y="42" fill="#fff" font-size="9" font-weight="bold" text-anchor="middle">COMP #${c+1}</text>
      <text x="${x}" y="55" fill="${col}" font-size="8" text-anchor="middle">${mNames[m]}</text>
      <text x="${x}" y="68" fill="#0ff" font-size="8" text-anchor="middle">SYS ${sys+1}</text>
      <line x1="${x}" y1="81" x2="${x}" y2="122" stroke="${col}" stroke-width="3"/>
      <circle cx="${x}" cy="102" r="5" fill="#ff0"/>
      <rect x="${x-26}" y="122" width="52" height="40" rx="4" fill="#071320" stroke="#08a"/>
      <text x="${x}" y="140" fill="#0ff" font-size="8" text-anchor="middle">MOTOR #${c+1}</text>
      <text x="${x}" y="153" fill="#8f8" font-size="8" text-anchor="middle">${m>0?'142A':'0A'}</text>
    `;
    cards += `<div class="card">
      <div><b>COMP #${c+1} (SYS ${sys+1})</b></div>
      <div class="row" style="margin-top:4px;">
        <button class="sm ${m===0?'shut':''}" onclick="cmd('comp=${c}&m=0')">STOP</button>
        <button class="sm ${m===1?'open':''}" onclick="cmd('comp=${c}&m=1')">RELIQ</button>
        <button class="sm ${m===2?'hot':''}" onclick="cmd('comp=${c}&m=2')">HOT GAS</button>
        <button class="sm" onclick="cmd('csys=${c}')">SYS ${sys+1}</button>
      </div>
    </div>`;
  }
  document.getElementById('compSvg').innerHTML = `
    <text x="190" y="15" fill="#0ff" font-size="9" font-weight="bold" text-anchor="middle">COMPRESSOR ROOM (EXHAUST VENT)</text>
    <line x1="10" y1="102" x2="370" y2="102" stroke="#f44" stroke-width="3" stroke-dasharray="6,3"/>
    <text x="190" y="97" fill="#ff0" font-size="8" text-anchor="middle">GAS-TIGHT BULKHEAD & SHAFT SEALS</text>
    <text x="190" y="180" fill="#0ff" font-size="9" font-weight="bold" text-anchor="middle">ELECTRIC MOTOR ROOM (+50 Pa)</text>
    ${units}`;
  document.getElementById('compControls').innerHTML = cards;
}

// --- VIEW 8: BALLAST WBT ---
function renderBallastView(d) {
  const pair = d.wtgt, pPct = d.wbt[pair*2][0], sPct = d.wbt[pair*2+1][0];
  const pH = (pPct / 100) * 125, sH = (sPct / 100) * 125;
  document.getElementById('wbtSvg').innerHTML = `
    <defs>
      <clipPath id="wP"><polygon points="30,25 95,25 95,65 58,65 58,120 185,120 185,150 30,150"/></clipPath>
      <clipPath id="wS"><polygon points="350,25 285,25 285,65 322,65 322,120 195,120 195,150 350,150"/></clipPath>
    </defs>
    <rect x="30" y="${150-pH}" width="160" height="${pH}" fill="rgba(0,150,255,0.65)" clip-path="url(#wP)"/>
    <rect x="190" y="${150-sH}" width="160" height="${sH}" fill="rgba(0,150,255,0.65)" clip-path="url(#wS)"/>
    <polygon points="30,25 95,25 95,65 58,65 58,120 185,120 185,150 30,150" fill="none" stroke="#0cf" stroke-width="2"/>
    <polygon points="350,25 285,25 285,65 322,65 322,120 195,120 195,150 350,150" fill="none" stroke="#0cf" stroke-width="2"/>
    <text x="110" y="142" fill="#fff" font-size="9">PORT: ${pPct}%</text>
    <text x="235" y="142" fill="#fff" font-size="9">STBD: ${sPct}%</text>
  `;
  const wNames = ['WBT1P','WBT1S','WBT2P','WBT2S','WBT3P','WBT3S','WBT4P','WBT4S','FPK','APT'];
  let sliders = '';
  for (let j = 0; j < 10; j += 2) {
    sliders += `<div class="row"><span style="width:75px">${wNames[j]}:${d.wbt[j][0]}%</span>` +
      `<input type="range" min="0" max="100" value="${d.wbt[j][0]}" onchange="cmd('wbt=${j}&v='+this.value)">` +
      `<input type="range" min="0" max="100" value="${d.wbt[j+1][0]}" onchange="cmd('wbt=${j+1}&v='+this.value)">` +
      `<span style="width:75px;text-align:right">${wNames[j+1]}:${d.wbt[j+1][0]}%</span></div>`;
  }
  document.getElementById('wbtControls').innerHTML = `
    <div class="card">
      <div class="row">
        <button class="sm" onclick="cmd('wtgt=0')">WBT1</button><button class="sm" onclick="cmd('wtgt=1')">WBT2</button>
        <button class="sm" onclick="cmd('wtgt=2')">WBT3</button><button class="sm" onclick="cmd('wtgt=3')">WBT4</button>
        <button class="sm" onclick="cmd('bp=0&m=1');cmd('bp=1&m=1')">BALLAST</button>
        <button class="sm" onclick="cmd('bp=0&m=2');cmd('bp=1&m=2')">DEBALLAST</button>
        <button class="sm shut" onclick="cmd('bp=0&m=0');cmd('bp=1&m=0')">STOP</button>
      </div>
      ${sliders}
    </div>`;
}

function renderActiveView(d) {
  if (curView >= 1 && curView <= 4) renderTankView(d);
  else if (curView === 5) renderHeadersView(d);
  else if (curView === 6) renderPumpsView(d);
  else if (curView === 7) renderCompView(d);
  else if (curView === 8) renderBallastView(d);
}

async function sync(q = '') {
  const r = await fetch('/state' + (q ? '?' + q : ''));
  const d = await r.json();
  lastData = d;
  shipGroup.position.y = -d.tm;
  shipGroup.rotation.z = -Math.atan2(d.trim, 222);
  shipGroup.rotation.x = d.heel * Math.PI / 180.0;
  d.ct.forEach((t, i) => {
    ctMeshes[i][0].scale.y = Math.max(0.01, t.p / 100.0);
    ctMeshes[i][1].scale.y = Math.max(0.01, t.s / 100.0);
  });
  const heelStr = d.heel < -0.05 ? `${Math.abs(d.heel).toFixed(1)}°P` : (d.heel > 0.05 ? `${d.heel.toFixed(1)}°S` : '0.0°');
  document.getElementById('hudTop').innerHTML =
    `<b>${d.spd}x</b> | F:${d.tf}m A:${d.ta}m | List:<b>${heelStr}</b> | GM0:<b>${d.gm}m</b> | BM:<b>${d.bm}%</b> | Q:${d.man.q[0]+d.man.q[1]}m³/h`;
  renderActiveView(d);
}

function cmd(q) { sync(q); }
setInterval(() => sync(), 300);
function anim() { requestAnimationFrame(anim); if (curView === 0) ren.render(scene, cam); }
anim();
