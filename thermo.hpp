#pragma once
#include <array>
#include <cmath>
#include <algorithm>

struct CargoGrade {
    const char* name;
    double mw, tb_c, ant_A, ant_B, ant_C;
    double rho_15, alpha, latent_kj, cp_liq, cp_vap, gamma_k, t_ref_marvs;
};

// 0: Commercial Propane (C3H8), 1: n-Butane (C4H10), 2: Anhydrous Ammonia (NH3)
constexpr std::array<CargoGrade, 3> GRADES = {{
    {"PROPANE C3",  44.097, -42.10, 3.92828, 803.997, -26.11, 507.5, 0.00162,  425.6, 2.24, 1.55, 1.13, -36.8},
    {"BUTANE C4",   58.122,  -0.50, 3.93266, 935.773, -34.36, 584.2, 0.00108,  385.8, 2.32, 1.67, 1.10,   4.8},
    {"AMMONIA NH3", 17.031, -33.34, 4.48540, 926.132, -32.98, 618.2, 0.00148, 1371.2, 4.45, 2.15, 1.31, -28.2}
}};

struct TankThermo {
    int grade_id = 0; // 0=Propane, 1=Butane, 2=Ammonia
    double t_top_shell = -38.5;
    double t_bot_shell = -41.2;
    double t_vapor     = -39.5;
    double t_liquid    = -41.5;
    double cool_rate_c_hr = 0.0;

    // Conserved Cargo Mass (Port MT, Stbd MT, Vapor kg)
    double mass_p_mt   = 3055.0;
    double mass_s_mt   = 3055.0;
    double vap_mass_kg = 19500.0;
    double prev_liq_m3 = 10500.0;
    bool mass_initialized = false;

    // Pressures in kPa gauge (MARVS = 25.0 kPa g, Hold Space Inerted = 2.2 kPa g)
    double press_kpa      = 9.5;
    double hold_press_kpa = 2.2;

    double hc_pct = 98.5, ig_pct = 1.4, o2_pct = 0.1;
    int purge_mode = 0;

    // Lineup & Throttling Valves
    bool vap_suc_vlv    = true;
    bool cond_in_vlv    = true;
    double fill_vlv_pct = 0.0;   // 0-100% Throttling Liquid Filling Valve for Stagger Loading!
    double load_flow_m3_hr = 0.0;// Actual loading flow entering this tank (m3/hr)

    // 6 Condensate Return Branch Valves: [0=Port, 1=Stbd][0=TopSpray, 1=BotSpray, 2=BotSparge]
    std::array<std::array<bool, 3>, 2> v_ret = {{{false, false, false}, {false, false, false}}};
    int sys_group = 0;
    bool lineup_active = false;

    const CargoGrade& grade() const { return GRADES[std::clamp(grade_id, 0, 2)]; }

    double liq_density(double t_c) const {
        const auto& g = grade();
        return g.rho_15 * (1.0 - g.alpha * (t_c - 15.0));
    }

    // Saturation pressure in kPa gauge
    double sat_press_kpa(double t_c) const {
        const auto& g = grade();
        double tk = t_c + 273.15;
        double p_abs_bar = std::pow(10.0, g.ant_A - (g.ant_B / (tk + g.ant_C)));
        return std::max(0.0, (p_abs_bar - 1.01325) * 100.0);
    }

    double vap_density() const {
        const auto& g = grade();
        double p_abs_kpa = press_kpa + 101.325;
        return (p_abs_kpa * g.mw) / (8.31446 * (t_vapor + 273.15) * 0.94);
    }

    double max_fill_limit() const {
        const auto& g = grade();
        return std::min(98.0, 98.0 * (liq_density(g.t_ref_marvs) / liq_density(t_liquid)));
    }

    void sync_mass_from_vol(double vp_m3, double vs_m3) {
        double rho_t = liq_density(t_liquid) / 1000.0;
        mass_p_mt = vp_m3 * rho_t;
        mass_s_mt = vs_m3 * rho_t;
        prev_liq_m3 = vp_m3 + vs_m3;
        vap_mass_kg = std::max(400.0, (21000.0 - prev_liq_m3) * vap_density());
        mass_initialized = true;
    }

    double total_cargo_mt() const {
        return mass_p_mt + mass_s_mt + (vap_mass_kg / 1000.0);
    }

    void step_thermo(
        double dt_sec, std::array<double, 2>& half_vol_m3, double max_half_m3,
        double amb_air_c, double amb_sw_c,
        double cond_flow_kg_s, bool is_hot_gas, double comp_suc_kg_s
    ) {
        if (!mass_initialized) sync_mass_from_vol(half_vol_m3[0], half_vol_m3[1]);
        if (dt_sec <= 0.0) return;

        const auto& g = grade();
        static constexpr double CP_TOP_STEEL_KJ = 420000.0 * 0.47; // 197,400 kJ/K (420t upper steel)
        static constexpr double CP_BOT_STEEL_KJ = 530000.0 * 0.47; // 249,100 kJ/K (530t lower steel)

        double tot_liq_mt = mass_p_mt + mass_s_mt;
        double fill_pct = ((half_vol_m3[0] + half_vol_m3[1]) / (2.0 * max_half_m3)) * 100.0;
        double dry_shell_frac = std::clamp(1.0 - (fill_pct / 98.0), 0.05, 1.0);
        double cp_liq_total_kj = std::max(2500.0, tot_liq_mt * 1000.0) * g.cp_liq;
        double cp_vap_total_kj = std::max(500.0, vap_mass_kg) * g.cp_vap;

        // 1. Ambient Heat Ingress governed by Ambient Air (Deck) & Seawater (Hold Space)
        double t_hold = 0.70 * amb_sw_c + 0.30 * amb_air_c;
        hold_press_kpa = 2.0 + (t_hold - 15.0) * 0.025; // Hold space N2 thermal pressure (kPa g)

        double q_amb_top_kw = 0.52 * (amb_air_c - t_top_shell);
        double q_amb_bot_kw = 0.60 * (t_hold - t_bot_shell);
        double d_top = (q_amb_top_kw * dt_sec) / CP_TOP_STEEL_KJ;
        double d_bot = (q_amb_bot_kw * dt_sec) / CP_BOT_STEEL_KJ;

        // Convective heat transfer between steel, vapor, and bulk liquid
        double q_top_vap_kw = 3.5 * dry_shell_frac * (t_top_shell - t_vapor);
        d_top   -= (q_top_vap_kw * dt_sec) / CP_TOP_STEEL_KJ;
        t_vapor += (q_top_vap_kw * dt_sec) / cp_vap_total_kj;

        double q_bot_liq_kw = (tot_liq_mt > 15.0 ? 5.2 : 0.8) * (t_bot_shell - t_liquid);
        d_bot    -= (q_bot_liq_kw * dt_sec) / CP_BOT_STEEL_KJ;
        t_liquid += (q_bot_liq_kw * dt_sec) / cp_liq_total_kj;

        // Natural Boil-Off Gas (BOG) mass transfer from Liquid -> Vapor (Conserves Total Mass)
        if (tot_liq_mt > 5.0 && t_liquid > g.tb_c - 0.5 && q_bot_liq_kw > 0.0) {
            double bog_kg = std::min(tot_liq_mt * 500.0, (q_bot_liq_kw * 0.25 * dt_sec) / g.latent_kj);
            double p_ratio = (tot_liq_mt > 0.01) ? (mass_p_mt / tot_liq_mt) : 0.5;
            mass_p_mt   -= (bog_kg / 1000.0) * p_ratio;
            mass_s_mt   -= (bog_kg / 1000.0) * (1.0 - p_ratio);
            vap_mass_kg += bog_kg;
        }

        // 2. Spray & Sparge Condensate Return with Wetted Area & Submergence Physics
        int n_top = (v_ret[0][0] ? 1 : 0) + (v_ret[1][0] ? 1 : 0);
        int n_bot = (v_ret[0][1] ? 1 : 0) + (v_ret[1][1] ? 1 : 0);
        int n_spg = (v_ret[0][2] ? 1 : 0) + (v_ret[1][2] ? 1 : 0);
        int n_open = n_top + n_bot + n_spg;

        lineup_active = (cond_in_vlv && n_open > 0 && cond_flow_kg_s > 0.01);
        double suc_removed_kg = (vap_suc_vlv ? comp_suc_kg_s : 0.0) * dt_sec;
        vap_mass_kg = std::max(150.0, vap_mass_kg - suc_removed_kg);

        if (lineup_active) {
            double m_per_vlv = cond_flow_kg_s / n_open;
            double t_cond_out = amb_sw_c + 5.5;
            double x_flash = is_hot_gas ? 1.0 :
                std::clamp((g.cp_liq * std::max(0.0, t_cond_out - g.tb_c)) / (g.latent_kj * 2.2), 0.05, 0.48);
            double liq_frac = 1.0 - x_flash;

            // A) TOP SPRAY: Scales with exposed dry upper shell area
            if (n_top > 0) {
                double m_top = n_top * m_per_vlv;
                if (!is_hot_gas) {
                    double dT_drive = std::clamp((t_top_shell - g.tb_c) / 35.0, 0.05, 1.0);
                    double q_cool_kw = m_top * liq_frac * g.latent_kj * (0.35 + 0.45 * dT_drive) * dry_shell_frac;
                    d_top   -= (q_cool_kw * 0.68 * dt_sec) / CP_TOP_STEEL_KJ;
                    t_vapor -= (q_cool_kw * 0.32 * dt_sec) / cp_vap_total_kj;
                    double evap_f = (t_top_shell > g.tb_c + 3.0) ? 0.85 : 0.12;
                    double to_vap_kg = m_top * dt_sec * (x_flash + liq_frac * evap_f);
                    double to_liq_kg = m_top * dt_sec - to_vap_kg;
                    vap_mass_kg += to_vap_kg;
                    mass_p_mt += (to_liq_kg / 2000.0); mass_s_mt += (to_liq_kg / 2000.0);
                } else {
                    double q_hot_kw = m_top * g.cp_vap * std::max(0.0, 68.0 - t_top_shell);
                    d_top   += (q_hot_kw * 0.65 * dt_sec) / CP_TOP_STEEL_KJ;
                    t_vapor += (q_hot_kw * 0.35 * dt_sec) / cp_vap_total_kj;
                    vap_mass_kg += m_top * dt_sec;
                }
            }
            // B) BOTTOM SPRAY: Mist cools lower shell when fill < 35%; submerged liquid injection when > 35%!
            if (n_bot > 0) {
                double m_bot = n_bot * m_per_vlv;
                bool submerged = (fill_pct > 35.0);
                if (!is_hot_gas) {
                    if (!submerged) {
                        double dT_drive = std::clamp((t_bot_shell - g.tb_c) / 35.0, 0.05, 1.0);
                        double q_cool_kw = m_bot * liq_frac * g.latent_kj * (0.38 + 0.45 * dT_drive);
                        d_bot   -= (q_cool_kw * 0.75 * dt_sec) / CP_BOT_STEEL_KJ;
                        t_vapor -= (q_cool_kw * 0.25 * dt_sec) / cp_vap_total_kj;
                        double evap_f = (t_bot_shell > g.tb_c + 2.5) ? 0.80 : 0.12;
                        double to_vap_kg = m_bot * dt_sec * (x_flash + liq_frac * evap_f);
                        double to_liq_kg = m_bot * dt_sec - to_vap_kg;
                        vap_mass_kg += to_vap_kg;
                        mass_p_mt += (to_liq_kg / 2000.0); mass_s_mt += (to_liq_kg / 2000.0);
                    } else {
                        // Submerged bottom spray mixes directly into bulk liquid
                        mass_p_mt += (m_bot * dt_sec) / 2000.0; mass_s_mt += (m_bot * dt_sec) / 2000.0;
                    }
                } else {
                    double q_hot_kw = m_bot * g.cp_vap * std::max(0.0, 68.0 - t_bot_shell);
                    d_bot += (q_hot_kw * 0.80 * dt_sec) / CP_BOT_STEEL_KJ;
                    vap_mass_kg += m_bot * dt_sec;
                }
            }
            // C) BOTTOM SPARGE: Bubble condensation proportional to liquid submergence head
            if (n_spg > 0) {
                double m_spg = n_spg * m_per_vlv;
                if (!is_hot_gas) {
                    double q_sub_kw = m_spg * liq_frac * g.cp_liq * std::max(0.0, t_liquid - (g.tb_c - 1.5));
                    t_liquid -= (q_sub_kw * 0.85 * dt_sec) / cp_liq_total_kj;
                    d_bot    -= (q_sub_kw * 0.15 * dt_sec) / CP_BOT_STEEL_KJ;
                    mass_p_mt += (m_spg * dt_sec) / 2000.0; mass_s_mt += (m_spg * dt_sec) / 2000.0;
                } else {
                    double q_pud_kw = m_spg * (g.latent_kj * 0.45 + g.cp_vap * std::max(0.0, 68.0 - t_bot_shell));
                    d_bot    += (q_pud_kw * 0.65 * dt_sec) / CP_BOT_STEEL_KJ;
                    t_liquid += (q_pud_kw * 0.35 * dt_sec) / cp_liq_total_kj;
                    vap_mass_kg += m_spg * dt_sec;
                }
            }
        }

        t_top_shell = std::clamp(t_top_shell + d_top, g.tb_c - 2.0, 55.0);
        t_bot_shell = std::clamp(t_bot_shell + d_bot, g.tb_c - 2.0, 55.0);
        t_vapor     = std::clamp(t_vapor, g.tb_c - 2.0, 65.0);
        t_liquid    = std::clamp(t_liquid, g.tb_c - 2.0, 45.0);
        cool_rate_c_hr = (d_top / dt_sec) * 3600.0;

        // 3. Recompute True Liquid Volume & Adiabatic Piston Compression of Vapor Space
        double rho_t = liq_density(t_liquid) / 1000.0;
        half_vol_m3[0] = std::clamp(mass_p_mt / rho_t, 0.0, max_half_m3 * 0.995);
        half_vol_m3[1] = std::clamp(mass_s_mt / rho_t, 0.0, max_half_m3 * 0.995);
        double cur_liq_m3 = half_vol_m3[0] + half_vol_m3[1];
        double d_liq_m3   = cur_liq_m3 - prev_liq_m3;
        prev_liq_m3       = cur_liq_m3;

        double vap_vol_m3 = std::max(400.0, 21000.0 - cur_liq_m3);
        // Adiabatic piston compression when liquid volume rises (loading) or falls (discharging)
        if (std::abs(d_liq_m3) > 0.01) {
            double p_abs_kpa = press_kpa + 101.325;
            double dp_piston_kpa = p_abs_kpa * g.gamma_k * (d_liq_m3 / vap_vol_m3);
            press_kpa += dp_piston_kpa;
            t_vapor   += (g.gamma_k - 1.0) * (t_vapor + 273.15) * (d_liq_m3 / vap_vol_m3);
        }

        double p_ideal_kpa = (vap_mass_kg * 8.31446 * (t_vapor + 273.15) * 0.94) / (vap_vol_m3 * g.mw);
        double p_mass_kpa  = std::max(0.0, p_ideal_kpa - 101.325);
        double p_eq_kpa    = sat_press_kpa(t_liquid * 0.82 + t_vapor * 0.18) + ig_pct * 0.16;
        double target_kpa  = (tot_liq_mt > 10.0) ? (p_eq_kpa * 0.72 + p_mass_kpa * 0.28) : p_mass_kpa;
        double relax       = std::min(0.22, dt_sec * 0.008);
        press_kpa          = std::clamp(press_kpa * (1.0 - relax) + target_kpa * relax, 0.5, 26.5);

        // 4. Purging Operations
        double dt_hr = dt_sec / 3600.0;
        if (purge_mode == 1) {
            hc_pct = std::max(0.5, hc_pct - 18.0 * dt_hr);
            o2_pct = std::max(1.2, o2_pct - 12.0 * dt_hr);
            ig_pct = 100.0 - hc_pct - o2_pct;
        } else if (purge_mode == 2) {
            o2_pct = std::min(20.9, o2_pct + 15.0 * dt_hr);
            hc_pct = std::max(0.0,  hc_pct - 15.0 * dt_hr);
            ig_pct = 100.0 - hc_pct - o2_pct;
        } else if (purge_mode == 3) {
            hc_pct = std::min(99.2, hc_pct + 22.0 * dt_hr);
            o2_pct = std::max(0.1,  o2_pct - 10.0 * dt_hr);
            ig_pct = std::max(0.7,  100.0 - hc_pct - o2_pct);
        }
    }
};
