#pragma once
#include "fluid_props.hpp"

struct DeepwellPump {
    bool running = false;
    double throttle_pct = 25.0;
    double flow_m3_hr = 0.0, disch_bar = 0.0, amps = 0.0;
    bool cavitating = false;
    int trip_code = 0; // 0=OK, 1=DRY-RUN (<35A), 2=OVERLOAD (>185A), 3=NPSH CAVITATION
    double low_amp_timer = 0.0;

    void step(double dt, double fill_pct, double rho_kg_m3, double dome_kpa,
              double p_sat_kpa, double hdr_back_bar, bool path_open) {
        if (!running) {
            flow_m3_hr = 0.0; disch_bar = 0.0; amps = 0.0; cavitating = false; low_amp_timer = 0.0;
            return;
        }
        double z_liq_m = (fill_pct / 100.0) * 18.5;
        double npsh_a = ((dome_kpa - p_sat_kpa) * 1000.0) / (rho_kg_m3 * 9.81) + z_liq_m;
        double thr = std::clamp(throttle_pct, 5.0, 100.0) / 100.0;
        double npsh_r = 0.55 + 1.65 * thr * thr;

        if (npsh_a < npsh_r || fill_pct < 0.25) {
            cavitating = true;
            if (fill_pct < 0.15 || npsh_a < 0.25) {
                running = false; trip_code = (fill_pct < 0.15) ? 1 : 3;
                flow_m3_hr = 0.0; disch_bar = 0.0; amps = 0.0;
                return;
            }
        } else {
            cavitating = false;
        }

        double p_static_bar = (dome_kpa / 100.0) + (rho_kg_m3 * 9.81 * z_liq_m) / 1e5;
        double dp_req = std::max(0.2, hdr_back_bar - p_static_bar);
        double dp_shutoff = 13.8; // Centrifugal shutoff head (bar)

        if (!path_open || dp_req >= dp_shutoff) {
            flow_m3_hr = 0.0;
            disch_bar = dp_shutoff + p_static_bar;
            amps = 44.0;
        } else {
            double curve_factor = std::sqrt(std::max(0.0, 1.0 - (dp_req / dp_shutoff) * (dp_req / dp_shutoff)));
            flow_m3_hr = 620.0 * thr * curve_factor * (cavitating ? 0.45 : 1.0);
            disch_bar = hdr_back_bar + 0.35 * thr;
            amps = 42.0 + (flow_m3_hr / 620.0) * 128.0 * (rho_kg_m3 / 580.0);
        }

        if (amps < 35.0) {
            low_amp_timer += dt;
            if (low_amp_timer > 4.0) { running = false; trip_code = 1; flow_m3_hr = 0.0; }
        } else { low_amp_timer = 0.0; }

        if (amps > 185.0) { running = false; trip_code = 2; flow_m3_hr = 0.0; }
    }
};
