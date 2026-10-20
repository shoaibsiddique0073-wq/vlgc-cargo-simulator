#pragma once
#include "hydro_stress.hpp"
#include "thermo.hpp"
#include "piping_pumps.hpp"
#include "safety_fgd.hpp"
#include "presets.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>

struct Compressor {
    int mode = 0;            // 0=STOP, 1=RELIQ, 2=HOT GAS
    int sys_id = 0;          // 0=Sys 1, 1=Sys 2
    double nom_flow_kg_s = 3.2;
    double act_flow_kg_s = 0.0;
    double cool_eff_pct  = 96.0; // Seawater condenser cooling efficiency (%)
    double cond_bar      = 8.5;  // Condenser discharge pressure (bar g)
};

struct VLGCEngine {
    std::array<std::array<double, 2>, 4> ct_m3 = {{{5250, 5250}, {5250, 5250}, {5250, 5250}, {5250, 5250}}};
    std::array<bool, 4> bhd_open = {true, true, true, true};
    std::array<TankThermo, 4> th;
    std::array<std::array<DeepwellPump, 2>, 4> dwp;
    LiquidManifold man;
    BallastSystem bal;
    std::array<Compressor, 4> comp;
    SafetyFGDSystem fgd;
    std::vector<SimPreset> presets;
    bool vap_xover = false, cond_xover = false, liq_xover = false;

    std::array<double, 10> wbt_m3 = {1100, 1100, 1300, 1300, 1300, 1300, 1100, 1100, 600, 800};
    int wbt_target = 0;

    // User-Configurable Ambient Air & Seawater Temperatures (C)
    double amb_air_c = 25.0;
    double amb_sw_c  = 22.0;

    double time_speed = 1.0, sim_seconds = 0.0;
    HydroState hydro;

    VLGCEngine() {
        presets = get_default_presets();
        th[0].sys_group = 0; th[1].sys_group = 1; th[2].sys_group = 0; th[3].sys_group = 1;
        comp[0].sys_id = 0;  comp[1].sys_id = 0;  comp[2].sys_id = 1;  comp[3].sys_id = 1;
        for (int i = 0; i < 4; ++i) th[i].sync_mass_from_vol(ct_m3[i][0], ct_m3[i][1]);
    }

    void apply_preset(int idx) {
        if (idx < 0 || idx >= (int)presets.size()) return;
        const auto& p = presets[idx];
        fgd.reset_esd();
        for (int c = 0; c < 4; ++c) comp[c].mode = p.comp_mode;
        man.op_mode = {p.man_op, p.man_op};
        man.shore_conn = {true, true}; man.esd_vlv = {true, true}; man.gate_vlv = {true, true};
        man.heater_on = p.htr_on; man.booster_on = p.bst_on;
        bal.pump_run = {false, false};

        for (int i = 0; i < 4; ++i) {
            int g = (th[i].sys_group == 0) ? p.grade_sys1 : p.grade_sys2;
            th[i].grade_id = g;
            double v_half = (p.ct_fill_pct / 100.0) * CT_GEOM[i].max_half_m3;
            ct_m3[i] = {v_half, v_half};
            bhd_open[i] = true;
            th[i].t_top_shell = (g == 1 && p.t_shell_c < -10.0) ? 1.5 : p.t_shell_c;
            th[i].t_bot_shell = th[i].t_top_shell - 1.2;
            th[i].t_vapor     = (g == 1 && p.t_vap_c < -10.0) ? 0.5 : p.t_vap_c;
            th[i].t_liquid    = (g == 1 && p.t_liq_c < -10.0) ? -0.3 : p.t_liq_c;
            th[i].press_mbarg = p.press_mbarg;
            th[i].hc_pct = p.hc_pct; th[i].ig_pct = p.ig_pct; th[i].o2_pct = p.o2_pct;
            th[i].purge_mode = p.purge_mode;
            th[i].vap_suc_vlv = true; th[i].cond_in_vlv = true;
            th[i].v_ret = {{{p.top_spray, p.bot_spray, p.bot_sparge}, {p.top_spray, p.bot_spray, p.bot_sparge}}};
            th[i].sync_mass_from_vol(v_half, v_half); // Lock exact cargo mass for preset!
            dwp[i][0].running = p.dwp_run; dwp[i][1].running = p.dwp_run;
            dwp[i][0].throttle_pct = 55.0; dwp[i][1].throttle_pct = 55.0;
            dwp[i][0].trip_code = 0;       dwp[i][1].trip_code = 0;
            man.fill_vlv[i] = p.fill_open;
        }
        for (int j = 0; j < 10; ++j) wbt_m3[j] = (p.wbt_fill_pct / 100.0) * WBT_GEOM[j].max_m3;
        if (idx == 11) {
            man.gate_vlv = {false, false};
            man.liq_pt_bar = {10.6, 10.6};
        }
    }

    void save_custom_preset() {
        const auto& t0 = th[0];
        double pct = ((ct_m3[0][0] + ct_m3[0][1]) / (2.0 * CT_GEOM[0].max_half_m3)) * 100.0;
        double wpct = (wbt_m3[0] / WBT_GEOM[0].max_m3) * 100.0;
        std::string nm = "Custom Snapshot #" + std::to_string(presets.size() - 11);
        presets.push_back({
            nm, "CUSTOM", th[0].grade_id, th[1].grade_id, pct,
            t0.t_top_shell, t0.t_vapor, t0.t_liquid, t0.press_mbarg,
            t0.hc_pct, t0.ig_pct, t0.o2_pct, t0.purge_mode, comp[0].mode,
            t0.v_ret[0][0], t0.v_ret[0][1], t0.v_ret[0][2],
            man.op_mode[0], dwp[0][0].running, man.fill_vlv[0], man.heater_on, man.booster_on, wpct
        });
    }

    void step(double real_dt_sec) {
        double dt = real_dt_sec * time_speed;
        sim_seconds += dt;
        bool alarm_triggered = false;

        if (fgd.esd1_active) {
            for (int c = 0; c < 4; ++c) comp[c].mode = 0;
            for (int i = 0; i < 4; ++i) {
                dwp[i][0].running = false; dwp[i][1].running = false;
                man.fill_vlv[i] = false;
            }
            man.esd_vlv = {false, false};
            man.op_mode = {0, 0};
            man.booster_on = false; man.heater_on = false;
        }

        // 1. Vapour & Condensate Header Lineups + Seawater Condenser Efficiency
        bool vap_avail[2] = {false, false};
        int cond_rec[2] = {0, 0}, suc_tanks[2] = {0, 0};
        double v_p_sum[2] = {0, 0}, v_t_sum[2] = {0, 0};

        for (int i = 0; i < 4; ++i) {
            int s = th[i].sys_group;
            if (th[i].vap_suc_vlv) {
                if (th[i].press_mbarg > 12.0) vap_avail[s] = true;
                v_p_sum[s] += th[i].press_mbarg / 1000.0;
                v_t_sum[s] += th[i].t_vapor;
                suc_tanks[s]++;
            }
            bool br_open = false;
            for (int sd = 0; sd < 2; ++sd) for (int b = 0; b < 3; ++b) if (th[i].v_ret[sd][b]) br_open = true;
            if (th[i].cond_in_vlv && br_open) cond_rec[s]++;
        }
        if (vap_xover && (vap_avail[0] || vap_avail[1])) vap_avail[0] = vap_avail[1] = true;

        // Seawater Temperature governs Condenser Saturation Pressure & Compressor Cooling Efficiency
        double t_cond_c = amb_sw_c + 5.5;
        double sw_eff = std::clamp(100.0 - std::max(0.0, amb_sw_c - 15.0) * 1.35, 58.0, 100.0);
        double flow_sys[2] = {0.0, 0.0};
        bool hot_sys[2] = {false, false};

        for (int c = 0; c < 4; ++c) {
            int s = comp[c].sys_id;
            int g_ref = (s == 0) ? th[0].grade_id : th[1].grade_id;
            double p_cond_mbarg = th[s].sat_press_mbarg(t_cond_c);
            comp[c].cond_bar = std::clamp(p_cond_mbarg / 1000.0 + (g_ref == 1 ? 1.8 : 0.8), 2.2, 18.5);
            comp[c].cool_eff_pct = (comp[c].mode == 1) ? sw_eff : (comp[c].mode == 2 ? 100.0 : 0.0);

            if (comp[c].mode > 0 && vap_avail[s]) {
                comp[c].act_flow_kg_s = comp[c].nom_flow_kg_s * (comp[c].mode == 1 ? (sw_eff / 100.0) : 1.0);
                flow_sys[s] += comp[c].act_flow_kg_s;
                if (comp[c].mode == 2) hot_sys[s] = true;
            } else {
                comp[c].act_flow_kg_s = 0.0;
            }
        }
        if (cond_xover) {
            double tot = flow_sys[0] + flow_sys[1];
            flow_sys[0] = flow_sys[1] = tot * 0.5;
            if (hot_sys[0] || hot_sys[1]) hot_sys[0] = hot_sys[1] = true;
            int tot_rec = cond_rec[0] + cond_rec[1];
            cond_rec[0] = cond_rec[1] = std::max(1, tot_rec);
        }

        for (int s = 0; s < 2; ++s) {
            man.vap_pt_bar[s]  = suc_tanks[s] > 0 ? (v_p_sum[s] / suc_tanks[s]) : 0.05;
            man.vap_tt_c[s]    = suc_tanks[s] > 0 ? (v_t_sum[s] / suc_tanks[s]) : amb_air_c;
            man.cond_pt_bar[s] = flow_sys[s] > 0.1 ? comp[s * 2].cond_bar : man.vap_pt_bar[s];
            man.cond_tt_c[s]   = flow_sys[s] > 0.1 ? (hot_sys[s] ? 68.0 : t_cond_c) : man.vap_tt_c[s];
        }

        // 2. Step 8 Deepwell Cargo Pumps & Shore Manifold Loading/Discharging (Mass-Conserving)
        man.total_flow_m3_hr = {0.0, 0.0};
        man.mast_vent_kg_hr  = {0.0, 0.0, 0.0, 0.0};
        bool pmp_on_sys[2]   = {false, false};
        double weighted_t = 0.0, total_q = 0.0;

        for (int i = 0; i < 4; ++i) {
            int sys = th[i].sys_group;
            bool m_open = man.is_manifold_open(sys);
            double rho_t = th[i].liq_density(th[i].t_liquid) / 1000.0;
            double sat_p = th[i].sat_press_mbarg(th[i].t_liquid);
            double max_fl = th[i].max_fill_limit();
            double max_half = CT_GEOM[i].max_half_m3 * (max_fl / 100.0);

            for (int side = 0; side < 2; ++side) {
                if (dwp[i][side].running) pmp_on_sys[sys] = true;
                double eff_bp = (man.op_mode[sys] == 1 && m_open) ?
                    (man.shore_bar[sys] - (man.booster_on ? 2.5 : 0.0)) : 13.8;
                double& half_mass = (side == 0) ? th[i].mass_p_mt : th[i].mass_s_mt;
                if (dwp[i][side].step_pump(dt, ct_m3[i][side], half_mass, CT_GEOM[i].max_half_m3,
                                           rho_t, th[i].press_mbarg, sat_p, std::max(0.5, eff_bp))) {
                    alarm_triggered = true;
                }
                if (m_open && man.op_mode[sys] == 1) {
                    man.total_flow_m3_hr[sys] += dwp[i][side].flow_m3_hr;
                    weighted_t += dwp[i][side].flow_m3_hr * th[i].t_liquid;
                    total_q += dwp[i][side].flow_m3_hr;
                }
            }

            if (man.op_mode[sys] == 2 && m_open && man.fill_vlv[i] && dt > 0.0) {
                int n_open = 0;
                for (int k = 0; k < 4; ++k) if (th[k].sys_group == sys && man.fill_vlv[k]) n_open++;
                double rate_tk = man.load_rate_m3_hr[sys] / std::max(1, n_open);
                double dv_half = std::min(max_half - ct_m3[i][0], (rate_tk / 3600.0) * dt * 0.5);
                if (dv_half > 0.0) {
                    ct_m3[i][0] += dv_half; ct_m3[i][1] += dv_half;
                    th[i].mass_p_mt += dv_half * rho_t;
                    th[i].mass_s_mt += dv_half * rho_t;
                    man.total_flow_m3_hr[sys] += rate_tk;
                }
            }
        }

        // Liquid Header PT/TT & Pipeline Thermal Relief Valve (Governed by Ambient Air Temp)
        for (int s = 0; s < 2; ++s) {
            bool m_open = man.is_manifold_open(s);
            if (man.total_flow_m3_hr[s] > 1.0) {
                man.liq_pt_bar[s] = man.shore_bar[s] + (man.op_mode[s] == 1 ? 1.8 : 0.6);
                man.liq_tt_c[s]   = (s == 0) ? th[0].t_liquid : th[1].t_liquid;
            } else if (pmp_on_sys[s] && !m_open) {
                man.liq_pt_bar[s] = 12.8;
            } else if (!m_open && dt > 0.0) {
                double dt_hr = dt / 3600.0;
                double d_temp = (amb_air_c - man.liq_tt_c[s]) * 0.35 * dt_hr;
                man.liq_tt_c[s]   += d_temp;
                man.liq_pt_bar[s] += std::max(0.0, d_temp * 2.4);
            } else {
                man.liq_pt_bar[s] = man.shore_bar[s];
            }

            if (man.liq_pt_bar[s] >= 10.0) {
                man.trv_lifted[s] = true;
                man.mast_vent_kg_hr[s] += (man.liq_pt_bar[s] - 9.5) * 180.0;
                if (dt > 0.0) man.liq_pt_bar[s] = std::max(9.8, man.liq_pt_bar[s] - 1.5 * (dt / 60.0));
            } else {
                man.trv_lifted[s] = false;
            }
        }

        double base_t = (total_q > 1.0) ? (weighted_t / total_q) : th[0].t_liquid;
        man.export_temp_c = man.heater_on ? std::max(amb_sw_c - 5.0, base_t + 38.0) : base_t;
        man.export_bar = man.shore_bar[0] + (man.booster_on ? 5.5 : 0.0);

        // 3. Step Tank Thermodynamics, Centerline Bulkhead Mass Equalization & MARVS
        double avg_rho = 0.0;
        std::array<double, 4> fill_arr{}, fl_arr{}, pr_arr{};
        std::array<int, 4> gr_arr{};

        for (int i = 0; i < 4; ++i) {
            double max_fl = th[i].max_fill_limit();
            double rho_t = th[i].liq_density(th[i].t_liquid) / 1000.0;
            if (bhd_open[i] && dt > 0.0) {
                // Equalize Port/Stbd Mass across open bulkhead valve without changing total tank mass!
                double heel_bias_mt = hydro.heel_deg * 85.0 * rho_t;
                double diff_mt = (th[i].mass_p_mt - th[i].mass_s_mt) + heel_bias_mt;
                double xfer_mt = diff_mt * std::min(0.85, 0.08 * std::max(1.0, dt));
                th[i].mass_p_mt = std::max(0.0, th[i].mass_p_mt - xfer_mt * 0.5);
                th[i].mass_s_mt = std::max(0.0, th[i].mass_s_mt + xfer_mt * 0.5);
            }

            int s = th[i].sys_group;
            // Closed-loop mass balance: if condensate return is active, suction = condensate return
            double cond_to_tk = (cond_rec[s] > 0) ? (flow_sys[s] / cond_rec[s]) : 0.0;
            double suc_from_tk = (cond_rec[s] > 0) ? cond_to_tk :
                ((th[i].vap_suc_vlv && suc_tanks[s] > 0) ? (flow_sys[s] / suc_tanks[s]) : 0.0);

            th[i].step_thermo(dt, ct_m3[i], CT_GEOM[i].max_half_m3, amb_air_c, amb_sw_c,
                              cond_to_tk, hot_sys[s], suc_from_tk);

            if (th[i].press_mbarg >= 250.0) {
                man.marvs_lifted[i] = true;
                double vent_rate_kg_hr = (th[i].press_mbarg - 245.0) * 85.0;
                man.mast_vent_kg_hr[i] += vent_rate_kg_hr;
                if (dt > 0.0) {
                    th[i].vap_mass_kg = std::max(100.0, th[i].vap_mass_kg - (vent_rate_kg_hr / 3600.0) * dt);
                    th[i].press_mbarg = std::max(248.0, th[i].press_mbarg - 8.0 * (dt / 60.0));
                }
            } else {
                man.marvs_lifted[i] = false;
            }

            avg_rho += rho_t;
            fill_arr[i] = ((ct_m3[i][0] + ct_m3[i][1]) / (2.0 * CT_GEOM[i].max_half_m3)) * 100.0;
            fl_arr[i] = max_fl; pr_arr[i] = th[i].press_mbarg; gr_arr[i] = th[i].grade_id;
        }
        avg_rho *= 0.25;

        if (fgd.step_safety(dt, fill_arr, fl_arr, pr_arr, gr_arr)) {
            alarm_triggered = true;
            for (int i = 0; i < 4; ++i) if (fgd.hhl_98_trip[i]) man.fill_vlv[i] = false;
        }

        // 4. Step Ballast Pumps (Singular vs Parallel) & WBT Branch Lines
        bal.step_ballast(dt, wbt_m3, WBT_GEOM);

        if (alarm_triggered && time_speed > 1.0) time_speed = 1.0;
        HullSolver::solve(ct_m3, bhd_open, avg_rho, wbt_m3, hydro);
    }

    std::string to_json() const {
        std::ostringstream ss;
        char b[960];
        std::snprintf(b, sizeof(b),
            "{\"spd\":%.0f,\"time\":%.0f,\"air\":%.1f,\"sw\":%.1f,"
            "\"disp\":%.0f,\"tf\":%.2f,\"ta\":%.2f,\"tm\":%.2f,"
            "\"trim\":%.2f,\"heel\":%.2f,\"tpc\":%.1f,\"mctc\":%.0f,\"lcb\":%.2f,\"lcf\":%.2f,\"lcg\":%.2f,"
            "\"kmt\":%.2f,\"kg\":%.2f,\"fsc\":%.2f,\"gm\":%.2f,"
            "\"sf\":%.1f,\"bm\":%.1f,\"hsf\":%.1f,\"hbm\":%.1f,\"wtgt\":%d,\"xo\":[%d,%d,%d],"
            "\"esd\":%d,\"cause\":\"%s\",\"mdp\":%.0f,\"cvent\":%d,\"floop\":%d,"
            "\"bal\":{\"pr\":[%d,%d],\"op\":%d,\"xo\":%d,\"q\":%.0f,\"pq\":[%.0f,%.0f],\"pb\":[%.2f,%.2f],\"pa\":[%.0f,%.0f]},"
            "\"man\":{\"op\":[%d,%d],\"sc\":[%d,%d],\"ev\":[%d,%d],\"gv\":[%d,%d],"
            "\"lpt\":[%.1f,%.1f],\"ltt\":[%.1f,%.1f],\"vpt\":[%.2f,%.2f],\"vtt\":[%.1f,%.1f],"
            "\"cpt\":[%.1f,%.1f],\"ctt\":[%.1f,%.1f],\"trv\":[%d,%d],\"marvs\":[%d,%d,%d,%d],"
            "\"mast\":[%.0f,%.0f,%.0f,%.0f],\"sb\":[%.1f,%.1f],\"q\":[%.0f,%.0f],\"htr\":%d,\"bst\":%d,\"et\":%.1f,\"eb\":%.1f},",
            time_speed, sim_seconds, amb_air_c, amb_sw_c,
            hydro.disp_mt, hydro.tf, hydro.ta, hydro.tm,
            hydro.trim_m, hydro.heel_deg, hydro.tpc, hydro.mctc, hydro.lcb, hydro.lcf, hydro.lcg,
            hydro.kmt, hydro.kg_solid, hydro.fsc, hydro.gm_fluid,
            hydro.max_sf_pct, hydro.max_bm_pct, hydro.harbor_sf_pct, hydro.harbor_bm_pct,
            wbt_target, vap_xover?1:0, cond_xover?1:0, liq_xover?1:0,
            fgd.esd1_active?1:0, fgd.last_cause.c_str(), fgd.motor_rm_dp_pa, fgd.comp_vent_on?1:0, fgd.fire_loop_ok?1:0,
            bal.pump_run[0]?1:0, bal.pump_run[1]?1:0, bal.op_mode, bal.ring_xover?1:0, bal.total_flow_m3_hr,
            bal.pump_flow_m3_hr[0], bal.pump_flow_m3_hr[1], bal.pump_bar[0], bal.pump_bar[1], bal.pump_amps[0], bal.pump_amps[1],
            man.op_mode[0], man.op_mode[1], man.shore_conn[0]?1:0, man.shore_conn[1]?1:0,
            man.esd_vlv[0]?1:0, man.esd_vlv[1]?1:0, man.gate_vlv[0]?1:0, man.gate_vlv[1]?1:0,
            man.liq_pt_bar[0], man.liq_pt_bar[1], man.liq_tt_c[0], man.liq_tt_c[1],
            man.vap_pt_bar[0], man.vap_pt_bar[1], man.vap_tt_c[0], man.vap_tt_c[1],
            man.cond_pt_bar[0], man.cond_pt_bar[1], man.cond_tt_c[0], man.cond_tt_c[1],
            man.trv_lifted[0]?1:0, man.trv_lifted[1]?1:0,
            man.marvs_lifted[0]?1:0, man.marvs_lifted[1]?1:0, man.marvs_lifted[2]?1:0, man.marvs_lifted[3]?1:0,
            man.mast_vent_kg_hr[0], man.mast_vent_kg_hr[1], man.mast_vent_kg_hr[2], man.mast_vent_kg_hr[3],
            man.shore_bar[0], man.shore_bar[1], man.total_flow_m3_hr[0], man.total_flow_m3_hr[1],
            man.heater_on?1:0, man.booster_on?1:0, man.export_temp_c, man.export_bar);
        ss << b;

        ss << "\"ct\":[";
        for (int i = 0; i < 4; ++i) {
            double p_pct = (ct_m3[i][0] / CT_GEOM[i].max_half_m3) * 100.0;
            double s_pct = (ct_m3[i][1] / CT_GEOM[i].max_half_m3) * 100.0;
            const auto& t = th[i]; const auto& dp = dwp[i][0]; const auto& ds = dwp[i][1];
            double rho = t.liq_density(t.t_liquid), mt = t.total_cargo_mt();
            std::snprintf(b, sizeof(b),
                "%s{\"p\":%.1f,\"s\":%.1f,\"bhd\":%d,\"g\":%d,\"sys\":%d,\"fl\":%.1f,\"pr\":%.0f,"
                "\"rho\":%.1f,\"mt\":%.1f,\"vsuc\":%d,\"cin\":%d,\"lact\":%d,"
                "\"ts\":%.2f,\"bs\":%.2f,\"tv\":%.2f,\"tl\":%.2f,\"cr\":%.2f,"
                "\"hc\":%.1f,\"ig\":%.1f,\"o2\":%.1f,\"prg\":%d,\"fv\":%d,"
                "\"vr\":[%d,%d,%d,%d,%d,%d],"
                "\"dp\":[%d,%.0f,%.0f,%.1f,%.0f,%d,%d],"
                "\"ds\":[%d,%.0f,%.0f,%.1f,%.0f,%d,%d]}",
                (i ? "," : ""), p_pct, s_pct, bhd_open[i]?1:0, t.grade_id, t.sys_group,
                t.max_fill_limit(), t.press_mbarg, rho, mt, t.vap_suc_vlv?1:0, t.cond_in_vlv?1:0, t.lineup_active?1:0,
                t.t_top_shell, t.t_bot_shell, t.t_vapor, t.t_liquid, t.cool_rate_c_hr,
                t.hc_pct, t.ig_pct, t.o2_pct, t.purge_mode, man.fill_vlv[i]?1:0,
                t.v_ret[0][0]?1:0, t.v_ret[0][1]?1:0, t.v_ret[0][2]?1:0,
                t.v_ret[1][0]?1:0, t.v_ret[1][1]?1:0, t.v_ret[1][2]?1:0,
                dp.running?1:0, dp.throttle_pct, dp.flow_m3_hr, dp.disch_bar, dp.amps, dp.cavitating?1:0, dp.trip_code,
                ds.running?1:0, ds.throttle_pct, ds.flow_m3_hr, ds.disch_bar, ds.amps, ds.cavitating?1:0, ds.trip_code);
            ss << b;
        }
        ss << "],\"comp\":[";
        for (int c = 0; c < 4; ++c) {
            std::snprintf(b, sizeof(b), "%s[%d,%d,%.0f,%.1f]",
                (c ? "," : ""), comp[c].mode, comp[c].sys_id, comp[c].cool_eff_pct, comp[c].cond_bar);
            ss << b;
        }
        ss << "],\"fgd\":[";
        for (int z = 0; z < 6; ++z) {
            const auto& zn = fgd.zones[z];
            std::snprintf(b, sizeof(b), "%s[%.1f,%.1f,%.0f,%d,%d]",
                (z ? "," : ""), zn.lel_pct, zn.o2_pct, zn.nh3_ppm, zn.leak_sim?1:0, zn.alarm_lvl);
            ss << b;
        }
        ss << "],\"wbt\":[";
        for (int j = 0; j < 10; ++j) {
            double pct = (wbt_m3[j] / WBT_GEOM[j].max_m3) * 100.0;
            int zone = (int)wbt_properties(j, pct)[3];
            std::snprintf(b, sizeof(b), "%s[%.1f,%d,%d,%.0f]",
                (j ? "," : ""), pct, zone, bal.wbt_vlv[j]?1:0, bal.branch_flow_m3_hr[j]);
            ss << b;
        }
        ss << "],\"presets\":[";
        for (size_t k = 0; k < presets.size(); ++k) {
            std::snprintf(b, sizeof(b), "%s{\"n\":\"%s\",\"c\":\"%s\"}",
                (k ? "," : ""), presets[k].name.c_str(), presets[k].category.c_str());
            ss << b;
        }
        ss << "],\"sf_c\":[";
        for (int s = 0; s < 20; ++s) { std::snprintf(b, sizeof(b), "%s%.1f", (s ? "," : ""), hydro.sf_stations[s]); ss << b; }
        ss << "],\"bm_c\":[";
        for (int s = 0; s < 20; ++s) { std::snprintf(b, sizeof(b), "%s%.1f", (s ? "," : ""), hydro.bm_stations[s]); ss << b; }
        ss << "]}";
        return ss.str();
    }
};
