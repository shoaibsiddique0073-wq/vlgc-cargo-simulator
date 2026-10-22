#pragma once
#include <cmath>
#include <algorithm>

struct CargoGradeProps {
    const char* name;
    double mw, t_boil_c, ant_A, ant_B, ant_C;
    double rho_15_kg_m3, alpha_therm, cp_liq, cp_vap, h_fg_kj_kg, k_isen, t_ref_igc;
};

static constexpr CargoGradeProps GRADES[3] = {
    {"PROPANE (C3)", 44.097, -42.1, 3.92828, 803.997, -26.11, 507.5, 0.00162, 2.25, 1.55, 425.0, 1.13, -36.8},
    {"N-BUTANE (C4)", 58.122, -0.5, 3.93266, 935.773, -34.36, 584.2, 0.00108, 2.32, 1.62, 386.0, 1.10,   4.8},
    {"AMMONIA (NH3)", 17.031, -33.3, 4.48540, 926.132, -32.98, 618.2, 0.00148, 4.45, 2.15, 1369.0, 1.31, -28.2}
};

struct FluidThermo {
    static double sat_press_kpa(int g, double temp_c) {
        const auto& pr = GRADES[std::clamp(g, 0, 2)];
        double tk = std::clamp(temp_c + 273.15, 215.0, 350.0);
        double p_bar_abs = std::pow(10.0, pr.ant_A - pr.ant_B / (tk + pr.ant_C));
        return std::max(0.5, (p_bar_abs - 1.01325) * 100.0);
    }

    // Exact inverse Antoine saturation temperature at gauge pressure (kPaG)
    // Limits minimum possible spray flash cooling to physical boiling point!
    static double sat_temp_c(int g, double press_kpa_g) {
        const auto& pr = GRADES[std::clamp(g, 0, 2)];
        double p_bar_abs = std::max(1.018, 1.01325 + std::max(0.5, press_kpa_g) / 100.0);
        double tk = pr.ant_B / (pr.ant_A - std::log10(p_bar_abs)) - pr.ant_C;
        return std::clamp(tk - 273.15, pr.t_boil_c - 0.2, 65.0);
    }

    static double liq_density(int g, double temp_c) {
        const auto& pr = GRADES[std::clamp(g, 0, 2)];
        return std::clamp(pr.rho_15_kg_m3 * (1.0 - pr.alpha_therm * (temp_c - 15.0)), 420.0, 740.0);
    }

    static double vap_density(int g, double press_kpa_g, double temp_c) {
        const auto& pr = GRADES[std::clamp(g, 0, 2)];
        double p_pa = (101.325 + std::max(0.0, press_kpa_g)) * 1000.0;
        double tk = std::max(220.0, temp_c + 273.15);
        return (p_pa * pr.mw) / (8314.46 * tk);
    }
};
