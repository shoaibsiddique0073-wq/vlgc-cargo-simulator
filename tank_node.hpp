#pragma once
#include "fluid_props.hpp"

struct TankThermo {
    int grade_id = 0, sys_group = 0, purge_mode = 0;
    double t_top_shell = -38.5, t_bot_shell = -41.2, t_vapor = -39.5, t_liquid = -41.5;
    double press_kpa = 9.50;
    double hold_press_kpa = 2.20, hold_temp_c = 12.0;
    double hc_pct = 98.5, ig_pct = 1.4, o2_pct = 0.1;
    double cooling_rate_c_hr = 0.0;
    double mass_p_mt = 3055.5, mass_s_mt = 3055.5;
    double fill_vlv_pct = 0.0, load_flow_m3_hr = 0.0;
    bool vap_suc_vlv = true, cond_in_vlv = true, loop_active = false;
    bool v_ret[2][3] = {{false, false, false}, {false, false, false}};

    double sat_press_kpa(double t_c) const { return FluidThermo::sat_press_kpa(grade_id, t_c); }
    double sat_temp_c(double p_kpa) const { return FluidThermo::sat_temp_c(grade_id, p_kpa); }
    double liq_density() const { return FluidThermo::liq_density(grade_id, t_liquid); }

    double max_fill_limit() const {
        double rho_ref = FluidThermo::liq_density(grade_id, GRADES[grade_id].t_ref_igc);
        return std::clamp(98.0 * (rho_ref / liq_density()), 85.0, 98.0);
    }

    void sync_mass_from_vol(double vp_m3, double vs_m3) {
        double rho_t = liq_density() / 1000.0;
        mass_p_mt = vp_m3 * rho_t; mass_s_mt = vs_m3 * rho_t;
    }

    void sync_vol_from_mass(double max_half_m3, double& vp_m3, double& vs_m3) {
        double rho_t = liq_density() / 1000.0;
        double max_m3 = (max_fill_limit() / 100.0) * max_half_m3;
        vp_m3 = std::clamp(mass_p_mt / rho_t, 0.0, max_m3);
        vs_m3 = std::clamp(mass_s_mt / rho_t, 0.0, max_m3);
        mass_p_mt = vp_m3 * rho_t; mass_s_mt = vs_m3 * rho_t;
    }
    void step(double dt, double fill_pct, double amb_air_c, double amb_sw_c,
              double comp_draw_kg_s, bool hot_gas_mode, double net_liq_m3_s, double& mast_vent_kg_hr) {
        double dt_hr = dt / 3600.0;
        const auto& pr = GRADES[std::clamp(grade_id, 0, 2)];

        // 1. Hold Space Thermal & Ideal-Gas Pressure Coupling
        double hold_target_t = 0.55 * amb_sw_c + 0.25 * amb_air_c + 0.20 * t_bot_shell;
        hold_temp_c += (hold_target_t - hold_temp_c) * std::min(1.0, dt_hr * 0.45);
        hold_press_kpa = std::clamp(2.0 + (hold_temp_c - 15.0) * 0.06, 0.5, 6.5);

        // 2. Ambient Hull Heat Ingress into Tank Shell, Vapor & Liquid
        t_top_shell += (amb_air_c - t_top_shell) * 0.0075 * dt_hr;
        t_bot_shell += (amb_sw_c - t_bot_shell) * 0.0055 * dt_hr;
        t_vapor     += ((0.6 * t_top_shell + 0.4 * t_liquid) - t_vapor) * 0.18 * dt_hr;
        if (fill_pct > 0.3) {
            t_liquid += (t_bot_shell - t_liquid) * (0.0045 / std::max(0.15, fill_pct / 50.0)) * dt_hr;
        }

        // 3. Spray / Sparging Return Loop Bounded Strictly by Saturation Temperature Floor T_sat(P_dome)!
        int n_top = (v_ret[0][0] ? 1 : 0) + (v_ret[1][0] ? 1 : 0);
        int n_bot = (v_ret[0][1] ? 1 : 0) + (v_ret[1][1] ? 1 : 0);
        int n_spg = (v_ret[0][2] ? 1 : 0) + (v_ret[1][2] ? 1 : 0);
        int n_tot = n_top + n_bot + n_spg;
        loop_active = (vap_suc_vlv && cond_in_vlv && n_tot > 0 && comp_draw_kg_s > 0.01);

        double prev_top = t_top_shell;
        double m_spray_cond_kg_s = 0.0;

        if (loop_active && dt > 0.0) {
            // Joule-Thomson expansion across spray nozzles cannot drop below T_sat(press_kpa)!
            double t_sat_floor = sat_temp_c(press_kpa);
            double t_supply = hot_gas_mode ? 68.0 : t_sat_floor;
            double cap_factor = std::min(2.5, comp_draw_kg_s / 1.2);

            if (n_top > 0) {
                double d_top = (t_supply - t_top_shell) * 0.22 * n_top * cap_factor * dt_hr;
                t_top_shell = hot_gas_mode ? std::min(65.0, t_top_shell + d_top) : std::max(t_sat_floor, t_top_shell + d_top);
                double d_vap = (t_supply - t_vapor) * 0.35 * n_top * cap_factor * dt_hr;
                t_vapor = hot_gas_mode ? std::min(65.0, t_vapor + d_vap) : std::max(t_sat_floor, t_vapor + d_vap);
                if (!hot_gas_mode && t_vapor > t_sat_floor) m_spray_cond_kg_s += 0.35 * comp_draw_kg_s;
            }
            if (n_bot > 0) {
                double d_bot = (t_supply - t_bot_shell) * 0.24 * n_bot * cap_factor * dt_hr;
                t_bot_shell = hot_gas_mode ? std::min(65.0, t_bot_shell + d_bot) : std::max(t_sat_floor, t_bot_shell + d_bot);
                if (!hot_gas_mode && fill_pct > 1.0) {
                    t_liquid = std::max(t_sat_floor, t_liquid + (t_sat_floor - t_liquid) * 0.15 * cap_factor * dt_hr);
                }
            }
            if (n_spg > 0) {
                double t_spg = hot_gas_mode ? 72.0 : t_sat_floor + 1.5;
                t_bot_shell = std::clamp(t_bot_shell + (t_spg - t_bot_shell) * 0.30 * n_spg * cap_factor * dt_hr, t_sat_floor, 68.0);
                t_liquid    = std::clamp(t_liquid + (t_spg - t_liquid) * 0.28 * n_spg * cap_factor * dt_hr, t_sat_floor, 65.0);
            }
        }
        cooling_rate_c_hr = (dt_hr > 1e-7) ? ((t_top_shell - prev_top) / dt_hr) : 0.0;

        // 4. Dynamic Real-Gas Ullage Vapor Mass & Pressure Balance (dP/dt)
        double v_ullage_m3 = std::max(420.0, 21000.0 * (1.0 - fill_pct / 100.0));
        double p_eq_kpa = sat_press_kpa(t_liquid);

        // Interfacial flash evaporation (+) or condensation (-) driven by (P_sat - P_dome)
        double m_boil_kg_s = (fill_pct > 0.2) ? ((p_eq_kpa - press_kpa) * 0.28 + 0.08) : ((t_vapor - prev_top) * 0.02);
        double m_net_vap_kg_s = m_boil_kg_s - (vap_suc_vlv ? comp_draw_kg_s : 0.0) - m_spray_cond_kg_s;

        double r_spec_kpa = 8.31446 / pr.mw;
        double dp_mass_kpa = (m_net_vap_kg_s * r_spec_kpa * (t_vapor + 273.15) / v_ullage_m3) * dt;
        double dp_piston_kpa = ((press_kpa + 101.325) * (net_liq_m3_s / v_ullage_m3)) * dt;

        press_kpa = std::clamp(press_kpa + dp_mass_kpa + dp_piston_kpa, 0.5, 32.0);

        // Coupled evaporative cooling of bulk liquid when compressor lowers dome pressure
        if (fill_pct > 1.0 && m_boil_kg_s > 0.05 && dt > 0.0) {
            double m_liq_kg = std::max(10000.0, (mass_p_mt + mass_s_mt) * 1000.0);
            double dt_evap = -(m_boil_kg_s * dt * pr.h_fg_kj_kg) / (m_liq_kg * pr.cp_liq);
            t_liquid = std::max(sat_temp_c(1.5), t_liquid + dt_evap);
        }

        // 5. MARVS Relief Valve (25.0 kPaG) -> Mast Riser
        if (press_kpa >= 25.0) {
            double v_rel_kg_s = (press_kpa - 24.8) * 1.65;
            mast_vent_kg_hr += v_rel_kg_s * 3600.0;
            press_kpa -= (v_rel_kg_s * r_spec_kpa * (t_vapor + 273.15) / v_ullage_m3) * dt;
        }

        // 6. Purging / Gas-Freeing Modes
        if (purge_mode == 1) { hc_pct = std::max(0.0, hc_pct - 4.0 * dt_hr); ig_pct = 100.0 - hc_pct - o2_pct; }
        else if (purge_mode == 2) { o2_pct = std::min(20.9, o2_pct + 4.0 * dt_hr); ig_pct = 100.0 - hc_pct - o2_pct; }
        else if (purge_mode == 3) { hc_pct = std::min(99.0, hc_pct + 5.0 * dt_hr); ig_pct = 100.0 - hc_pct - o2_pct; }
    }
};
