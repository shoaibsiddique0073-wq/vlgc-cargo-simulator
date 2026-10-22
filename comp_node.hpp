#pragma once
#include "fluid_props.hpp"

struct CompressorUnit {
    int mode = 0;   // 0=STOP, 1=RELIQ, 2=HOT GAS
    int sys_id = 0; // 0=SYS1, 1=SYS2
    double sw_eff_pct = 92.0;
    double suc_kpa = 8.0;
    double cond_bar = 8.5;
    double disch_temp_c = 55.0;
    double load_pct = 0.0; // Unloader 0-100% (Cuts out at 1.5 kPaG suction floor!)
    double vap_draw_kg_s = 0.0;

    void step(int g, double hdr_suc_kpa, double hdr_vap_c, double amb_sw_c, bool connected) {
        suc_kpa = hdr_suc_kpa;
        sw_eff_pct = std::clamp(100.0 - std::max(0.0, amb_sw_c - 15.0) * 1.35, 58.0, 100.0);
        double t_cond_sat = amb_sw_c + 9.5;
        cond_bar = std::clamp(1.013 + FluidThermo::sat_press_kpa(g, t_cond_sat) / 100.0, 4.5, 18.5);

        if (mode == 0 || !connected) {
            load_pct = 0.0; vap_draw_kg_s = 0.0; disch_temp_c = amb_sw_c;
            return;
        }
        // Automatic Suction Unloader: prevents vacuum and runaway cooling below T_boil
        if (suc_kpa <= 1.5) load_pct = 0.0;
        else if (suc_kpa < 6.0) load_pct = ((suc_kpa - 1.5) / 4.5) * 100.0;
        else load_pct = 100.0;

        double p_suc_abs = 1.01325 + std::max(0.01, suc_kpa) / 100.0;
        double rp = std::max(1.2, cond_bar / p_suc_abs);
        double eta_vol = std::clamp(0.94 - 0.035 * rp, 0.45, 0.90);
        double rho_suc = FluidThermo::vap_density(g, suc_kpa, hdr_vap_c);
        double v_swept_m3_s = 0.92; // Sulzer 2-stage labyrinth compressor swept volume

        vap_draw_kg_s = v_swept_m3_s * eta_vol * rho_suc * (load_pct / 100.0) * (mode == 1 ? sw_eff_pct / 100.0 : 1.0);
        double k = GRADES[std::clamp(g, 0, 2)].k_isen;
        double tk_in = std::max(225.0, hdr_vap_c + 273.15);
        disch_temp_c = (tk_in * std::pow(rp, (k - 1.0) / k)) - 273.15;
    }
};
