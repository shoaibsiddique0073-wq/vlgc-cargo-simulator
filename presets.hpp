#pragma once
#include <vector>
#include <string>
#include <array>

struct SimPreset {
    std::string name;
    std::string category; // "PRE-DRYDOCK", "POST-DRYDOCK", "CARGO OPS", "EMERGENCY", "CUSTOM"
    int grade_sys1, grade_sys2; // 0=C3, 1=C4, 2=NH3
    double ct_fill_pct;
    double t_shell_c, t_vap_c, t_liq_c;
    double press_mbarg;
    double hc_pct, ig_pct, o2_pct;
    int purge_mode;
    int comp_mode; // 0=Stop, 1=Reliq, 2=HotGas
    bool top_spray, bot_spray, bot_sparge;
    int man_op;    // 0=Idle, 1=Disch, 2=Load
    bool dwp_run, fill_open, htr_on, bst_on;
    double wbt_fill_pct;
};

inline std::vector<SimPreset> get_default_presets() {
    return {
        // --- PRE-DRYDOCKING SEQUENCE (1 to 4) ---
        {"1. Pre-DD: Stripping & Puddle Heating", "PRE-DRYDOCK", 0, 0, 1.2, -39.0, -35.0, -41.5, 135.0,
         98.5, 1.4, 0.1, 0, 2, false, false, true, 1, false, false, false, false, 65.0},
        {"2. Pre-DD: Tank Warming-Up (+5°C Target)", "PRE-DRYDOCK", 0, 0, 0.0, -18.0, 12.0, -15.0, 160.0,
         98.0, 1.9, 0.1, 0, 2, false, true, true, 0, false, false, false, false, 65.0},
        {"3. Pre-DD: Inerting (IGG/N2 Purging <2% HC)", "PRE-DRYDOCK", 0, 0, 0.0, 18.0, 20.0, 18.0, 60.0,
         14.0, 84.5, 1.5, 1, 0, false, false, false, 0, false, false, false, false, 60.0},
        {"4. Pre-DD: Aerating (Gas-Free for Man Entry)", "PRE-DRYDOCK", 0, 0, 0.0, 24.0, 24.0, 24.0, 15.0,
         0.2, 82.8, 17.0, 2, 0, false, false, false, 0, false, false, false, false, 55.0},

        // --- POST-DRYDOCKING SEQUENCE (5 to 7) ---
        {"5. Post-DD: Drying & Inerting (O2 < 2%)", "POST-DRYDOCK", 0, 0, 0.0, 25.0, 25.0, 25.0, 45.0,
         0.0, 91.5, 8.5, 1, 0, false, false, false, 0, false, false, false, false, 60.0},
        {"6. Post-DD: Gassing-Up (Purging N2 w/ C3 Vapor)", "POST-DRYDOCK", 0, 0, 0.0, 22.0, 15.0, 20.0, 85.0,
         68.0, 31.5, 0.5, 3, 0, false, false, false, 0, false, false, false, false, 60.0},
        {"7. Post-DD: Tank Cool-Down (Top/Bot Spray)", "POST-DRYDOCK", 0, 0, 2.5, 8.0, -12.0, -41.2, 145.0,
         98.8, 1.1, 0.1, 0, 1, true, true, false, 0, false, false, false, false, 60.0},

        // --- STANDARD & SPECIAL CARGO OPERATIONS (8 to 11) ---
        {"8. Ops: Full Homogeneous C3 Propane Loading", "CARGO OPS", 0, 0, 35.0, -40.5, -40.8, -41.5, 110.0,
         99.0, 0.9, 0.1, 0, 1, true, false, false, 2, false, true, false, false, 55.0},
        {"9. Ops: 2-Grade Segregated (Sys1: C3, Sys2: C4)", "CARGO OPS", 0, 1, 68.0, -40.5, -40.8, -41.5, 95.0,
         99.0, 0.9, 0.1, 0, 1, false, false, true, 1, true, false, false, false, 25.0},
        {"10. Ops: Full 8-Pump Discharging + Ballasting", "CARGO OPS", 0, 0, 82.0, -41.0, -41.0, -41.6, 90.0,
         99.0, 0.9, 0.1, 0, 1, false, false, false, 1, true, false, false, false, 15.0},
        {"11. Ops: Heated & Pressurized Discharge (SW Htr+Bst)", "CARGO OPS", 0, 0, 60.0, -40.8, -40.8, -41.5, 105.0,
         99.0, 0.9, 0.1, 0, 1, false, false, false, 1, true, false, true, true, 30.0},

        // --- EMERGENCY DRILLS (12) ---
        {"12. Drill: Overpressure & Trapped Line Mast Venting", "EMERGENCY", 0, 0, 85.0, -34.0, -33.5, -35.8, 256.0,
         99.0, 0.9, 0.1, 0, 0, false, false, false, 1, false, false, false, false, 20.0}
    };
}
