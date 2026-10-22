#pragma once
#include <string>
#include <array>

struct SimPreset {
    std::string name;
    std::string category;
    int grade_sys1, grade_sys2;
    double ct_fill_pct, t_shell_c, t_vap_c, t_liq_c, press_kpa;
    double hc_pct, ig_pct, o2_pct;
    int purge_mode, comp_mode;
    std::array<bool, 3> v_ret;
    int man_op;
    bool dwp_run, fill_vlv;
    double wbt_fill_pct;
};

static const std::array<SimPreset, 12> DEFAULT_PRESETS = {{
    {"1. Pre-DD: Stripping & Puddle Heating", "PRE-DRYDOCK", 0, 0, 1.2, -39.0, -35.0, -41.5, 13.5, 98.5, 1.4, 0.1, 0, 2, {false,false,true}, 1, false, false, 65.0},
    {"2. Pre-DD: Tank Warming-Up (+5C Target)", "PRE-DRYDOCK", 0, 0, 0.0, -18.0, 12.0, -15.0, 16.0, 98.0, 1.9, 0.1, 0, 2, {false,true,true}, 0, false, false, 65.0},
    {"3. Pre-DD: Inerting (IGG/N2 Purging <2% HC)", "PRE-DRYDOCK", 0, 0, 0.0, 18.0, 20.0, 18.0, 6.0, 14.0, 84.5, 1.5, 1, 0, {false,false,false}, 0, false, false, 60.0},
    {"4. Pre-DD: Aerating (Gas-Free for Man Entry)", "PRE-DRYDOCK", 0, 0, 0.0, 24.0, 24.0, 24.0, 1.5, 0.2, 82.8, 17.0, 2, 0, {false,false,false}, 0, false, false, 55.0},
    {"5. Post-DD: Drying & Inerting (O2 < 2%)", "POST-DRYDOCK", 0, 0, 0.0, 25.0, 25.0, 25.0, 4.5, 0.0, 91.5, 8.5, 1, 0, {false,false,false}, 0, false, false, 60.0},
    {"6. Post-DD: Gassing-Up (Purging N2 w/ C3 Vapor)", "POST-DRYDOCK", 0, 0, 0.0, 22.0, 15.0, 20.0, 8.5, 68.0, 31.5, 0.5, 3, 0, {false,false,false}, 0, false, false, 60.0},
    {"7. Post-DD: Tank Cool-Down (Top/Bot Spray)", "POST-DRYDOCK", 0, 0, 2.5, 8.0, -12.0, -41.2, 14.5, 98.8, 1.1, 0.1, 0, 1, {true,true,false}, 0, false, false, 60.0},
    {"8. Ops: Full Homogeneous C3 Propane Loading", "CARGO OPS", 0, 0, 35.0, -40.5, -40.8, -41.5, 11.0, 99.0, 0.9, 0.1, 0, 1, {true,false,false}, 2, false, true, 45.0},
    {"9. Ops: 2-Grade Segregated (Sys1: C3, Sys2: C4)", "CARGO OPS", 0, 1, 68.0, -40.5, -40.8, -41.5, 9.5, 99.0, 0.9, 0.1, 0, 1, {false,false,true}, 1, true, false, 25.0},
    {"10. Ops: Full 8-Pump Discharging + Ballasting", "CARGO OPS", 0, 0, 82.0, -41.0, -41.0, -41.6, 9.0, 99.0, 0.9, 0.1, 0, 1, {false,false,false}, 1, true, false, 15.0},
    {"11. Ops: Heated & Pressurized Discharge", "CARGO OPS", 0, 0, 60.0, -40.8, -40.8, -41.5, 10.5, 99.0, 0.9, 0.1, 0, 1, {false,false,false}, 1, true, false, 30.0},
    {"12. Drill: Overpressure & Trapped Line Mast Venting", "EMERGENCY", 0, 0, 85.0, -34.0, -33.5, -35.8, 25.6, 99.0, 0.9, 0.1, 0, 0, {false,false,false}, 1, false, false, 20.0}
}};
