#pragma once
#include "hydro_stress.hpp"
#include "fluid_props.hpp"
#include "comp_node.hpp"
#include "pump_node.hpp"
#include "tank_node.hpp"
#include "safety_fgd.hpp"
#include "presets_12.hpp"
#include <sstream>
#include <iomanip>

struct ManifoldSystem {
    int op_mode[2] = {1, 1};
    bool shore_conn[2] = {true, true}, esd_vlv[2] = {true, true};
    double gate_vlv_pct[2] = {100.0, 100.0}, shore_bar[2] = {4.0, 4.0}, flow_m3_hr[2] = {0.0, 0.0};
    double liq_pt_bar[2] = {4.0, 4.0}, liq_tt_c[2] = {-41.2, -0.3};
    double vap_pt_bar[2] = {0.095, 0.095}, vap_tt_c[2] = {-39.0, 0.5};
    double cond_pt_bar[2] = {8.5, 8.5}, cond_tt_c[2] = {30.0, 30.0};
    bool line_trv_lift[2] = {false, false}, marvs_lift[4] = {false, false, false, false};
    double mast_vent_kg_hr[4] = {0.0, 0.0, 0.0, 0.0};
    bool heater_on = false, booster_on = false;
    double export_temp_c = -41.5, export_bar = 4.0;
};

struct BallastSystem {
    bool pump_run[2] = {false, false};
    int op_mode = 1; // 0=HOLD, 1=BALLAST IN, 2=DEBALLAST, 3=HEEL P->S, 4=HEEL S->P, 5=GRAVITATE
    bool ring_xover = true;
    double wbt_vlv_pct[10] = {100.0, 100.0, 0, 0, 0, 0, 0, 0, 0, 0};
    double total_flow_m3_hr = 0.0, tank_flow_m3_hr[10] = {0};
    double pump_flow[2] = {0.0, 0.0}, pump_bar[2] = {0.0, 0.0}, pump_amps[2] = {0.0, 0.0};
};

struct VLGCEngine {
    double sim_time_s = 0.0, time_speed = 1.0, amb_air_c = 25.0, amb_sw_c = 22.0;
    std::array<std::array<double, 2>, 4> ct_m3{{{5250,5250},{5250,5250},{5250,5250},{5250,5250}}};
    std::array<bool, 4> bhd_open{true, true, true, true};
    std::array<TankThermo, 4> th{};
    std::array<std::array<DeepwellPump, 2>, 4> dwp{};
    std::array<CompressorUnit, 4> comp{};
    ManifoldSystem man{};
    BallastSystem bal{};
    SafetyFGDSystem fgd{};
    std::array<double, 10> wbt_m3{1125,1125,1125,1125,1125,1125,1125,1125,720,720};
    int wbt_target = 0;
    bool vap_xover = false, cond_xover = false, liq_xover = false;
    HydroState hydro{};
    std::vector<SimPreset> presets;

    VLGCEngine() {
        for (int i = 0; i < 4; ++i) {
            th[i].sys_group = (i < 2) ? 0 : 1;
            th[i].sync_mass_from_vol(ct_m3[i][0], ct_m3[i][1]);
            comp[i].sys_id = (i < 2) ? 0 : 1;
        }
        presets.assign(DEFAULT_PRESETS.begin(), DEFAULT_PRESETS.end());
    }

    void step(double dt_real) {
        double dt = dt_real * time_speed;
        sim_time_s += dt;
        fgd.step(dt);
        if (fgd.esd1_active) {
            for (auto& c : comp) c.mode = 0;
            for (int i = 0; i < 4; ++i) { dwp[i][0].running = false; dwp[i][1].running = false; th[i].fill_vlv_pct = 0.0; }
            man.esd_vlv[0] = false; man.esd_vlv[1] = false; man.op_mode[0] = 0; man.op_mode[1] = 0;
        }

        // 1. Solve Vapor Header Suction Conditions & Step Compressors
        double sys_suc_kpa[2] = {0.0, 0.0}, sys_vap_c[2] = {-38.0, -38.0};
        int sys_cnt[2] = {0, 0}, sys_g[2] = {0, 0};
        for (int i = 0; i < 4; ++i) {
            int s = th[i].sys_group; sys_g[s] = th[i].grade_id;
            if (th[i].vap_suc_vlv) { sys_suc_kpa[s] += th[i].press_kpa; sys_vap_c[s] += th[i].t_vapor; sys_cnt[s]++; }
        }
        for (int s = 0; s < 2; ++s) {
            if (sys_cnt[s] > 0) { sys_suc_kpa[s] /= sys_cnt[s]; sys_vap_c[s] /= sys_cnt[s]; }
            man.vap_pt_bar[s] = sys_suc_kpa[s] / 100.0; man.vap_tt_c[s] = sys_vap_c[s];
        }
        if (vap_xover) {
            double avg_p = 0.5 * (sys_suc_kpa[0] + sys_suc_kpa[1]);
            sys_suc_kpa[0] = sys_suc_kpa[1] = avg_p;
        }

        double sys_vap_draw_kg_s[2] = {0.0, 0.0};
        bool sys_hot[2] = {false, false};
        for (int c = 0; c < 4; ++c) {
            int s = comp[c].sys_id;
            comp[c].step(sys_g[s], sys_suc_kpa[s], sys_vap_c[s], amb_sw_c, sys_cnt[s] > 0);
            sys_vap_draw_kg_s[s] += comp[c].vap_draw_kg_s;
            if (comp[c].mode == 2) sys_hot[s] = true;
            man.cond_pt_bar[s] = comp[c].cond_bar;
            man.cond_tt_c[s] = (comp[c].mode == 2) ? comp[c].disch_temp_c : (amb_sw_c + 6.5);
        }
        if (cond_xover) {
            double tot = 0.5 * (sys_vap_draw_kg_s[0] + sys_vap_draw_kg_s[1]);
            sys_vap_draw_kg_s[0] = sys_vap_draw_kg_s[1] = tot;
        }

        // 2. Solve Liquid Manifold Network Backpressure & Loading Distribution
        man.flow_m3_hr[0] = 0.0; man.flow_m3_hr[1] = 0.0;
        double load_w[4] = {0,0,0,0}, sum_lw[2] = {0,0};
        for (int i = 0; i < 4; ++i) {
            th[i].load_flow_m3_hr = 0.0;
            int s = th[i].sys_group;
            double kv_f = std::pow(th[i].fill_vlv_pct / 100.0, 1.25);
            bool m_open = man.shore_conn[s] && man.esd_vlv[s] && (man.gate_vlv_pct[s] > 1.0);
            if (man.op_mode[s] == 2 && m_open && kv_f > 0.01) {
                double fill_pct = ((ct_m3[i][0] + ct_m3[i][1]) / (2.0 * CT_GEOM[i].max_half_m3)) * 100.0;
                double p_tk_bar = (th[i].press_kpa / 100.0) + (th[i].liq_density() * 9.81 * (fill_pct / 100.0) * 18.5) / 1e5;
                load_w[i] = kv_f * std::sqrt(std::max(0.25, man.shore_bar[s] + 1.8 - p_tk_bar));
                sum_lw[s] += load_w[i];
            }
        }
        // Calculate dynamic manifold backpressure from total active pumps
        double est_q_sys[2] = {0.0, 0.0};
        for (int i = 0; i < 4; ++i) {
            int s = th[i].sys_group;
            for (int sd = 0; sd < 2; ++sd) if (dwp[i][sd].running) est_q_sys[s] += dwp[i][sd].flow_m3_hr;
        }
        double hdr_back_bar[2] = {man.shore_bar[0], man.shore_bar[1]};
        for (int s = 0; s < 2; ++s) {
            double kv_g = std::max(0.05, std::pow(man.gate_vlv_pct[s] / 100.0, 1.25));
            double r_drop = std::pow(est_q_sys[s] / (1800.0 * kv_g), 2.0) * 3.5;
            hdr_back_bar[s] = man.shore_bar[s] + 0.6 + r_drop;
        }

        std::array<double, 4> rhos{};
        std::array<std::array<double, 2>, 4> masses{};
        for (int i = 0; i < 4; ++i) {
            man.mast_vent_kg_hr[i] = 0.0;
            int s = th[i].sys_group;
            double kv_g = std::pow(man.gate_vlv_pct[s] / 100.0, 1.25);
            bool m_open = man.shore_conn[s] && man.esd_vlv[s] && (kv_g > 0.01);
            double rho = th[i].liq_density(), p_sat = th[i].sat_press_kpa(th[i].t_liquid);

            // Torricelli Bulkhead Orifice Level Equalization
            if (bhd_open[i] && dt > 0.0) {
                double hp = (ct_m3[i][0] / CT_GEOM[i].max_half_m3) * 18.5;
                double hs = (ct_m3[i][1] / CT_GEOM[i].max_half_m3) * 18.5;
                double dh = (hp - hs) + 2.0 * (CT_GEOM[i].half_beam * 0.46) * std::tan(hydro.heel_deg * M_PI / 180.0);
                if (std::abs(dh) > 0.004) {
                    double q_orf = 0.65 * 0.125 * std::sqrt(2.0 * 9.81 * std::abs(dh));
                    double max_eq = (std::abs(dh) / 18.5) * CT_GEOM[i].max_half_m3 * (rho / 1000.0) * 0.45;
                    double dm = std::min(max_eq, q_orf * dt * (rho / 1000.0)) * (dh > 0.0 ? 1.0 : -1.0);
                    th[i].mass_p_mt = std::max(0.0, th[i].mass_p_mt - dm);
                    th[i].mass_s_mt = std::max(0.0, th[i].mass_s_mt + dm);
                }
            }

            double net_liq_m3_s = 0.0;
            for (int sd = 0; sd < 2; ++sd) {
                double f_side = (ct_m3[i][sd] / CT_GEOM[i].max_half_m3) * 100.0;
                dwp[i][sd].step(dt, f_side, rho, th[i].press_kpa, p_sat, hdr_back_bar[s], m_open && man.op_mode[s] == 1);
                if (dwp[i][sd].running && m_open && man.op_mode[s] == 1) {
                    double q_s = dwp[i][sd].flow_m3_hr / 3600.0;
                    double dm = q_s * dt * (rho / 1000.0);
                    if (sd == 0) th[i].mass_p_mt = std::max(0.0, th[i].mass_p_mt - dm);
                    else         th[i].mass_s_mt = std::max(0.0, th[i].mass_s_mt - dm);
                    net_liq_m3_s -= q_s;
                    man.flow_m3_hr[s] += dwp[i][sd].flow_m3_hr;
                }
            }

            if (load_w[i] > 0.0 && sum_lw[s] > 0.0) {
                double q_hdr = 2400.0 * kv_g * std::min(1.0, sum_lw[s] / 0.85);
                th[i].load_flow_m3_hr = q_hdr * (load_w[i] / sum_lw[s]);
                double q_s = th[i].load_flow_m3_hr / 3600.0;
                double dm = q_s * dt * (rho / 1000.0) * 0.5;
                th[i].mass_p_mt += dm; th[i].mass_s_mt += dm;
                net_liq_m3_s += q_s;
                man.flow_m3_hr[s] += th[i].load_flow_m3_hr;
            }

            double fill_tot = ((ct_m3[i][0] + ct_m3[i][1]) / (2.0 * CT_GEOM[i].max_half_m3)) * 100.0;
            double tk_vap_draw = (sys_cnt[s] > 0 && th[i].vap_suc_vlv) ? (sys_vap_draw_kg_s[s] / sys_cnt[s]) : 0.0;
            th[i].step(dt, fill_tot, amb_air_c, amb_sw_c, tk_vap_draw, sys_hot[s], net_liq_m3_s, man.mast_vent_kg_hr[i]);
            man.marvs_lift[i] = (th[i].press_kpa >= 25.0);

            th[i].sync_vol_from_mass(CT_GEOM[i].max_half_m3, ct_m3[i][0], ct_m3[i][1]);
            rhos[i] = th[i].liq_density() / 1000.0;
            masses[i][0] = th[i].mass_p_mt; masses[i][1] = th[i].mass_s_mt;
        }

        for (int s = 0; s < 2; ++s) {
            man.liq_pt_bar[s] = (man.flow_m3_hr[s] > 1.0) ? hdr_back_bar[s] : man.shore_bar[s];
            man.liq_tt_c[s] = th[s * 2].t_liquid;
        }
        man.export_temp_c = man.heater_on ? std::max(0.5, man.liq_tt_c[0] + 42.0) : man.liq_tt_c[0];
        man.export_bar = man.booster_on ? (man.liq_pt_bar[0] + 6.5) : man.liq_pt_bar[0];

        // 3. Ballast System & Hydrostatics
        int n_bp = (bal.pump_run[0] ? 1 : 0) + (bal.pump_run[1] ? 1 : 0);
        double max_bp = (n_bp == 0) ? 0.0 : (n_bp == 1 ? 1250.0 : 2320.0), sum_br = 0.0, br_c[10]{};
        for (int j = 0; j < 10; ++j) {
            double kv = std::pow(bal.wbt_vlv_pct[j] / 100.0, 1.25);
            if (kv < 0.01) continue;
            if (bal.op_mode == 5) {
                double f_pct = (wbt_m3[j] / WBT_GEOM[j].max_m3) * 100.0;
                double dh = hydro.tm - wbt_properties(j, f_pct)[0];
                if (std::abs(dh) > 0.15) br_c[j] = (dh > 0 ? 1.0 : -1.0) * std::min(650.0 * kv, kv * 175.0 * std::sqrt(std::abs(dh)));
            } else { br_c[j] = 650.0 * kv; sum_br += br_c[j]; }
        }
        if (bal.op_mode == 5) {
            bal.total_flow_m3_hr = 0.0;
            for (int j = 0; j < 10; ++j) { bal.tank_flow_m3_hr[j] = br_c[j]; bal.total_flow_m3_hr += std::abs(br_c[j]); if (dt > 0) wbt_m3[j] = std::clamp(wbt_m3[j] + (br_c[j]/3600.0)*dt, 0.0, WBT_GEOM[j].max_m3); }
        } else {
            bal.total_flow_m3_hr = (n_bp > 0 && bal.op_mode > 0 && sum_br > 0) ? std::min(max_bp, sum_br) : 0.0;
            for (int p = 0; p < 2; ++p) {
                bal.pump_flow[p] = bal.pump_run[p] ? (bal.total_flow_m3_hr / std::max(1, n_bp)) : 0.0;
                bal.pump_bar[p] = bal.pump_run[p] ? (3.4 - (bal.pump_flow[p]/1250.0)*0.85) : 0.0;
                bal.pump_amps[p] = bal.pump_run[p] ? (68.0 + (bal.pump_flow[p]/1250.0)*112.0) : 0.0;
            }
            for (int j = 0; j < 10; ++j) {
                bal.tank_flow_m3_hr[j] = (bal.total_flow_m3_hr > 0 && sum_br > 0) ? (br_c[j]/sum_br)*bal.total_flow_m3_hr*(bal.op_mode==2?-1.0:1.0) : 0.0;
                if (dt > 0) wbt_m3[j] = std::clamp(wbt_m3[j] + (bal.tank_flow_m3_hr[j]/3600.0)*dt, 0.0, WBT_GEOM[j].max_m3);
            }
        }
        HullSolver::solve(ct_m3, masses, bhd_open, rhos, wbt_m3, hydro);
    }
    void apply_preset(int idx) {
        if (idx < 0 || idx >= (int)presets.size()) return;
        const auto& pr = presets[idx];
        fgd.reset_esd();
        for (int c = 0; c < 4; ++c) comp[c].mode = pr.comp_mode;
        man.op_mode[0] = pr.man_op; man.op_mode[1] = pr.man_op;
        for (int i = 0; i < 4; ++i) {
            th[i].grade_id = (th[i].sys_group == 0) ? pr.grade_sys1 : pr.grade_sys2;
            ct_m3[i][0] = (pr.ct_fill_pct / 100.0) * CT_GEOM[i].max_half_m3;
            ct_m3[i][1] = (pr.ct_fill_pct / 100.0) * CT_GEOM[i].max_half_m3;
            th[i].t_top_shell = pr.t_shell_c; th[i].t_bot_shell = pr.t_liq_c + 0.3;
            th[i].t_vapor = pr.t_vap_c; th[i].t_liquid = pr.t_liq_c;
            th[i].press_kpa = pr.press_kpa;
            th[i].hc_pct = pr.hc_pct; th[i].ig_pct = pr.ig_pct; th[i].o2_pct = pr.o2_pct;
            th[i].purge_mode = pr.purge_mode;
            for (int sd = 0; sd < 2; ++sd) {
                for (int b = 0; b < 3; ++b) th[i].v_ret[sd][b] = pr.v_ret[b];
                dwp[i][sd].running = pr.dwp_run; dwp[i][sd].trip_code = 0;
            }
            th[i].fill_vlv_pct = pr.fill_vlv ? 100.0 : 0.0;
            th[i].sync_mass_from_vol(ct_m3[i][0], ct_m3[i][1]);
        }
        for (int j = 0; j < 10; ++j) wbt_m3[j] = (pr.wbt_fill_pct / 100.0) * WBT_GEOM[j].max_m3;
        step(0.0);
    }

    void save_custom_preset() {
        SimPreset cp = DEFAULT_PRESETS[0];
        cp.name = "Custom Snapshot"; cp.category = "CUSTOM";
        cp.grade_sys1 = th[0].grade_id; cp.grade_sys2 = th[2].grade_id;
        cp.ct_fill_pct = (ct_m3[0][0] / CT_GEOM[0].max_half_m3) * 100.0;
        cp.t_shell_c = th[0].t_top_shell; cp.t_vap_c = th[0].t_vapor; cp.t_liq_c = th[0].t_liquid;
        cp.press_kpa = th[0].press_kpa; cp.comp_mode = comp[0].mode;
        presets.push_back(cp);
    }

    std::string to_json() const {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2);
        o << "{\"spd\":" << (int)time_speed << ",\"time\":" << (int)sim_time_s
          << ",\"air\":" << amb_air_c << ",\"sw\":" << amb_sw_c
          << ",\"disp\":" << (int)hydro.disp_mt << ",\"tf\":" << hydro.tf << ",\"ta\":" << hydro.ta
          << ",\"tm\":" << hydro.tm << ",\"trim\":" << hydro.trim_m << ",\"heel\":" << hydro.heel_deg
          << ",\"tpc\":" << hydro.tpc << ",\"mctc\":" << (int)hydro.mctc
          << ",\"lcb\":" << hydro.lcb << ",\"lcf\":" << hydro.lcf << ",\"lcg\":" << hydro.lcg
          << ",\"kmt\":" << hydro.kmt << ",\"kg\":" << hydro.kg_solid << ",\"fsc\":" << hydro.fsc
          << ",\"gm\":" << hydro.gm_fluid << ",\"sf\":" << hydro.max_sf_pct << ",\"bm\":" << hydro.max_bm_pct
          << ",\"hsf\":" << hydro.harbor_sf_pct << ",\"hbm\":" << hydro.harbor_bm_pct
          << ",\"wtgt\":" << wbt_target << ",\"xo\":[" << vap_xover << "," << cond_xover << "," << liq_xover << "]"
          << ",\"esd\":" << fgd.esd1_active << ",\"cause\":\"" << fgd.last_cause << "\""
          << ",\"mdp\":" << (int)fgd.motor_rm_dp_pa << ",\"cvent\":" << fgd.comp_vent_on << ",\"floop\":" << fgd.fire_loop_ok
          << ",\"bal\":{\"pr\":[" << bal.pump_run[0] << "," << bal.pump_run[1] << "],\"op\":" << bal.op_mode
          << ",\"xo\":" << bal.ring_xover << ",\"q\":" << (int)bal.total_flow_m3_hr
          << ",\"pq\":[" << (int)bal.pump_flow[0] << "," << (int)bal.pump_flow[1] << "]"
          << ",\"pb\":[" << bal.pump_bar[0] << "," << bal.pump_bar[1] << "]"
          << ",\"pa\":[" << (int)bal.pump_amps[0] << "," << (int)bal.pump_amps[1] << "]}"
          << ",\"man\":{\"op\":[" << man.op_mode[0] << "," << man.op_mode[1] << "],\"sc\":[" << man.shore_conn[0] << "," << man.shore_conn[1] << "]"
          << ",\"ev\":[" << man.esd_vlv[0] << "," << man.esd_vlv[1] << "],\"gv\":[" << (int)man.gate_vlv_pct[0] << "," << (int)man.gate_vlv_pct[1] << "]"
          << ",\"lpt\":[" << man.liq_pt_bar[0] << "," << man.liq_pt_bar[1] << "],\"ltt\":[" << man.liq_tt_c[0] << "," << man.liq_tt_c[1] << "]"
          << ",\"vpt\":[" << man.vap_pt_bar[0] << "," << man.vap_pt_bar[1] << "],\"vtt\":[" << man.vap_tt_c[0] << "," << man.vap_tt_c[1] << "]"
          << ",\"cpt\":[" << man.cond_pt_bar[0] << "," << man.cond_pt_bar[1] << "],\"ctt\":[" << man.cond_tt_c[0] << "," << man.cond_tt_c[1] << "]"
          << ",\"trv\":[" << man.line_trv_lift[0] << "," << man.line_trv_lift[1] << "]"
          << ",\"marvs\":[" << man.marvs_lift[0] << "," << man.marvs_lift[1] << "," << man.marvs_lift[2] << "," << man.marvs_lift[3] << "]"
          << ",\"mast\":[" << (int)man.mast_vent_kg_hr[0] << "," << (int)man.mast_vent_kg_hr[1] << "," << (int)man.mast_vent_kg_hr[2] << "," << (int)man.mast_vent_kg_hr[3] << "]"
          << ",\"sb\":[" << man.shore_bar[0] << "," << man.shore_bar[1] << "],\"q\":[" << (int)man.flow_m3_hr[0] << "," << (int)man.flow_m3_hr[1] << "]"
          << ",\"htr\":" << man.heater_on << ",\"bst\":" << man.booster_on << ",\"et\":" << man.export_temp_c << ",\"eb\":" << man.export_bar << "},\"ct\":[";
        for (int i = 0; i < 4; ++i) {
            double p_pct = (ct_m3[i][0] / CT_GEOM[i].max_half_m3) * 100.0, s_pct = (ct_m3[i][1] / CT_GEOM[i].max_half_m3) * 100.0;
            o << (i ? "," : "") << "{\"p\":" << p_pct << ",\"s\":" << s_pct << ",\"mp\":" << th[i].mass_p_mt << ",\"ms\":" << th[i].mass_s_mt
              << ",\"bhd\":" << bhd_open[i] << ",\"g\":" << th[i].grade_id << ",\"sys\":" << th[i].sys_group
              << ",\"fl\":" << th[i].max_fill_limit() << ",\"pr\":" << th[i].press_kpa << ",\"hpr\":" << th[i].hold_press_kpa << ",\"ht\":" << th[i].hold_temp_c
              << ",\"rho\":" << (int)th[i].liq_density() << ",\"mt\":" << (th[i].mass_p_mt + th[i].mass_s_mt)
              << ",\"vsuc\":" << th[i].vap_suc_vlv << ",\"cin\":" << th[i].cond_in_vlv << ",\"lact\":" << th[i].loop_active
              << ",\"lq\":" << (int)th[i].load_flow_m3_hr << ",\"ts\":" << th[i].t_top_shell << ",\"bs\":" << th[i].t_bot_shell
              << ",\"tv\":" << th[i].t_vapor << ",\"tl\":" << th[i].t_liquid << ",\"cr\":" << th[i].cooling_rate_c_hr
              << ",\"hc\":" << th[i].hc_pct << ",\"ig\":" << th[i].ig_pct << ",\"o2\":" << th[i].o2_pct
              << ",\"prg\":" << th[i].purge_mode << ",\"fv\":" << (int)th[i].fill_vlv_pct
              << ",\"vr\":[" << th[i].v_ret[0][0] << "," << th[i].v_ret[0][1] << "," << th[i].v_ret[0][2] << "," << th[i].v_ret[1][0] << "," << th[i].v_ret[1][1] << "," << th[i].v_ret[1][2] << "]"
              << ",\"dp\":[" << dwp[i][0].running << "," << (int)dwp[i][0].throttle_pct << "," << (int)dwp[i][0].flow_m3_hr << "," << dwp[i][0].disch_bar << "," << (int)dwp[i][0].amps << "," << dwp[i][0].cavitating << "," << dwp[i][0].trip_code << "]"
              << ",\"ds\":[" << dwp[i][1].running << "," << (int)dwp[i][1].throttle_pct << "," << (int)dwp[i][1].flow_m3_hr << "," << dwp[i][1].disch_bar << "," << (int)dwp[i][1].amps << "," << dwp[i][1].cavitating << "," << dwp[i][1].trip_code << "]}";
        }
        o << "],\"comp\":[";
        for (int c = 0; c < 4; ++c) o << (c ? "," : "") << "[" << comp[c].mode << "," << comp[c].sys_id << "," << (int)comp[c].sw_eff_pct << "," << comp[c].cond_bar << "," << (int)comp[c].load_pct << "," << comp[c].suc_kpa << "]";
        o << "],\"fgd\":[";
        for (int z = 0; z < 6; ++z) o << (z ? "," : "") << "[" << fgd.zones[z].hc_lel_pct << "," << fgd.zones[z].o2_pct << "," << (int)fgd.zones[z].nh3_ppm << "," << fgd.zones[z].leak_sim << "," << fgd.zones[z].alarm_state << "]";
        o << "],\"wbt\":[";
        for (int j = 0; j < 10; ++j) {
            double f = (wbt_m3[j] / WBT_GEOM[j].max_m3) * 100.0;
            o << (j ? "," : "") << "[" << f << "," << (int)wbt_properties(j, f)[3] << "," << (int)bal.wbt_vlv_pct[j] << "," << (int)bal.tank_flow_m3_hr[j] << "]";
        }
        o << "],\"presets\":[";
        for (size_t p = 0; p < presets.size(); ++p) o << (p ? "," : "") << "{\"n\":\"" << presets[p].name << "\",\"c\":\"" << presets[p].category << "\"}";
        o << "],\"sf_c\":[";
        for (int s = 0; s < 20; ++s) o << (s ? "," : "") << hydro.sf_stations[s];
        o << "],\"bm_c\":[";
        for (int s = 0; s < 20; ++s) o << (s ? "," : "") << hydro.bm_stations[s];
        o << "]}";
        return o.str();
    }
};
