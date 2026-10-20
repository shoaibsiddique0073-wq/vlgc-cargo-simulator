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
    int trip_code     = 0;

    bool step_pump(
        double dt_sec, double& half_vol_m3, double& half_mass_mt, double max_half_m3,
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
        if (dt_sec > 0.0 && flow_m3_hr > 0.0) {
            double dv = std::min(half_vol_m3, (flow_m3_hr / 3600.0) * dt_sec);
            half_vol_m3 -= dv;
            half_mass_mt = std::max(0.0, half_mass_mt - dv * rho_t_m3);
        }
        return false;
    }
};

struct BallastSystem {
    // 2 Main Centrifugal Ballast Pumps (1,250 m3/hr @ 2.8 bar design each)
    std::array<bool, 2> pump_run = {false, false};
    // 0=HOLD/ISOLATED, 1=BALLAST FROM SEA, 2=DEBALLAST TO SEA, 3=HEEL P->S, 4=HEEL S->P
    int op_mode = 1;
    bool ring_xover = true; // Port/Starboard Ring Main Crossover Valve

    // Individual Branch Valves for all 10 WBTs (WBT1P/S..WBT4P/S, FPK, APT)
    std::array<bool, 10> wbt_vlv = {true, true, false, false, false, false, false, false, false, false};
    std::array<double, 10> branch_flow_m3_hr{};
    std::array<double, 2> pump_flow_m3_hr = {0.0, 0.0};
    std::array<double, 2> pump_bar        = {0.0, 0.0};
    std::array<double, 2> pump_amps       = {0.0, 0.0};
    double total_flow_m3_hr = 0.0;

    static constexpr double MAX_SINGLE_PUMP_M3_HR   = 1250.0; // 1 Pump Singular Capacity
    static constexpr double MAX_PARALLEL_PUMP_M3_HR = 2320.0; // 2 Pumps Parallel Capacity (with pipe friction)
    static constexpr double MAX_BRANCH_LINE_M3_HR   = 650.0;  // Max velocity cap per DN350 WBT branch line

    template <typename GeomArray>
    void step_ballast(double dt_sec, std::array<double, 10>& wbt_m3, const GeomArray& geom) {
        branch_flow_m3_hr.fill(0.0);
        int n_pumps = (pump_run[0] ? 1 : 0) + (pump_run[1] ? 1 : 0);
        if (n_pumps == 0 || op_mode == 0) {
            pump_flow_m3_hr = {0.0, 0.0};
            pump_bar = {0.0, 0.0};
            pump_amps = {0.0, 0.0};
            total_flow_m3_hr = 0.0;
            return;
        }

        // Total available pump capacity (Singular vs Parallel)
        double max_pump_cap = (n_pumps == 1) ? MAX_SINGLE_PUMP_M3_HR : MAX_PARALLEL_PUMP_M3_HR;

        if (op_mode == 1 || op_mode == 2) {
            // Count open and eligible WBT branch valves
            int n_open = 0;
            for (int j = 0; j < 10; ++j) {
                if (!wbt_vlv[j]) continue;
                // If crossover is shut, Pump 1 (Port) only serves Port/Center, Pump 2 (Stbd) only serves Stbd/Center
                if (!ring_xover) {
                    bool is_port = (j < 8 && (j % 2 == 0));
                    bool is_stbd = (j < 8 && (j % 2 == 1));
                    if (is_port && !pump_run[0]) continue;
                    if (is_stbd && !pump_run[1]) continue;
                }
                if (op_mode == 1 && wbt_m3[j] < geom[j].max_m3 - 0.5) n_open++;
                if (op_mode == 2 && wbt_m3[j] > 0.5) n_open++;
            }

            if (n_open == 0) {
                // Dead-headed against closed WBT branch valves!
                total_flow_m3_hr = 0.0;
                for (int p = 0; p < 2; ++p) {
                    pump_flow_m3_hr[p] = 0.0;
                    pump_bar[p]  = pump_run[p] ? 3.45 : 0.0;
                    pump_amps[p] = pump_run[p] ? 68.0 : 0.0;
                }
                return;
            }

            // Total system flow cannot exceed active pump capacity OR sum of open branch line caps
            total_flow_m3_hr = std::min(max_pump_cap, n_open * MAX_BRANCH_LINE_M3_HR);
            double flow_per_branch = total_flow_m3_hr / n_open;

            for (int j = 0; j < 10; ++j) {
                if (!wbt_vlv[j]) continue;
                if (!ring_xover) {
                    if (j < 8 && (j % 2 == 0) && !pump_run[0]) continue;
                    if (j < 8 && (j % 2 == 1) && !pump_run[1]) continue;
                }
                if (op_mode == 1 && wbt_m3[j] < geom[j].max_m3 - 0.5) {
                    branch_flow_m3_hr[j] = flow_per_branch;
                    if (dt_sec > 0.0) wbt_m3[j] = std::min(geom[j].max_m3, wbt_m3[j] + (flow_per_branch / 3600.0) * dt_sec);
                } else if (op_mode == 2 && wbt_m3[j] > 0.5) {
                    branch_flow_m3_hr[j] = -flow_per_branch;
                    if (dt_sec > 0.0) wbt_m3[j] = std::max(0.0, wbt_m3[j] - (flow_per_branch / 3600.0) * dt_sec);
                }
            }
        } else if (op_mode == 3 || op_mode == 4) {
            // Internal Port <-> Starboard Heel Transfer across open WBT pairs (1..4)
            int n_pairs = 0;
            for (int pair = 0; pair < 4; ++pair) {
                int ip = pair * 2, is = pair * 2 + 1;
                if (wbt_vlv[ip] && wbt_vlv[is]) n_pairs++;
            }
            total_flow_m3_hr = (n_pairs > 0) ? std::min(max_pump_cap, n_pairs * MAX_BRANCH_LINE_M3_HR) : 0.0;
            double q_pair = (n_pairs > 0) ? (total_flow_m3_hr / n_pairs) : 0.0;
            for (int pair = 0; pair < 4; ++pair) {
                int ip = pair * 2, is = pair * 2 + 1;
                if (!wbt_vlv[ip] || !wbt_vlv[is]) continue;
                double dv = (q_pair / 3600.0) * dt_sec;
                if (op_mode == 3 && wbt_m3[ip] > 1.0 && wbt_m3[is] < geom[is].max_m3 - 1.0) {
                    branch_flow_m3_hr[ip] = -q_pair; branch_flow_m3_hr[is] = q_pair;
                    if (dt_sec > 0.0) { wbt_m3[ip] -= dv; wbt_m3[is] += dv; }
                } else if (op_mode == 4 && wbt_m3[is] > 1.0 && wbt_m3[ip] < geom[ip].max_m3 - 1.0) {
                    branch_flow_m3_hr[is] = -q_pair; branch_flow_m3_hr[ip] = q_pair;
                    if (dt_sec > 0.0) { wbt_m3[is] -= dv; wbt_m3[ip] += dv; }
                }
            }
        }

        // Update Individual Pump Telemetry (Singular vs Parallel load sharing)
        for (int p = 0; p < 2; ++p) {
            if (pump_run[p]) {
                pump_flow_m3_hr[p] = total_flow_m3_hr / n_pumps;
                pump_bar[p]  = 3.40 - (pump_flow_m3_hr[p] / 1250.0) * 0.85;
                pump_amps[p] = 68.0 + (pump_flow_m3_hr[p] / 1250.0) * 112.0;
            } else {
                pump_flow_m3_hr[p] = 0.0; pump_bar[p] = 0.0; pump_amps[p] = 0.0;
            }
        }
    }
};

struct LiquidManifold {
    std::array<int, 2> op_mode = {1, 1};
    std::array<bool, 2> shore_conn = {true, true};
    std::array<bool, 2> esd_vlv    = {true, true};
    std::array<bool, 2> gate_vlv   = {true, true};

    std::array<double, 2> shore_bar = {4.0, 4.0};
    std::array<double, 2> load_rate_m3_hr = {2400.0, 2400.0};
    std::array<double, 2> shore_temp_c = {-41.2, -0.2};
    std::array<bool, 4> fill_vlv = {false, false, false, false};

    std::array<double, 2> liq_pt_bar  = {4.0, 4.0};
    std::array<double, 2> liq_tt_c    = {-41.2, -0.3};
    std::array<double, 2> vap_pt_bar  = {0.095, 0.095};
    std::array<double, 2> vap_tt_c    = {-39.0, 0.5};
    std::array<double, 2> cond_pt_bar = {0.10, 0.10};
    std::array<double, 2> cond_tt_c   = {-41.0, 0.0};
    std::array<bool, 2>   trv_lifted  = {false, false};
    std::array<bool, 4>   marvs_lifted = {false, false, false, false};
    std::array<double, 4> mast_vent_kg_hr = {0.0, 0.0, 0.0, 0.0};

    bool heater_on = false, booster_on = false;
    std::array<double, 2> total_flow_m3_hr = {0.0, 0.0};
    double export_temp_c = -41.5, export_bar = 4.0;

    bool is_manifold_open(int sys) const {
        return shore_conn[sys] && esd_vlv[sys] && gate_vlv[sys];
    }
};
