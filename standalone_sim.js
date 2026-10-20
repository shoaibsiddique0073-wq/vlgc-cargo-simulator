// Automatic Serverless Physics Fallback v1.1 (Ballast Network, Air/SW Thermo & Mass Conservation)
(function() {
  const GRADES = [
    {mw:44.097, tb:-42.1, A:3.92828, B:803.997, C:-26.11, r15:507.5, a:0.00162, tr:-36.8},
    {mw:58.122, tb:-0.5,  A:3.93266, B:935.773, C:-34.36, r15:584.2, a:0.00108, tr:4.8},
    {mw:17.031, tb:-33.3, A:4.48540, B:926.132, C:-32.98, r15:618.2, a:0.00148, tr:-28.2}
  ];
  const PRESETS = [
    {n:"1. Pre-DD: Stripping & Puddle Heating", c:"PRE-DRYDOCK", g1:0, g2:0, p:1.2, ts:-39, tv:-35, tl:-41.5, pr:135, hc:98.5, ig:1.4, o2:0.1, prg:0, cm:2, vr:[0,0,1,0,0,1], mop:1, dwp:0, fv:0, wbt:65},
    {n:"2. Pre-DD: Tank Warming-Up (+5°C Target)", c:"PRE-DRYDOCK", g1:0, g2:0, p:0.0, ts:-18, tv:12, tl:-15, pr:160, hc:98, ig:1.9, o2:0.1, prg:0, cm:2, vr:[0,1,1,0,1,1], mop:0, dwp:0, fv:0, wbt:65},
    {n:"3. Pre-DD: Inerting (IGG/N2 Purging <2% HC)", c:"PRE-DRYDOCK", g1:0, g2:0, p:0.0, ts:18, tv:20, tl:18, pr:60, hc:14, ig:84.5, o2:1.5, prg:1, cm:0, vr:[0,0,0,0,0,0], mop:0, dwp:0, fv:0, wbt:60},
    {n:"4. Pre-DD: Aerating (Gas-Free for Man Entry)", c:"PRE-DRYDOCK", g1:0, g2:0, p:0.0, ts:24, tv:24, tl:24, pr:15, hc:0.2, ig:82.8, o2:17, prg:2, cm:0, vr:[0,0,0,0,0,0], mop:0, dwp:0, fv:0, wbt:55},
    {n:"5. Post-DD: Drying & Inerting (O2 < 2%)", c:"POST-DRYDOCK", g1:0, g2:0, p:0.0, ts:25, tv:25, tl:25, pr:45, hc:0, ig:91.5, o2:8.5, prg:1, cm:0, vr:[0,0,0,0,0,0], mop:0, dwp:0, fv:0, wbt:60},
    {n:"6. Post-DD: Gassing-Up (Purging N2 w/ C3 Vapor)", c:"POST-DRYDOCK", g1:0, g2:0, p:0.0, ts:22, tv:15, tl:20, pr:85, hc:68, ig:31.5, o2:0.5, prg:3, cm:0, vr:[0,0,0,0,0,0], mop:0, dwp:0, fv:0, wbt:60},
    {n:"7. Post-DD: Tank Cool-Down (Top/Bot Spray)", c:"POST-DRYDOCK", g1:0, g2:0, p:2.5, ts:8, tv:-12, tl:-41.2, pr:145, hc:98.8, ig:1.1, o2:0.1, prg:0, cm:1, vr:[1,1,0,1,1,0], mop:0, dwp:0, fv:0, wbt:60},
    {n:"8. Ops: Full Homogeneous C3 Propane Loading", c:"CARGO OPS", g1:0, g2:0, p:35, ts:-40.5, tv:-40.8, tl:-41.5, pr:110, hc:99, ig:0.9, o2:0.1, prg:0, cm:1, vr:[1,0,0,1,0,0], mop:2, dwp:0, fv:1, wbt:55},
    {n:"9. Ops: 2-Grade Segregated (Sys1: C3, Sys2: C4)", c:"CARGO OPS", g1:0, g2:1, p:68, ts:-40.5, tv:-40.8, tl:-41.5, pr:95, hc:99, ig:0.9, o2:0.1, prg:0, cm:1, vr:[0,0,1,0,0,1], mop:1, dwp:1, fv:0, wbt:25},
    {n:"10. Ops: Full 8-Pump Discharging + Ballasting", c:"CARGO OPS", g1:0, g2:0, p:82, ts:-41, tv:-41, tl:-41.6, pr:90, hc:99, ig:0.9, o2:0.1, prg:0, cm:1, vr:[0,0,0,0,0,0], mop:1, dwp:1, fv:0, wbt:15},
    {n:"11. Ops: Heated & Pressurized Discharge", c:"CARGO OPS", g1:0, g2:0, p:60, ts:-40.8, tv:-40.8, tl:-41.5, pr:105, hc:99, ig:0.9, o2:0.1, prg:0, cm:1, vr:[0,0,0,0,0,0], mop:1, dwp:1, fv:0, wbt:30},
    {n:"12. Drill: Overpressure & Trapped Line Mast Venting", c:"EMERGENCY", g1:0, g2:0, p:85, ts:-34, tv:-33.5, tl:-35.8, pr:256, hc:99, ig:0.9, o2:0.1, prg:0, cm:0, vr:[0,0,0,0,0,0], mop:1, dwp:0, fv:0, wbt:20}
  ];

  const S = {
    spd:1, time:0, air:25.0, sw:22.0, disp:51200, tf:8.2, ta:8.6, tm:8.4, trim:-0.4, heel:0.0,
    tpc:68.2, mctc:995, lcb:1.9, lcf:-2.6, lcg:1.5, kmt:16.9, kg:11.8, fsc:1.2, gm:3.9,
    sf:42, bm:48, hsf:32, hbm:36, wtgt:0, xo:[0,0,0],
    esd:0, cause:"SYSTEM NORMAL", mdp:55, cvent:1, floop:1,
    bal:{pr:[0,0], op:1, xo:1, q:0, pq:[0,0], pb:[0.0,0.0], pa:[0,0]},
    man:{op:[1,1], sc:[1,1], ev:[1,1], gv:[1,1], lpt:[4.0,4.0], ltt:[-41.2,-0.3],
         vpt:[0.09,0.09], vtt:[-39,0.5], cpt:[0.1,0.1], ctt:[-41,0],
         trv:[0,0], marvs:[0,0,0,0], mast:[0,0,0,0], sb:[4.0,4.0], q:[0,0], htr:0, bst:0, et:-41.5, eb:4.0},
    ct:[0,1,2,3].map(i => ({
      p:50, s:50, mp:3055.5, ms:3055.5, bhd:1, g:0, sys:i%2, fl:96.9, pr:95, rho:582, mt:6111.0, vsuc:1, cin:1, lact:0,
      ts:-38.5, bs:-41.2, tv:-39.5, tl:-41.5, cr:0.0, hc:98.5, ig:1.4, o2:0.1, prg:0, fv:0,
      vr:[0,0,0,0,0,0], dp:[0,25,0,0,0,0,0], ds:[0,25,0,0,0,0,0]
    })),
    comp:[[0,0,91,9.2],[0,0,91,9.2],[0,1,91,9.2],[0,1,91,9.2]],
    fgd:[0,1,2,3,4,5].map(z => [0.0, z<4?0.8:20.9, 0, 0, 0]),
    wbt:[0,1,2,3,4,5,6,7,8,9].map((_,j) => [45.0, 0, j<2?1:0, 0]),
    presets: PRESETS, sf_c: Array(20).fill(0), bm_c: Array(20).fill(0)
  };

  function syncMass(t) {
    const gr = GRADES[t.g];
    t.rho = +(gr.r15 * (1 - gr.a * (t.tl - 15))).toFixed(1);
    t.mp = +((t.p / 100) * 10500 * (t.rho / 1000)).toFixed(1);
    t.ms = +((t.s / 100) * 10500 * (t.rho / 1000)).toFixed(1);
    t.mt = +(t.mp + t.ms).toFixed(1);
  }
  S.ct.forEach(syncMass);

  function stepLocal(dtReal) {
    const dt = dtReal * S.spd; S.time = Math.round(S.time + dt);
    if (S.esd) {
      S.comp.forEach(c => c[0]=0);
      S.ct.forEach(t => { t.dp[0]=0; t.ds[0]=0; t.fv=0; });
      S.man.ev = [0,0]; S.man.op = [0,0];
    }
    const swEff = Math.round(Math.max(58, Math.min(100, 100 - Math.max(0, S.sw - 15) * 1.35)));
    const flowSys = [0,0], hotSys = [false,false];
    S.comp.forEach(c => {
      c[2] = c[0]===1 ? swEff : (c[0]===2 ? 100 : 0);
      c[3] = +(6.5 + S.sw * 0.22).toFixed(1);
      if (c[0]>0) { flowSys[c[1]] += 3.2 * (c[2]/100); if (c[0]===2) hotSys[c[1]]=true; }
    });

    let sumW = 18650, sumMx = 18650 * -8.45, sumMy = 0, sumMz = 18650 * 13.8, sumFsm = 0;
    const lcgCt = [64.2, 22.8, -19.5, -61.0];
    S.man.q = [0,0]; S.man.mast = [0,0,0,0];

    S.ct.forEach((t, i) => {
      const gr = GRADES[t.g];
      t.rho = +(gr.r15 * (1 - gr.a * (t.tl - 15))).toFixed(1);
      const rhoRef = gr.r15 * (1 - gr.a * (gr.tr - 15));
      t.fl = +Math.min(98, 98 * (rhoRef / t.rho)).toFixed(1);

      if (t.bhd && dt > 0) {
        const d = (t.mp - t.ms) + S.heel * 12.0;
        t.mp = +(t.mp - d*0.15).toFixed(1); t.ms = +(t.ms + d*0.15).toFixed(1);
      }
      const mOpen = S.man.sc[t.sys] && S.man.ev[t.sys] && S.man.gv[t.sys];
      [t.dp, t.ds].forEach((p, sd) => {
        if (p[0]) {
          const q = Math.round((p[1]/100) * 620);
          p[2] = mOpen && S.man.op[t.sys]===1 ? q : 0;
          p[3] = mOpen ? S.man.sb[t.sys] + 1.5 : 13.5;
          p[4] = Math.round(45 + (q/600)*115);
          if (dt > 0 && mOpen && S.man.op[t.sys]===1) {
            const dmt = (q/3600)*dt*(t.rho/1000);
            if (sd===0) t.mp = Math.max(0, +(t.mp - dmt).toFixed(1));
            else t.ms = Math.max(0, +(t.ms - dmt).toFixed(1));
          }
          S.man.q[t.sys] += p[2];
        } else { p[2]=0; p[3]=0; p[4]=0; }
      });
      if (S.man.op[t.sys]===2 && mOpen && t.fv && dt > 0) {
        const dmt = (600/3600)*dt*(t.rho/1000);
        t.mp += dmt; t.ms += dmt; S.man.q[t.sys] += 1200;
      }

      // Ambient Air & Seawater Heat Ingress + Spray Cooling (Conserves Mass!)
      if (dt > 0) {
        const dtHr = dt / 3600;
        t.ts = +(t.ts + (S.air - t.ts) * 0.008 * dtHr).toFixed(2);
        t.bs = +(t.bs + (S.sw - t.bs) * 0.006 * dtHr).toFixed(2);
      }
      const nOpen = t.vr.reduce((a,b)=>a+b, 0);
      t.lact = (t.vsuc && t.cin && nOpen > 0 && flowSys[t.sys] > 0) ? 1 : 0;
      if (t.lact && dt > 0) {
        const tSup = hotSys[t.sys] ? 68 : gr.tb - 1;
        const rate = ((tSup - t.ts) * 0.08 * (swEff/100)) * (dt / 3600);
        t.ts = +(t.ts + rate).toFixed(2); t.bs = +(t.bs + rate*0.85).toFixed(2);
        t.tv = +(t.tv + rate*1.1).toFixed(2); t.cr = +((rate / (dt/3600))).toFixed(2);
      } else { t.cr = 0.0; }

      t.mt = +(t.mp + t.ms).toFixed(1);
      t.p = +Math.min(98, (t.mp / (10500 * (t.rho/1000))) * 100).toFixed(1);
      t.s = +Math.min(98, (t.ms / (10500 * (t.rho/1000))) * 100).toFixed(1);

      const pEq = Math.max(10, (Math.pow(10, gr.A - gr.B / (t.tl + 273.15 + gr.C)) - 1.013) * 1000);
      t.pr = Math.round(t.pr * 0.8 + pEq * 0.2);
      if (t.pr >= 250) { S.man.marvs[i] = 1; S.man.mast[i] = Math.round((t.pr-245)*80); }
      else S.man.marvs[i] = 0;

      sumW += t.mt; sumMx += t.mt * lcgCt[i];
      sumMy += (t.ms - t.mp) * 7.5;
      sumMz += t.mt * (1.8 + ((t.p+t.s)/200)*10.2);
      if (t.p > 2 && t.p < 95) sumFsm += 1100;
    });

    // Ballast Pumps (Singular = 1250 m3/h max, Parallel = 2320 m3/h max, Branch = 650 m3/h max)
    const nBp = S.bal.pr[0] + S.bal.pr[1];
    const maxBpCap = nBp === 0 ? 0 : (nBp === 1 ? 1250 : 2320);
    const openWbt = S.wbt.filter(w => w[2] === 1).length;
    S.bal.q = (nBp > 0 && S.bal.op > 0 && openWbt > 0) ? Math.min(maxBpCap, openWbt * 650) : 0;
    const qBr = openWbt > 0 ? Math.round(S.bal.q / openWbt) : 0;
    [0,1].forEach(p => {
      S.bal.pq[p] = S.bal.pr[p] ? Math.round(S.bal.q / Math.max(1, nBp)) : 0;
      S.bal.pb[p] = S.bal.pr[p] ? +(3.4 - (S.bal.pq[p]/1250)*0.85).toFixed(2) : 0;
      S.bal.pa[p] = S.bal.pr[p] ? Math.round(68 + (S.bal.pq[p]/1250)*112) : 0;
    });

    S.wbt.forEach((w) => {
      w[3] = (w[2] && S.bal.q > 0) ? (S.bal.op===2 ? -qBr : qBr) : 0;
      if (dt > 0 && w[3] !== 0) w[0] = +Math.max(0, Math.min(100, w[0] + ((w[3]/3600)*dt/25))).toFixed(1);
      w[1] = w[0] <= 45 ? 0 : (w[0] <= 70 ? 1 : 2);
      const wm = (w[0]/100) * 2500 * 1.025;
      sumW += wm; sumMz += wm * (w[1]===0 ? 2.0 : (w[1]===1 ? 7.5 : 14.5));
      sumFsm += (w[1]===1 ? 85 : 550);
    });

    S.disp = Math.round(sumW);
    S.tm = +(4.0 + (sumW - 23400) / 6800).toFixed(2);
    S.kmt = +(22.5 - S.tm * 0.55).toFixed(2);
    S.kg = +(sumMz / sumW).toFixed(2);
    S.fsc = +(sumFsm / sumW).toFixed(2);
    S.gm = +Math.max(0.3, S.kmt - S.kg - S.fsc).toFixed(2);
    S.lcg = +(sumMx / sumW).toFixed(2);
    S.trim = +((sumW * (S.lcg - S.lcb)) / (S.mctc * 100)).toFixed(2);
    S.tf = +(S.tm + S.trim * 0.48).toFixed(2);
    S.ta = +(S.tm - S.trim * 0.52).toFixed(2);
    S.heel = +(Math.atan2(sumMy / sumW, S.gm) * (180/Math.PI)).toFixed(2);
  }

  function parseCmd(url) {
    const q = url.split('?')[1]; if (!q) return;
    const p = new URLSearchParams(q);
    if (p.has('spd')) S.spd = +p.get('spd');
    if (p.has('air')) S.air = +p.get('air');
    if (p.has('sw')) S.sw = +p.get('sw');
    if (p.has('ct')) { const i = +p.get('ct'), s = +p.get('side'), v = +p.get('v'); if (s===0) S.ct[i].p = v; else S.ct[i].s = v; syncMass(S.ct[i]); }
    if (p.has('bhd')) { const i = +p.get('bhd'); S.ct[i].bhd = 1 - S.ct[i].bhd; }
    if (p.has('grade')) { const i = +p.get('grade'), g = +p.get('g'); S.ct[i].g = g; S.ct[i].tl = GRADES[g].tb + 0.5; syncMass(S.ct[i]); }
    if (p.has('sys')) { const i = +p.get('sys'); S.ct[i].sys = 1 - S.ct[i].sys; }
    if (p.has('vsuc')) { const i = +p.get('vsuc'); S.ct[i].vsuc = 1 - S.ct[i].vsuc; }
    if (p.has('cin')) { const i = +p.get('cin'); S.ct[i].cin = 1 - S.ct[i].cin; }
    if (p.has('fvlv')) { const i = +p.get('fvlv'); S.ct[i].fv = 1 - S.ct[i].fv; }
    if (p.has('vret')) { const i = +p.get('vret'), idx = (+p.get('side'))*3 + (+p.get('br')); S.ct[i].vr[idx] = 1 - S.ct[i].vr[idx]; }
    if (p.has('pstart')) { const i = +p.get('pstart'), s = +p.get('side'); const arr = s===0 ? S.ct[i].dp : S.ct[i].ds; arr[0] = 1 - arr[0]; }
    if (p.has('pthr')) { const i = +p.get('pthr'), s = +p.get('side'), v = +p.get('v'); if (s===0) S.ct[i].dp[1]=v; else S.ct[i].ds[1]=v; }
    if (p.has('sconn')) { const s = +p.get('sconn'); S.man.sc[s] = 1 - S.man.sc[s]; }
    if (p.has('mesd')) { const s = +p.get('mesd'); S.man.ev[s] = 1 - S.man.ev[s]; }
    if (p.has('mgate')) { const s = +p.get('mgate'); S.man.gv[s] = 1 - S.man.gv[s]; }
    if (p.has('mop')) { S.man.op[+p.get('mop')] = +p.get('m'); }
    if (p.has('htr')) S.man.htr = 1 - S.man.htr;
    if (p.has('bst')) S.man.bst = 1 - S.man.bst;
    if (p.has('comp')) S.comp[+p.get('comp')][0] = +p.get('m');
    if (p.has('csys')) { const c = +p.get('csys'); S.comp[c][1] = 1 - S.comp[c][1]; }
    if (p.has('xo')) { const x = +p.get('xo'); S.xo[x] = 1 - S.xo[x]; }
    if (p.has('wbt')) S.wbt[+p.get('wbt')][0] = +p.get('v');
    if (p.has('bpr')) { const b = +p.get('bpr'); S.bal.pr[b] = 1 - S.bal.pr[b]; }
    if (p.has('bop')) S.bal.op = +p.get('bop');
    if (p.has('bxo')) S.bal.xo = 1 - S.bal.xo;
    if (p.has('bvlv')) { const j = +p.get('bvlv'); S.wbt[j][2] = 1 - S.wbt[j][2]; }
    if (p.has('wtgt')) S.wtgt = +p.get('wtgt');
    if (p.has('leak')) { const z = +p.get('leak'); S.fgd[z][3] = 1 - S.fgd[z][3]; }
    if (p.has('esd')) {
      const e = +p.get('esd');
      if (e===1) { S.esd=1; S.cause="MANUAL ESD-1 PUSHBUTTON"; }
      else if (e===0) { S.esd=0; S.cause="ESD RESET - NORMAL"; S.fgd.forEach(z=>{z[0]=0;z[3]=0;z[4]=0;}); }
    }
    if (p.has('preset')) {
      const idx = +p.get('preset');
      if (idx === -1) {
        S.presets.push({...PRESETS[0], n:"Custom Snapshot #" + (S.presets.length - 11), c:"CUSTOM", p:S.ct[0].p});
      } else if (S.presets[idx]) {
        const pr = S.presets[idx]; S.esd = 0;
        S.comp.forEach(c => c[0] = pr.cm); S.man.op = [pr.mop, pr.mop];
        S.ct.forEach(t => {
          t.g = t.sys===0 ? pr.g1 : pr.g2; t.p = pr.p; t.s = pr.p;
          t.ts = pr.ts; t.tv = pr.tv; t.tl = pr.tl; t.pr = pr.pr;
          t.hc = pr.hc; t.ig = pr.ig; t.o2 = pr.o2; t.prg = pr.prg;
          t.vr = [...pr.vr]; t.dp[0] = pr.dwp; t.ds[0] = pr.dwp; t.fv = pr.fv;
          syncMass(t);
        });
        S.wbt.forEach(w => w[0] = pr.wbt);
      }
    }
  }

  const origFetch = window.fetch;
  let serverlessMode = (location.protocol === 'file:' || location.hostname.includes('github.io'));
  window.fetch = async function(url, opts) {
    if (typeof url === 'string' && url.includes('/state')) {
      if (!serverlessMode) {
        try {
          const r = await origFetch(url, opts);
          if (r.ok) return r;
        } catch(e) { serverlessMode = true; }
      }
      parseCmd(url);
      stepLocal(0.3);
      return { ok: true, json: async () => S };
    }
    return origFetch(url, opts);
  };
})();
