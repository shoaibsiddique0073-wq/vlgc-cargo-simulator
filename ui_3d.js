let curView = 0, activeTank = 0, lastData = null, themeIdx = 0;
let showInfo = true, showCtrl = true, audioEnabled = true, audioCtx = null;
const themes = ['', 'theme-dark', 'theme-term'];

// Hybrid Web Audio API Synthesizer + Local .ogg Player (Guarantees Sound on Android!)
function playSfx(name) {
  if (!audioEnabled) return;
  try { new Audio('/assets/audio/' + name + '.ogg').play().catch(()=>{}); } catch(e){}
  try {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();
    const now = audioCtx.currentTime;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.connect(gain); gain.connect(audioCtx.destination);

    if (name === 'click') {
      osc.type = 'sine'; osc.frequency.setValueAtTime(950, now);
      gain.gain.setValueAtTime(0.12, now); gain.gain.exponentialRampToValueAtTime(0.001, now + 0.045);
      osc.start(now); osc.stop(now + 0.045);
    } else if (name === 'valve_toggle') {
      osc.type = 'triangle'; osc.frequency.setValueAtTime(320, now); osc.frequency.exponentialRampToValueAtTime(140, now + 0.11);
      gain.gain.setValueAtTime(0.18, now); gain.gain.exponentialRampToValueAtTime(0.001, now + 0.11);
      osc.start(now); osc.stop(now + 0.11);
    } else if (name === 'pump_start') {
      osc.type = 'sawtooth'; osc.frequency.setValueAtTime(90, now); osc.frequency.exponentialRampToValueAtTime(260, now + 0.28);
      gain.gain.setValueAtTime(0.16, now); gain.gain.exponentialRampToValueAtTime(0.001, now + 0.28);
      osc.start(now); osc.stop(now + 0.28);
    } else if (name === 'alarm_buzzer' || name === 'alarm') {
      osc.type = 'square'; osc.frequency.setValueAtTime(880, now); osc.frequency.setValueAtTime(620, now + 0.15);
      gain.gain.setValueAtTime(0.24, now); gain.gain.exponentialRampToValueAtTime(0.001, now + 0.35);
      osc.start(now); osc.stop(now + 0.35);
    } else if (name === 'success') {
      osc.type = 'sine'; osc.frequency.setValueAtTime(523, now); osc.frequency.setValueAtTime(784, now + 0.1);
      gain.gain.setValueAtTime(0.18, now); gain.gain.exponentialRampToValueAtTime(0.001, now + 0.25);
      osc.start(now); osc.stop(now + 0.25);
    }
  } catch(e){}
}

function toggleInfo() {
  showInfo = !showInfo;
  document.body.classList.toggle('hide-info', !showInfo);
  document.getElementById('btnInfo').className = 'sm ' + (showInfo ? 'open' : 'shut');
  playSfx('click');
}

function toggleCtrl() {
  showCtrl = !showCtrl;
  document.body.classList.toggle('hide-ctrl', !showCtrl);
  document.getElementById('btnCtrl').className = 'sm ' + (showCtrl ? 'open' : 'shut');
  playSfx('click');
}

function toggleAudio() {
  audioEnabled = !audioEnabled;
  document.getElementById('btnAud').className = 'sm ' + (audioEnabled ? 'open' : 'shut');
  if (audioEnabled) playSfx('success');
}

function cycleTheme() {
  themeIdx = (themeIdx + 1) % 3;
  document.body.classList.remove('theme-dark', 'theme-term');
  if (themes[themeIdx]) document.body.classList.add(themes[themeIdx]);
  playSfx('click');
}

function setView(v) {
  playSfx('click');
  curView = v;
  for (let i = 0; i <= 11; i++) {
    const b = document.getElementById('nb'+i);
    if (b) b.className = (i === v) ? 'act' : (i === 9 ? 'esd-btn' : '');
  }
  document.getElementById('view3d').style.display = (v === 0) ? 'block' : 'none';
  document.getElementById('viewTank').style.display = (v >= 1 && v <= 4) ? 'block' : 'none';
  document.getElementById('viewHeaders').style.display = (v === 5) ? 'block' : 'none';
  document.getElementById('viewPumps').style.display = (v === 6) ? 'block' : 'none';
  document.getElementById('viewComp').style.display = (v === 7) ? 'block' : 'none';
  document.getElementById('viewBallast').style.display = (v === 8) ? 'block' : 'none';
  document.getElementById('viewSafety').style.display = (v === 9) ? 'block' : 'none';
  document.getElementById('viewLoadicator').style.display = (v === 10) ? 'block' : 'none';
  document.getElementById('viewPresets').style.display = (v === 11) ? 'block' : 'none';
  if (v >= 1 && v <= 4) activeTank = v - 1;
  if (lastData && window.renderActiveView) window.renderActiveView(lastData);
}

// --- THREE.JS 3D SHIP-SHAPED VLGC + 4 VENT MAST RISERS + MANIFOLD FLANGES ---
const v3d = document.getElementById('view3d');
const scene = new THREE.Scene();
const cam = new THREE.PerspectiveCamera(45, innerWidth/(innerHeight-84), 1, 1800);
cam.position.set(175, 95, 200);
const ren = new THREE.WebGLRenderer({antialias: true});
ren.setSize(innerWidth, innerHeight - 84);
v3d.appendChild(ren.domElement);
new THREE.OrbitControls(cam, ren.domElement);

const shipGroup = new THREE.Group();
scene.add(shipGroup);
const clickableMeshes = [], mastPlumes3D = [];
let manFlangeMat = null;

function buildShipHull() {
  const g = new THREE.Group();
  const deckShape = new THREE.Shape();
  deckShape.moveTo(-111, -11); deckShape.lineTo(-92, -18.3); deckShape.lineTo(62, -18.3);
  deckShape.quadraticCurveTo(95, -15, 114, 0);
  deckShape.quadraticCurveTo(95, 15, 62, 18.3);
  deckShape.lineTo(-92, 18.3); deckShape.lineTo(-111, 11); deckShape.closePath();

  const hullExtrude = new THREE.ExtrudeGeometry(deckShape, {depth: 22.2, bevelEnabled: false});
  hullExtrude.rotateX(Math.PI / 2); hullExtrude.translate(0, 22.2, 0);
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(hullExtrude), new THREE.LineBasicMaterial({color: 0x0088bb})));
  g.add(new THREE.Mesh(hullExtrude, new THREE.MeshBasicMaterial({color: 0x06182c, transparent: true, opacity: 0.25})));

  const bulb = new THREE.Mesh(new THREE.SphereGeometry(4.2, 12, 10), new THREE.MeshBasicMaterial({color: 0x0088bb, wireframe: true}));
  bulb.scale.set(2.4, 0.9, 0.8); bulb.position.set(112, 3.8, 0); g.add(bulb);

  const trunk = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(168, 2.2, 26)), new THREE.LineBasicMaterial({color: 0x00aacc}));
  trunk.position.set(2, 23.3, 0); g.add(trunk);

  const accom = new THREE.Mesh(new THREE.BoxGeometry(18, 14, 24), new THREE.MeshBasicMaterial({color: 0x113355, transparent: true, opacity: 0.75}));
  accom.position.set(-94, 29.2, 0); g.add(accom);

  const bridge = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(6, 3.5, 36)), new THREE.LineBasicMaterial({color: 0x00ffff}));
  bridge.position.set(-88, 38.0, 0); g.add(bridge);

  const funnel = new THREE.Mesh(new THREE.CylinderGeometry(2.5, 3.5, 10, 12), new THREE.MeshBasicMaterial({color: 0xff6600, wireframe: true}));
  funnel.position.set(-99, 41.0, 0); g.add(funnel);

  const compMesh = new THREE.Mesh(new THREE.BoxGeometry(16, 5.5, 16), new THREE.MeshBasicMaterial({color: 0xffaa00, wireframe: true}));
  compMesh.position.set(2, 27.0, 0); compMesh.userData = {targetView: 7};
  clickableMeshes.push(compMesh); g.add(compMesh);

  manFlangeMat = new THREE.MeshBasicMaterial({color: 0x00ff66, wireframe: true});
  const manMesh = new THREE.Mesh(new THREE.BoxGeometry(8, 3.5, 38.5), manFlangeMat);
  manMesh.position.set(-8, 24.5, 0); manMesh.userData = {targetView: 6};
  clickableMeshes.push(manMesh); g.add(manMesh);

  const pipeConfigs = [
    {z: -3.2, col: 0x00ffaa}, {z: -2.2, col: 0x00aaee},
    {z:  0.0, col: 0xff4444}, {z:  1.0, col: 0xffaa44},
    {z:  2.5, col: 0x00ffff}, {z:  3.3, col: 0x4488ff}
  ];
  pipeConfigs.forEach(p => {
    const cyl = new THREE.Mesh(new THREE.CylinderGeometry(0.28, 0.28, 152, 8), new THREE.MeshBasicMaterial({color: p.col}));
    cyl.rotation.z = Math.PI / 2; cyl.position.set(2, 25.0, p.z); g.add(cyl);
  });
  return g;
}
shipGroup.add(buildShipHull());

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

const ctMeshes = [[], [], [], []], pumpHeads3D = [[], [], [], []];
const xPos = [64.2, 22.8, -19.5, -61.0], halfWidths = [14.2, 16.4, 16.4, 15.8];
const gradeColors = [0x00ffaa, 0x00bbff, 0xcc66ff];

for (let i = 0; i < 4; i++) {
  for (let side = 0; side < 2; side++) {
    const zSign = (side === 0) ? -1 : 1;
    const frameGeo = makePrismaticHalf(36, halfWidths[i], 18.8, side === 1);
    const wire = new THREE.LineSegments(new THREE.EdgesGeometry(frameGeo), new THREE.LineBasicMaterial({color: 0x2266cc}));
    wire.position.set(xPos[i], 1.6, 0); shipGroup.add(wire);

    const liqGeo = makePrismaticHalf(35.2, halfWidths[i]*0.98, 18.5, side === 1);
    const liqMesh = new THREE.Mesh(liqGeo, new THREE.MeshBasicMaterial({color: 0x00ffaa, transparent: true, opacity: 0.68}));
    liqMesh.position.set(xPos[i], 1.7, 0); liqMesh.userData = {targetView: i + 1};
    clickableMeshes.push(liqMesh); shipGroup.add(liqMesh); ctMeshes[i].push(liqMesh);

    const dome = new THREE.Mesh(new THREE.BoxGeometry(6.5, 2.4, 4.5), new THREE.MeshBasicMaterial({color: 0x114477, wireframe: true}));
    dome.position.set(xPos[i], 25.2, zSign * 4.2); dome.userData = {targetView: i + 1};
    clickableMeshes.push(dome); shipGroup.add(dome);

    const pMat = new THREE.MeshBasicMaterial({color: 0x777777});
    const pMotor = new THREE.Mesh(new THREE.CylinderGeometry(0.9, 0.9, 2.6, 10), pMat);
    pMotor.position.set(xPos[i] - 1.5, 27.2, zSign * 4.2); pMotor.userData = {targetView: i + 1};
    clickableMeshes.push(pMotor); shipGroup.add(pMotor); pumpHeads3D[i].push(pMat);
  }

  // No. 1 to No. 4 Vent Mast Risers on Deck + Gas Venting Plume Cone
  const mast = new THREE.Mesh(new THREE.CylinderGeometry(0.45, 0.7, 13.5, 8), new THREE.MeshBasicMaterial({color: 0xdddddd}));
  mast.position.set(xPos[i] + 5.5, 31.0, 0); mast.userData = {targetView: 5};
  clickableMeshes.push(mast); shipGroup.add(mast);

  const plume = new THREE.Mesh(new THREE.ConeGeometry(2.8, 8.0, 10), new THREE.MeshBasicMaterial({color: 0xff5500, wireframe: true}));
  plume.position.set(xPos[i] + 5.5, 41.5, 0); plume.rotation.x = Math.PI; plume.visible = false;
  shipGroup.add(plume); mastPlumes3D.push(plume);
}

const seaUniforms = { uTime: { value: 0.0 } };
const seaMat = new THREE.ShaderMaterial({
  uniforms: seaUniforms, transparent: true, side: THREE.DoubleSide,
  vertexShader: `
    uniform float uTime; varying float vElev;
    void main() {
      vec3 pos = position;
      float w = sin(pos.x * 0.04 + uTime * 1.6) * 0.45 + cos(pos.y * 0.05 + uTime * 1.2) * 0.35;
      pos.z += w; vElev = w;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(pos, 1.0);
    }`,
  fragmentShader: `
    varying float vElev;
    void main() {
      vec3 deep = vec3(0.0, 0.16, 0.38), crest = vec3(0.0, 0.55, 0.85);
      gl_FragColor = vec4(mix(deep, crest, clamp((vElev + 0.8) * 0.65, 0.0, 1.0)), 0.48);
    }`
});
const sea = new THREE.Mesh(new THREE.PlaneGeometry(650, 650, 64, 64), seaMat);
sea.rotation.x = Math.PI / 2; scene.add(sea);

const raycaster = new THREE.Raycaster(), pointer = new THREE.Vector2();
let downPos = {x: 0, y: 0};
ren.domElement.addEventListener('pointerdown', e => { downPos = {x: e.clientX, y: e.clientY}; });
ren.domElement.addEventListener('pointerup', e => {
  if (Math.hypot(e.clientX - downPos.x, e.clientY - downPos.y) > 8) return;
  const rect = ren.domElement.getBoundingClientRect();
  pointer.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
  pointer.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
  raycaster.setFromCamera(pointer, cam);
  const hits = raycaster.intersectObjects(clickableMeshes);
  if (hits.length > 0 && hits[0].object.userData.targetView) setView(hits[0].object.userData.targetView);
});

function update3DShip(d) {
  shipGroup.position.y = -d.tm;
  shipGroup.rotation.z = -Math.atan2(d.trim, 222);
  shipGroup.rotation.x = d.heel * Math.PI / 180.0;

  // Manifold 3D Flange Color: Green when Connected & ESD+Gate Open, Red when Isolated/Disconnected
  const m1Open = d.man.sc[0] && d.man.ev[0] && d.man.gv[0];
  if (manFlangeMat) manFlangeMat.color.setHex(m1Open ? 0x00ff66 : 0xff3333);

  d.ct.forEach((t, i) => {
    ctMeshes[i][0].scale.y = Math.max(0.01, t.p / 100.0);
    ctMeshes[i][1].scale.y = Math.max(0.01, t.s / 100.0);
    ctMeshes[i][0].material.color.setHex(gradeColors[t.g]);
    ctMeshes[i][1].material.color.setHex(gradeColors[t.g]);
    pumpHeads3D[i][0].color.setHex(t.dp[0] ? 0x00ff44 : (t.dp[6] > 0 ? 0xff2222 : 0x777777));
    pumpHeads3D[i][1].color.setHex(t.ds[0] ? 0x00ff44 : (t.ds[6] > 0 ? 0xff2222 : 0x777777));
    if (mastPlumes3D[i]) mastPlumes3D[i].visible = (d.man.mast[i] > 1.0);
  });

  const heelStr = d.heel < -0.05 ? `${Math.abs(d.heel).toFixed(1)}° PORT` : (d.heel > 0.05 ? `${d.heel.toFixed(1)}° STBD` : '0.0° EVEN');
  document.getElementById('draftOverlay').innerHTML =
    `<b style="color:#fff">HULL DRAFTS & MANIFOLD</b><br>` +
    `FWD (Tf): <b>${d.tf}m</b> | MID (Tm): <b>${d.tm}m</b> | AFT (Ta): <b>${d.ta}m</b><br>` +
    `TRIM: <b>${d.trim>0?'+':''}${d.trim}m</b> | LIST: <b>${heelStr}</b> | GM0: <b>${d.gm}m</b><br>` +
    `MANIFOLD: <b style="color:${m1Open?'#0f0':'#f55'}">${m1Open?'CONNECTED & OPEN':'ISOLATED/SHUT'}</b><br>` +
    `LIQ LINE PT: <b>${d.man.lpt[0]} bar</b> | TT: <b>${d.man.ltt[0]}°C</b>`;
}

function anim(t) {
  requestAnimationFrame(anim);
  seaUniforms.uTime.value = t * 0.001;
  if (curView === 0) ren.render(scene, cam);
}
requestAnimationFrame(anim);
