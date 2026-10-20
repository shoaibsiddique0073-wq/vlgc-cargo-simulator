#pragma once
#include <array>
#include <cmath>
#include <algorithm>

struct DeepwellPump {
    bool running = false;
    double throttle_pct = 25.0; // 0-100% Discharge Throttling Valve
    double flow_m3_hr = 0.0;
    double disch_bar  = 0.0;
    double amps       = 0.0;
    bool cavitating   = false;
    int trip_code     = 0; // 0=OK, 1=DRY-RUN (<35A), 2=OVERLOAD (>185A), 3=NPSH CAVITATION

    bool step_pump(
        double dt_sec, double& half_vol_m3, double& half_mass_mt, double max_half_m3,
        double rho_t_m3, double dome_kpa, double sat_kpa, double line_backpress_bar
    ) {
        if (!running) {
            flow_m3_hr = 0.0; disch_bar = 0.0; amps = 0.0; cavitating = false;
            return false;
        }
        if (half_vol_m3 < 18.0) {
            running = false; flow_m3_hr = 0.0; amps = 28.0; trip_code = 1;
            return true;
        }
        // Shutoff head in bar (145 mLC + dome pressure in bar)
        double h_shutoff_bar = (145.0 * rho_t_m3 * 9.81) / 100.0 + (dome_kpa / 100.0);
        double kv = std::pow(std::clamp(throttle_pct, 0.0, 100.0) / 100.0, 1.35);
        double dp_avail = std::max(0.0, h_shutoff_bar - line_backpress_bar);
        flow_m3_hr = kv * 690.0 * std::sqrt(dp_avail / std::max(1.0, h_shutoff_bar));
        disch_bar  = line_backpress_bar + (1.0 - kv * 0.75) * (h_shutoff_bar - line_backpress_bar);

        // NPSH calculation using dome overpressure in kPa (1 kPa = 10 mbar)
        double liq_level_m = (half_vol_m3 / max_half_m3) * 18.5;
        double overpress_kpa = dome_kpa - sat_kpa;
        double npsh_avail_m = liq_level_m + (overpress_kpa / (rho_t_m3 * 9.81));
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
    // 0=HOLD, 1=BALLAST IN, 2=DEBALLAST OUT, 3=HEEL P->S, 4=HEEL S->P, 5=GRAVITATE SEA
    int op_mode = 1;
    bool ring_xover = true;

    // 0-100% Throttling Butterfly Valves for all 10 WBTs (WBT1P/S..WBT4P/S, FPK, APT)
    std::array<double, 10> wbt_vlv_pct = {100.0, 100.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 10> branch_flow_m3_hr{};
    std::array<double, 2> pump_flow_m3_hr = {0.0, 0.0};
    std::array<double, 2> pump_bar        = {0.0, 0.0};
    std::array<double, 2> pump_amps       = {0.0, 0.0};
    double total_flow_m3_hr = 0.0;

    static constexpr double MAX_SINGLE_PUMP_M3_HR   = 1250.0;
    static constexpr double MAX_PARALLEL_PUMP_M3_HR = 2320.0;
    static constexpr double MAX_BRANCH_LINE_M3_HR   = 650.0;

    static double wbt_water_elev_m(int j, double fill_pct) {
        if (j >= 8) return 3.0 + (fill_pct / 100.0) * 11.0; // FPK / APT
        if (fill_pct <= 45.0) return (fill_pct / 45.0) * 2.8; // Double Bottom (0 - 2.8m)
        if (fill_pct <= 70.0) return 2.8 + ((fill_pct - 45.0) / 25.0) * 9.7; // Trunking (2.8 - 12.5m)
        return 12.5 + ((fill_pct - 70.0) / 30.0) * 8.5; // Topside Wing (12.5 - 21.0m)
    }

    template <typename GeomArray>
    void step_ballast(double dt_sec, std::array<double, 10>& wbt_m3, const GeomArray& geom, double ship_tm_m) {
        branch_flow_m3_hr.fill(0.0);
        int n_pumps = (pump_run[0] ? 1 : 0) + (pump_run[1] ? 1 : 0);

        // Mode 5: Hydrostatic Gravity Ballasting / Deballasting via Sea Chest (No Pump Required!)
        if (op_mode == 5) {
            total_flow_m3_hr = 0.0;
            for (int j = 0; j < 10; ++j) {
                double kv = std::pow(std::clamp(wbt_vlv_pct[j], 0.0, 100.0) / 100.0, 1.25);
                if (kv < 0.01) continue;
                double pct = (wbt_m3[j] / geom[j].max_m3) * 100.0;
                double h_tk = wbt_water_elev_m(j, pct);
                double dh = ship_tm_m - h_tk; // +ve = Sea higher than tank (Gravitate IN), -ve = Tank higher (Gravitate OUT)
                if (std::abs(dh) > 0.15) {
                    double q = std::copysign( std::min(MAX_BRANCH_LINE_M3_HR * kv, kv * 175.0 * std::sqrt(std::abs(dh))), dh );
                    if (q > 0.0 && wbt_m3[j] >= geom[j].max_m3 - 0.5) q = 0.0;
                    if (q < 0.0 && wbt_m3[j] <= 0.5) q = 0.0;
                    branch_flow_m3_hr[j] = q;
                    total_flow_m3_hr += std::abs(q);
                    if (dt_sec > 0.0) wbt_m3[j] = std::clamp(wbt_m3[j] + (q / 3600.0) * dt_sec, 0.0, geom[j].max_m3);
                }
            }
            pump_flow_m3_hr = {0.0, 0.0}; pump_bar = {0.0, 0.0}; pump_amps = {0.0, 0.0};
            return;
        }

        if (n_pumps == 0 || op_mode == 0) {
            pump_flow_m3_hr = {0.0, 0.0}; pump_bar = {0.0, 0.0}; pump_amps = {0.0, 0.0};
            total_flow_m3_hr = 0.0;
            return;
        }

        double max_pump_cap = (n_pumps == 1) ? MAX_SINGLE_PUMP_M3_HR : MAX_PARALLEL_PUMP_M3_HR;

        if (op_mode == 1 || op_mode == 2) {
            // Compute throttle conductance & static head weight for each WBT branch
            std::array<double, 10> br_cap{};
            double sum_cap = 0.0;
            for (int j = 0; j < 10; ++j) {
                double kv = std::pow(std::clamp(wbt_vlv_pct[j], 0.0, 100.0) / 100.0, 1.25);
                if (kv < 0.01) continue;
                if (!ring_xover) {
                    if (j < 8 && (j % 2 == 0) && !pump_run[0]) continue;
                    if (j < 8 && (j % 2 == 1) && !pump_run[1]) continue;
                }
                double pct = (wbt_m3[j] / geom[j].max_m3) * 100.0;
                double h_tk = wbt_water_elev_m(j, pct);
                // Head factor: pumping up into Topside Wing (20m) faces higher static head than Double Bottom (2m)
                double head_factor = (op_mode == 1)
                    ? std::sqrt(std::clamp((28.0 - (h_tk - ship_tm_m)) / 28.0, 0.25, 1.25))
                    : std::sqrt(std::clamp((28.0 + (h_tk - ship_tm_m)) / 28.0, 0.25, 1.25));

                if (op_mode == 1 && wbt_m3[j] < geom[j].max_m3 - 0.5) {
                    br_cap[j] = MAX_BRANCH_LINE_M3_HR * kv * head_factor;
                    sum_cap += br_cap[j];
                } else if (op_mode == 2 && wbt_m3[j] > 0.5) {
                    br_cap[j] = MAX_BRANCH_LINE_M3_HR * kv * head_factor;
                    sum_cap += br_cap[j];
                }
            }

            if (sum_cap < 0.1) {
                total_flow_m3_hr = 0.0;
                for (int p = 0; p < 2; ++p) {
                    pump_flow_m3_hr[p] = 0.0;
                    pump_bar[p]  = pump_run[p] ? 3.45 : 0.0;
                    pump_amps[p] = pump_run[p] ? 68.0 : 0.0;
                }
                return;
            }

            // Scale branch flows so total never exceeds active Singular/Parallel pump capacity
            total_flow_m3_hr = std::min(max_pump_cap, sum_cap);
            double scale = total_flow_m3_hr / sum_cap;

            for (int j = 0; j < 10; ++j) {
                if (br_cap[j] <= 0.0) continue;
                double q = br_cap[j] * scale;
                if (op_mode == 1) {
                    branch_flow_m3_hr[j] = q;
                    if (dt_sec > 0.0) wbt_m3[j] = std::min(geom[j].max_m3, wbt_m3[j] + (q / 3600.0) * dt_sec);
                } else {
                    branch_flow_m3_hr[j] = -q;
                    if (dt_sec > 0.0) wbt_m3[j] = std::max(0.0, wbt_m3[j] - (q / 3600.0) * dt_sec);
                }
            }
        } else if (op_mode == 3 || op_mode == 4) {
            // Internal Heel Transfer Port <-> Starboard throttled by valve opening %
            std::array<double, 4> pair_cap{};
            double sum_pair_cap = 0.0;
            for (int pair = 0; pair < 4; ++pair) {
                int ip = pair * 2, is = pair * 2 + 1;
                double kv_pair = std::min(wbt_vlv_pct[ip], wbt_vlv_pct[is]) / 100.0;
                if (kv_pair > 0.01) {
                    pair_cap[pair] = MAX_BRANCH_LINE_M3_HR * std::pow(kv_pair, 1.25);
                    sum_pair_cap += pair_cap[pair];
                }
            }
            total_flow_m3_hr = std::min(max_pump_cap, sum_pair_cap);
            double scale = (sum_pair_cap > 0.1) ? (total_flow_m3_hr / sum_pair_cap) : 0.0;

            for (int pair = 0; pair < 4; ++pair) {
                if (pair_cap[pair] <= 0.0) continue;
                int ip = pair * 2, is = pair * 2 + 1;
                double q_pair = pair_cap[pair] * scale;
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
    std::array<double, 2> gate_vlv_pct = {100.0, 100.0}; // 0-100% Throttling Manifold Gate Valves!

    std::array<double, 2> shore_bar = {4.0, 4.0};
    std::array<double, 2> load_rate_m3_hr = {2400.0, 2400.0};
    std::array<double, 2> shore_temp_c = {-41.2, -0.2};

    // Deck Header PT (bar g) & TT (C) Sensors + TRVs
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
        return shore_conn[sys] && esd_vlv[sys] && (gate_vlv_pct[sys] > 1.0);
    }

    double gate_kv(int sys) const {
        if (!is_manifold_open(sys)) return 0.0;
        return std::pow(std::clamp(gate_vlv_pct[sys], 0.0, 100.0) / 100.0, 1.25);
    }
};
