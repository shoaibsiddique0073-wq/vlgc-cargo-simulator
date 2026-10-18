#pragma once
#include <array>
#include <string>
#include <algorithm>

struct FGDZone {
    const char* name;
    double lel_pct = 0.0;  // Hydrocarbon / Ammonia % LEL (Alarm @ 10%, Trip @ 30%)
    double o2_pct  = 0.8;  // Oxygen % (Inerted ~0.8% in Hold Spaces, 20.9% in Comp/Motor Rm)
    double nh3_ppm = 0.0;  // Toxic Ammonia ppm (Alarm @ 25 ppm, Trip @ 100 ppm)
    bool leak_sim  = false;
    int alarm_lvl  = 0;    // 0=NORMAL, 1=LOW ALARM (>10% LEL), 2=HIGH-HIGH TRIP (>30% LEL)
};

struct SafetyFGDSystem {
    // 6 FGD Monitored Locations: 0..3 = Hold Spaces 1-4, 4 = Compressor Room, 5 = Electric Motor Room
    std::array<FGDZone, 6> zones = {{
        {"HOLD SPACE 1",    0.0,  0.8, 0.0, false, 0},
        {"HOLD SPACE 2",    0.0,  0.8, 0.0, false, 0},
        {"HOLD SPACE 3",    0.0,  0.8, 0.0, false, 0},
        {"HOLD SPACE 4",    0.0,  0.8, 0.0, false, 0},
        {"COMPRESSOR ROOM", 0.0, 20.9, 0.0, false, 0},
        {"E-MOTOR ROOM",    0.0, 20.9, 0.0, false, 0}
    }};

    double motor_rm_dp_pa = 55.0; // Motor room positive pressure (+55 Pa normal, <15 Pa alarm)
    bool comp_vent_on     = true; // Compressor room negative extraction fan
    bool fire_loop_ok     = true; // Fusible plug loop (98-104C)
    bool esd1_active      = false;
    std::string last_cause = "SYSTEM NORMAL";

    std::array<bool, 4> hl_95_alarm = {false, false, false, false};
    std::array<bool, 4> hhl_98_trip = {false, false, false, false};

    bool step_safety(
        double dt_sec,
        const std::array<double, 4>& tank_fill_pct,
        const std::array<double, 4>& tank_max_fl,
        const std::array<double, 4>& dome_mbarg,
        const std::array<int, 4>& tank_grade
    ) {
        bool new_alarm = false;

        // 1. Update 6 Fixed Gas Detection (FGD) Sampling Channels
        for (int z = 0; z < 6; ++z) {
            auto& zn = zones[z];
            if (zn.leak_sim) {
                zn.lel_pct = std::min(100.0, zn.lel_pct + 4.5 * std::max(0.2, dt_sec));
                int g_ref = (z < 4) ? tank_grade[z] : tank_grade[0];
                if (g_ref == 2) zn.nh3_ppm = std::min(500.0, zn.nh3_ppm + 25.0 * std::max(0.2, dt_sec));
            } else {
                zn.lel_pct = std::max(0.0, zn.lel_pct - 6.0 * std::max(0.2, dt_sec));
                zn.nh3_ppm = std::max(0.0, zn.nh3_ppm - 30.0 * std::max(0.2, dt_sec));
            }

            int prev = zn.alarm_lvl;
            if (zn.lel_pct >= 30.0 || zn.nh3_ppm >= 100.0) {
                zn.alarm_lvl = 2;
                if (z >= 4 && !esd1_active) { // Confirmed High-High Gas in Comp/Motor Room triggers ESD-1!
                    esd1_active = true;
                    last_cause = std::string("FGD 30% LEL TRIP: ") + zn.name;
                    new_alarm = true;
                }
            } else if (zn.lel_pct >= 10.0 || zn.nh3_ppm >= 25.0) {
                zn.alarm_lvl = 1;
            } else {
                zn.alarm_lvl = 0;
            }
            if (zn.alarm_lvl > prev) new_alarm = true;
        }

        // 2. Motor Room Positive Pressure (dP) & Compressor Room Exhaust Fan Interlock
        if (!comp_vent_on && !esd1_active) {
            esd1_active = true;
            last_cause = "COMP ROOM EXHAUST VENT FAILURE";
            new_alarm = true;
        }
        if (motor_rm_dp_pa < 15.0 && !esd1_active) {
            esd1_active = true;
            last_cause = "MOTOR ROOM LOSS OF +dP (<15 Pa)";
            new_alarm = true;
        }
        if (!fire_loop_ok && !esd1_active) {
            esd1_active = true;
            last_cause = "FUSIBLE FIRE LOOP MELTED (104°C)";
            new_alarm = true;
        }

        // 3. Tank 95% High Alarm, 98% High-High Overflow Trip & Dome Pressure ESD-1
        for (int i = 0; i < 4; ++i) {
            hl_95_alarm[i] = (tank_fill_pct[i] >= 95.0);
            if (tank_fill_pct[i] >= tank_max_fl[i] - 0.05) {
                if (!hhl_98_trip[i]) {
                    hhl_98_trip[i] = true;
                    last_cause = "CT" + std::to_string(i + 1) + " 98% MAX FL OVERFLOW TRIP";
                    new_alarm = true;
                }
            } else {
                hhl_98_trip[i] = false;
            }

            if (dome_mbarg[i] >= 245.0 && !esd1_active) {
                esd1_active = true;
                last_cause = "CT" + std::to_string(i + 1) + " HIGH-HIGH DOME PRESS (>245 mbarg)";
                new_alarm = true;
            } else if (dome_mbarg[i] <= 10.0 && !esd1_active) {
                esd1_active = true;
                last_cause = "CT" + std::to_string(i + 1) + " VACUUM PROTECTION (<10 mbarg)";
                new_alarm = true;
            }
        }
        return new_alarm;
    }

    void trigger_manual_esd() {
        esd1_active = true;
        last_cause = "MANUAL CCR / MANIFOLD ESD-1 PUSHBUTTON";
    }

    void reset_esd() {
        esd1_active = false;
        fire_loop_ok = true;
        comp_vent_on = true;
        motor_rm_dp_pa = 55.0;
        for (auto& z : zones) { z.leak_sim = false; z.lel_pct = 0.0; z.nh3_ppm = 0.0; z.alarm_lvl = 0; }
        last_cause = "ESD-1 RESET - SYSTEM NORMAL";
    }
};
