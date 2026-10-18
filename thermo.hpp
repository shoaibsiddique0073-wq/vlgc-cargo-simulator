#pragma once
#include <array>
#include <cmath>
#include <algorithm>

struct CargoGrade {
    const char* name;
    double mw;         // Molecular weight (g/mol)
    double tb_c;       // Boiling point at 1 atm (C)
    double ant_A, ant_B, ant_C; // Antoine constants (bar abs, K)
    double rho_15;     // Liquid density at 15C (kg/m3)
    double alpha;      // Thermal expansion coefficient (1/C)
    double latent_kj;  // Latent heat of vaporization dH_vap (kJ/kg)
    double cp_liq;     // Specific heat of liquid (kJ/kg.K)
    double cp_vap;     // Specific heat of vapor (kJ/kg.K)
    double t_ref_marvs;// Saturation temp at MARVS 250 mbarg (C)
};

// 0: Commercial Propane (C3H8), 1: n-Butane (C4H10), 2: Anhydrous Ammonia (NH3)
constexpr std::array<CargoGrade, 3> GRADES = {{
    {"PROPANE C3",  44.097, -42.10, 3.92828, 803.997, -26.11, 507.5, 0.00162,  425.6, 2.24, 1.55, -36.8},
    {"BUTANE C4",   58.122,  -0.50, 3.93266, 935.773, -34.36, 584.2, 0.00108,  385.8, 2.32, 1.67,   4.8},
    {"AMMONIA NH3", 17.031, -33.34, 4.48540, 926.132, -32.98, 618.2, 0.00148, 1371.2, 4.45, 2.15, -28.2}
}};

struct TankThermo {
    int grade_id = 0; // 0=Propane, 1=Butane, 2=Ammonia
    // 4-Zone Thermal Model (C)
    double t_top_shell = -38.5;
    double t_bot_shell = -41.2;
    double t_vapor     = -39.5;
    double t_liquid    = -41.5;
    double cool_rate_c_hr = 0.0;

    double press_mbarg = 95.0; // Dome gauge pressure (MARVS = 250 mbarg)
    double hc_pct = 98.5, ig_pct = 1.4, o2_pct = 0.1;
    int purge_mode = 0; // 0=Isolate, 1=Inert(IGG/N2), 2=Aerate(Air), 3=Gas-Up(HC/NH3)

    // Lineup Valves per Tank
    bool vap_suc_vlv = true;  // Dome Vapour Suction Valve to Vapour Header
    bool cond_in_vlv = true;  // Dome Condensate Return Main Inlet Valve
    // 6 Branch Valves: [0=Port, 1=Stbd][0=TopSpray, 1=BotSpray, 2=BotSparge]
    std::array<std::array<bool, 3>, 2> v_ret = {{{false, false, false}, {false, false, false}}};
    int sys_group = 0;        // 0 = System 1 Header, 1 = System 2 Header
    bool lineup_active = false; // True ONLY when full suction->comp->cond->branch lineup is complete

    const CargoGrade& grade() const { return GRADES[std::clamp(grade_id, 0, 2)]; }

    double liq_density(double t_c) const {
        const auto& g = grade();
        return g.rho_15 * (1.0 - g.alpha * (t_c - 15.0));
    }

    double sat_press_mbarg(double t_c) const {
        const auto& g = grade();
        double tk = t_c + 273.15;
        double p_abs = std::pow(10.0, g.ant_A - (g.ant_B / (tk + g.ant_C)));
        return std::max(0.0, (p_abs - 1.01325) * 1000.0);
    }

    double vap_density() const {
        const auto& g = grade();
        double p_kpa = ((press_mbarg / 1000.0) + 1.01325) * 100.0;
        return (p_kpa * g.mw) / (8.31446 * (t_vapor + 273.15) * 0.95);
    }

    double max_fill_limit() const {
        const auto& g = grade();
        return std::min(98.0, 98.0 * (liq_density(g.t_ref_marvs) / liq_density(t_liquid)));
    }

    // 950 Metric Tons of Low-Temp Steel per Type A Tank:
    // Upper Shell/Dome = 420,000 kg (Cp = 197,400 kJ/K), Lower Shell/Hopper = 530,000 kg (Cp = 249,100 kJ/K)
    void step_thermo(double dt_sec, double vol_m3, double cond_flow_kg_s, bool is_hot_gas, double comp_suc_kg_s) {
        if (dt_sec <= 0.0) return;
        const auto& g = grade();
        static constexpr double CP_TOP_STEEL_KJ = 420000.0 * 0.47; // 197,400 kJ/K
        static constexpr double CP_BOT_STEEL_KJ = 530000.0 * 0.47; // 249,100 kJ/K
        double vap_mass_kg = std::max(500.0, (21000.0 - vol_m3) * vap_density());
        double cp_vap_total_kj = vap_mass_kg * g.cp_vap;
        double liq_mass_kg = std::max(2000.0, vol_m3 * liq_density(t_liquid));
        double cp_liq_total_kj = liq_mass_kg * g.cp_liq;

        // 1. Ambient Insulation Heat Ingress (~32 kW top, ~28 kW bottom across 120mm PUF)
        double q_amb_top_kw = 1.45 * (25.0 - t_top_shell);
        double q_amb_bot_kw = 1.25 * (22.0 - t_bot_shell);
        double d_top = (q_amb_top_kw * dt_sec) / CP_TOP_STEEL_KJ;
        double d_bot = (q_amb_bot_kw * dt_sec) / CP_BOT_STEEL_KJ;

        // Convective coupling between shell, vapor, and bulk liquid
        double q_top_vap_kw = 4.2 * (t_top_shell - t_vapor);
        d_top   -= (q_top_vap_kw * dt_sec) / CP_TOP_STEEL_KJ;
        t_vapor += (q_top_vap_kw * dt_sec) / cp_vap_total_kj;
        t_liquid += (3.5 * (t_bot_shell - t_liquid) * dt_sec) / cp_liq_total_kj;

        // 2. Check Branch Valves & Strict Condensate Lineup Gate
        int n_top = (v_ret[0][0] ? 1 : 0) + (v_ret[1][0] ? 1 : 0);
        int n_bot = (v_ret[0][1] ? 1 : 0) + (v_ret[1][1] ? 1 : 0);
        int n_spg = (v_ret[0][2] ? 1 : 0) + (v_ret[1][2] ? 1 : 0);
        int n_open = n_top + n_bot + n_spg;

        lineup_active = (cond_in_vlv && n_open > 0 && cond_flow_kg_s > 0.01);
        double flash_kg = 0.0, condense_kg = 0.0;

        if (lineup_active) {
            double m_per_vlv = cond_flow_kg_s / n_open; // kg/s per open branch valve
            double t_supply = is_hot_gas ? 68.0 : (g.tb_c - 1.0);

            // A) TOP SPRAY: Atomizes into upper vapor space & upper shell
            if (n_top > 0) {
                double m_top = n_top * m_per_vlv;
                if (!is_hot_gas) {
                    // Latent heat of vaporization absorbed from upper steel & vapor space
                    double q_cool_kw = m_top * (g.latent_kj + g.cp_vap * std::max(0.0, t_top_shell - g.tb_c));
                    d_top   -= (q_cool_kw * 0.72 * dt_sec) / CP_TOP_STEEL_KJ;
                    t_vapor -= (q_cool_kw * 0.28 * dt_sec) / cp_vap_total_kj;
                    if (t_top_shell > g.tb_c + 2.5) flash_kg += m_top * dt_sec * 0.90; // Flashes on warm steel!
                    else condense_kg += m_top * dt_sec * 0.55; // Condenses vapor once cold
                } else {
                    double q_hot_kw = m_top * g.cp_vap * (t_supply - t_top_shell);
                    d_top   += (q_hot_kw * 0.65 * dt_sec) / CP_TOP_STEEL_KJ;
                    t_vapor += (q_hot_kw * 0.35 * dt_sec) / cp_vap_total_kj;
                    flash_kg += m_top * dt_sec * 0.40;
                }
            }
            // B) BOTTOM SPRAY: Cools lower/middle shell plating & vapor space
            if (n_bot > 0) {
                double m_bot = n_bot * m_per_vlv;
                if (!is_hot_gas) {
                    double q_cool_kw = m_bot * (g.latent_kj + g.cp_vap * std::max(0.0, t_bot_shell - g.tb_c));
                    d_bot   -= (q_cool_kw * 0.78 * dt_sec) / CP_BOT_STEEL_KJ;
                    t_vapor -= (q_cool_kw * 0.22 * dt_sec) / cp_vap_total_kj;
                    if (t_bot_shell > g.tb_c + 2.0) flash_kg += m_bot * dt_sec * 0.92;
                } else {
                    double q_hot_kw = m_bot * g.cp_vap * (t_supply - t_bot_shell);
                    d_bot += (q_hot_kw * 0.80 * dt_sec) / CP_BOT_STEEL_KJ;
                    flash_kg += m_bot * dt_sec * 0.40;
                }
            }
            // C) BOTTOM SPARGE: Injected under liquid heel -> heats/cools bulk liquid & bottom sump; ~0 upper shell cooling
            if (n_spg > 0) {
                double m_spg = n_spg * m_per_vlv;
                if (!is_hot_gas) {
                    double q_sub_kw = m_spg * g.cp_liq * (t_liquid - t_supply);
                    t_liquid -= (q_sub_kw * 0.85 * dt_sec) / cp_liq_total_kj;
                    d_bot    -= (q_sub_kw * 0.15 * dt_sec) / CP_BOT_STEEL_KJ;
                } else {
                    // Hot Gas Puddle Heating / Stripping at tank bottom
                    double q_puddle_kw = m_spg * (g.latent_kj * 0.6 + g.cp_vap * (t_supply - t_bot_shell));
                    d_bot    += (q_puddle_kw * 0.65 * dt_sec) / CP_BOT_STEEL_KJ;
                    t_liquid += (q_puddle_kw * 0.35 * dt_sec) / cp_liq_total_kj;
                    flash_kg += m_spg * dt_sec * 0.85;
                }
            }
        }

        t_top_shell += d_top;
        t_bot_shell += d_bot;
        cool_rate_c_hr = (d_top / dt_sec) * 3600.0;

        // 3. Dome Pressure Mass & Vapour-Liquid Equilibrium Balance
        double suc_removed_kg = (vap_suc_vlv ? comp_suc_kg_s : 0.0) * dt_sec;
        double net_vap_delta_kg = flash_kg - condense_kg - suc_removed_kg;
        double dp_mass_mbar = (net_vap_delta_kg / vap_mass_kg) * 1013.25;

        double p_eq = sat_press_mbarg(t_liquid * 0.82 + t_vapor * 0.18) + ig_pct * 1.6;
        double relax = std::min(0.25, dt_sec * 0.008);
        press_mbarg = press_mbarg * (1.0 - relax) + p_eq * relax + dp_mass_mbar;
        press_mbarg = std::clamp(press_mbarg, 5.0, 265.0);

        // 4. Purging / Gas Atmosphere Operations
        double dt_hr = dt_sec / 3600.0;
        if (purge_mode == 1) { // Inerting
            hc_pct = std::max(0.5, hc_pct - 18.0 * dt_hr);
            o2_pct = std::max(1.2, o2_pct - 12.0 * dt_hr);
            ig_pct = 100.0 - hc_pct - o2_pct;
        } else if (purge_mode == 2) { // Aerating
            o2_pct = std::min(20.9, o2_pct + 15.0 * dt_hr);
            hc_pct = std::max(0.0,  hc_pct - 15.0 * dt_hr);
            ig_pct = 100.0 - hc_pct - o2_pct;
        } else if (purge_mode == 3) { // Gassing-Up
            hc_pct = std::min(99.2, hc_pct + 22.0 * dt_hr);
            o2_pct = std::max(0.1,  o2_pct - 10.0 * dt_hr);
            ig_pct = std::max(0.7,  100.0 - hc_pct - o2_pct);
        }
    }
};
