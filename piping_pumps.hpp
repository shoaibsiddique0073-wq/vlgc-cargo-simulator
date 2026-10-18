#pragma once
#include <array>
#include <cmath>
#include <algorithm>

struct DeepwellPump {
    bool running = false;
    double throttle_pct = 25.0;
    double flow_m3_hr = 0.0;
    double disch_bar  = 0.0;
    double amps       = 0.0;
    bool cavitating   = false;
    int trip_code     = 0; // 0=OK, 1=DRY-RUN (<35A), 2=OVERLOAD (>185A), 3=NPSH CAVITATION

    bool step_pump(
        double dt_sec, double& half_vol_m3, double max_half_m3,
        double rho_t_m3, double dome_mbarg, double sat_mbarg, double line_backpress_bar
    ) {
        if (!running) {
            flow_m3_hr = 0.0; disch_bar = 0.0; amps = 0.0; cavitating = false;
            return false;
        }
        if (half_vol_m3 < 18.0) {
            running = false; flow_m3_hr = 0.0; amps = 28.0; trip_code = 1;
            return true;
        }
        double h_shutoff_bar = (145.0 * rho_t_m3 * 9.81) / 100.0 + (dome_mbarg / 1000.0);
        double kv = std::pow(std::clamp(throttle_pct, 0.0, 100.0) / 100.0, 1.35);
        double dp_avail = std::max(0.0, h_shutoff_bar - line_backpress_bar);
        flow_m3_hr = kv * 690.0 * std::sqrt(dp_avail / std::max(1.0, h_shutoff_bar));
        disch_bar  = line_backpress_bar + (1.0 - kv * 0.75) * (h_shutoff_bar - line_backpress_bar);

        double liq_level_m = (half_vol_m3 / max_half_m3) * 18.5;
        double overpress_mbar = dome_mbarg - sat_mbarg;
        double npsh_avail_m = liq_level_m + (overpress_mbar / (rho_t_m3 * 98.1));
        double npsh_req_m   = 1.1 + 2.8 * std::pow(flow_m3_hr / 600.0, 2.0);

        cavitating = (npsh_avail_m < npsh_req_m);
        if (cavitating) {
            flow_m3_hr *= 0.55;
            if (npsh_avail_m < npsh_req_m - 0.9 && throttle_pct > 45.0) {
                running = false; flow_m3_hr = 0.0; amps = 0.0; trip_code = 3;
                return true;
            }
        }

        amps = 44.0 + (flow_m3_hr / 600.0) * 118.0 * (rho_t_m3 / 0.58);
        if (throttle_pct > 82.0 && line_backpress_bar < 2.2) amps += 35.0;
        if (amps > 185.0) {
            running = false; flow_m3_hr = 0.0; trip_code = 2;
            return true;
        }
        if (dt_sec > 0.0) {
            half_vol_m3 = std::max(0.0, half_vol_m3 - (flow_m3_hr / 3600.0) * dt_sec);
        }
        return false;
    }
};

struct LiquidManifold {
    // 2 Header Systems (Sys 1 & Sys 2): 0=IDLE, 1=DISCHARGING, 2=SHORE LOADING
    std::array<int, 2> op_mode = {1, 1};
    // Shore Flange & Manifold Isolation Valves (Sys 1 & Sys 2)
    std::array<bool, 2> shore_conn = {true, true}; // Chicksan Loading Arm Connected
    std::array<bool, 2> esd_vlv    = {true, true}; // Pneumatic Manifold ESD Valve
    std::array<bool, 2> gate_vlv   = {true, true}; // Manual Presentation Gate Valve

    std::array<double, 2> shore_bar = {4.0, 4.0};
    std::array<double, 2> load_rate_m3_hr = {2400.0, 2400.0};
    std::array<double, 2> shore_temp_c = {-41.2, -0.2};
    std::array<bool, 4> fill_vlv = {false, false, false, false};

    // Deck Header PT (bar g) & TT (C) Sensors + Thermal Relief Valves (TRV Setpoint = 10.0 bar g)
    std::array<double, 2> liq_pt_bar  = {4.0, 4.0};
    std::array<double, 2> liq_tt_c    = {-41.2, -0.3};
    std::array<double, 2> vap_pt_bar  = {0.095, 0.095};
    std::array<double, 2> vap_tt_c    = {-39.0, 0.5};
    std::array<double, 2> cond_pt_bar = {0.10, 0.10};
    std::array<double, 2> cond_tt_c   = {-41.0, 0.0};
    std::array<bool, 2>   trv_lifted  = {false, false}; // Liquid Line TRV -> Mast Riser
    std::array<bool, 4>   marvs_lifted = {false, false, false, false}; // Tank MARVS (>250 mbarg)
    std::array<double, 4> mast_vent_kg_hr = {0.0, 0.0, 0.0, 0.0}; // No. 1-4 Vent Mast Risers

    bool heater_on = false, booster_on = false;
    std::array<double, 2> total_flow_m3_hr = {0.0, 0.0};
    double export_temp_c = -41.5, export_bar = 4.0;

    bool is_manifold_open(int sys) const {
        return shore_conn[sys] && esd_vlv[sys] && gate_vlv[sys];
    }
};
