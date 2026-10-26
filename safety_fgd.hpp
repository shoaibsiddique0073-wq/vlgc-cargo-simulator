#pragma once
#include <string>
#include <array>
#include <algorithm>

struct FGDZone {
    double hc_lel_pct = 0.0;
    double o2_pct = 0.8;
    double nh3_ppm = 0.0;
    bool leak_sim = false;
    int alarm_state = 0; // 0=OK, 1=WARN (>20% LEL), 2=ESD TRIP (>40% LEL)
};

struct SafetyFGDSystem {
    std::array<FGDZone, 6> zones{};
    bool esd1_active = false;
    std::string last_cause = "SYSTEM NORMAL";
    double motor_rm_dp_pa = 55.0;
    bool comp_vent_on = true;
    bool fire_loop_ok = true;

    SafetyFGDSystem() {
        for (int z = 0; z < 6; ++z) zones[z].o2_pct = (z < 4) ? 0.8 : 20.9;
    }

    void trigger_manual_esd() { trigger_esd("MANUAL ESD-1 PUSHBUTTON"); }
    void trigger_esd(const std::string& reason) {
        esd1_active = true;
        last_cause = reason;
    }

    void reset_esd() {
        esd1_active = false;
        last_cause = "ESD RESET - NORMAL";
        motor_rm_dp_pa = 55.0;
        comp_vent_on = true;
        fire_loop_ok = true;
        for (auto& z : zones) {
            z.hc_lel_pct = 0.0;
            z.nh3_ppm = 0.0;
            z.leak_sim = false;
            z.alarm_state = 0;
        }
    }

    void step(double dt) {
        static const char* z_names[6] = {
            "HOLD SPACE 1", "HOLD SPACE 2", "HOLD SPACE 3",
            "HOLD SPACE 4", "COMPRESSOR ROOM", "E-MOTOR ROOM"
        };
        for (int i = 0; i < 6; ++i) {
            auto& z = zones[i];
            if (z.leak_sim) {
                z.hc_lel_pct = std::min(100.0, z.hc_lel_pct + 4.5 * dt);
                z.nh3_ppm = std::min(500.0, z.nh3_ppm + 18.0 * dt);
            } else {
                z.hc_lel_pct = std::max(0.0, z.hc_lel_pct - 6.0 * dt);
                z.nh3_ppm = std::max(0.0, z.nh3_ppm - 25.0 * dt);
            }
            if (z.hc_lel_pct >= 40.0 || z.nh3_ppm >= 150.0) {
                z.alarm_state = 2;
                trigger_esd(std::string("FGD HIGH-HIGH GAS: ") + z_names[i]);
            } else if (z.hc_lel_pct >= 20.0 || z.nh3_ppm >= 50.0) {
                z.alarm_state = 1;
            } else {
                z.alarm_state = 0;
            }
        }
        if (!comp_vent_on) trigger_esd("COMP ROOM VENTILATION TRIP");
        if (motor_rm_dp_pa < 20.0) trigger_esd("MOTOR ROOM +dP LOSS (<20 Pa)");
        if (!fire_loop_ok) trigger_esd("FUSIBLE FIRE LOOP MELT (104C)");
    }
};
