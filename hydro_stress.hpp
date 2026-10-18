#pragma once
#include "ship_tables.hpp"
#include <cmath>
#include <algorithm>

struct HydroState {
    double disp_mt = 0.0, tf = 0.0, ta = 0.0, tm = 0.0, trim_m = 0.0;
    double tpc = 0.0, mctc = 0.0, lcb = 0.0, lcf = 0.0, lcg = 0.0;
    double kmt = 0.0, kg_solid = 0.0, fsc = 0.0, gm_fluid = 0.0;
    double tcg = 0.0, heel_deg = 0.0;
    double max_sf_pct = 0.0, max_bm_pct = 0.0; // % of Seagoing Permissible Limits
    double harbor_sf_pct = 0.0, harbor_bm_pct = 0.0;
    std::array<double, 20> sf_stations{};
    std::array<double, 20> bm_stations{};
};

struct HullSolver {
    static constexpr double LBP = 222.0;
    static constexpr double LIGHTSHIP_MT = 18650.0;
    static constexpr double LS_LCG = -8.45, LS_VCG = 13.80;
    static constexpr double RHO_SW = 1.025; // Seawater density (t/m3)

    static void solve(
        const std::array<std::array<double, 2>, 4>& ct_vol_m3, // [tank 0..3][0=Port, 1=Stbd]
        const std::array<bool, 4>& bhd_open,
        double cargo_rho_t_m3,
        const std::array<double, 10>& wbt_vol_m3,
        HydroState& out
    ) {
        double sum_w = LIGHTSHIP_MT;
        double sum_mx = LIGHTSHIP_MT * LS_LCG;
        double sum_my = 0.0; // Transverse moment (+Stbd, -Port)
        double sum_mz = LIGHTSHIP_MT * LS_VCG;
        double sum_fsm = 0.0;

        // 20-Station weight distribution array (Aft stn 0 to Fwd stn 19)
        std::array<double, 20> stn_w{};
        for (int s = 0; s < 20; ++s) {
            // Lightship weight distribution (heavier aft at engine/deckhouse)
            stn_w[s] = (LIGHTSHIP_MT / 20.0) * (s < 4 ? 1.45 : (s > 16 ? 0.75 : 0.95));
        }

        // 1. Integrate Cargo Tanks 1-4 (Port & Starboard halves)
        for (int i = 0; i < 4; ++i) {
            for (int side = 0; side < 2; ++side) {
                double v = ct_vol_m3[i][side];
                double fill_pct = (v / CT_GEOM[i].max_half_m3) * 100.0;
                double w = v * cargo_rho_t_m3;
                double sign = (side == 0) ? -1.0 : 1.0; // 0 = Port (-), 1 = Stbd (+)
                double tcg_half = sign * (CT_GEOM[i].half_beam * 0.48);
                double vcg_half = ct_vcg(fill_pct);
                double it_half  = ct_half_inertia(i, fill_pct);
                // Open bulkhead valve slightly couples P/S sloshing (+25% FSM factor)
                if (bhd_open[i]) it_half *= 1.25;

                sum_w   += w;
                sum_mx  += w * CT_GEOM[i].lcg;
                sum_my  += w * tcg_half;
                sum_mz  += w * vcg_half;
                sum_fsm += it_half * cargo_rho_t_m3;

                // Map Cargo Tank i to longitudinal stations
                int s_start = 15 - i * 3;
                for (int s = s_start; s < s_start + 3; ++s) stn_w[s] += w / 3.0;
            }
        }

        // 2. Integrate Ballast Tanks (WBT 1P/S-4P/S, FPK, APT)
        for (int j = 0; j < 10; ++j) {
            double v = wbt_vol_m3[j];
            double fill_pct = (v / WBT_GEOM[j].max_m3) * 100.0;
            double w = v * RHO_SW;
            auto props = wbt_properties(j, fill_pct); // {vcg, tcg, it, zone}

            sum_w   += w;
            sum_mx  += w * WBT_GEOM[j].lcg;
            sum_my  += w * props[1];
            sum_mz  += w * props[0];
            sum_fsm += props[2] * RHO_SW;

            if (j < 8) {
                int tank_idx = j / 2;
                int s_start = 15 - tank_idx * 3;
                for (int s = s_start; s < s_start + 3; ++s) stn_w[s] += w / 3.0;
            } else if (j == 8) {
                stn_w[18] += w * 0.5; stn_w[19] += w * 0.5; // FPK
            } else {
                stn_w[0]  += w * 0.5; stn_w[1]  += w * 0.5; // APT
            }
        }

        // 3. Hydrostatic Table Lookup & Drafts/Trim/Heel
        out.disp_mt = sum_w;
        HydroRow hr = interp_hydro(sum_w);
        out.tm   = hr.draft;
        out.tpc  = hr.tpc;
        out.mctc = hr.mctc;
        out.lcb  = hr.lcb;
        out.lcf  = hr.lcf;
        out.kmt  = hr.kmt;

        out.lcg      = sum_mx / sum_w;
        out.tcg      = sum_my / sum_w;
        out.kg_solid = sum_mz / sum_w;
        out.fsc      = sum_fsm / sum_w;
        out.gm_fluid = std::max(0.15, out.kmt - out.kg_solid - out.fsc);

        out.trim_m = (sum_w * (out.lcg - out.lcb)) / (out.mctc * 100.0); // +ve = Trim by Head
        out.tf = out.tm + out.trim_m * ((LBP / 2.0 - out.lcf) / LBP);
        out.ta = out.tm - out.trim_m * ((LBP / 2.0 + out.lcf) / LBP);

        // Static Heel/List Angle (+ve = Stbd List, -ve = Port List)
        double rad_heel = std::atan2(out.tcg, out.gm_fluid);
        out.heel_deg = std::clamp(rad_heel * (180.0 / M_PI), -18.0, 18.0);

        // 4. 20-Station Longitudinal Hull Girder Shear Force & Bending Moment
        double sf_run = 0.0, bm_run = 0.0, peak_sf = 0.0, peak_bm = 0.0;
        const double dx = LBP / 20.0;
        for (int s = 0; s < 20; ++s) {
            double x_norm = (s - 9.5) / 10.0; // -1.0 (Aft) to +1.0 (Fwd)
            // Parabolic/prismatic buoyancy distribution along submerged hull
            double b_factor = 1.18 * (1.0 - 0.45 * std::pow(x_norm, 4)) + 0.04 * out.trim_m * x_norm;
            double stn_b = (sum_w / 20.0) * b_factor;
            double net_load = stn_b - stn_w[s]; // +ve upward buoyancy excess (Hogging tendency)

            sf_run += net_load;
            bm_run += sf_run * dx;
            out.sf_stations[s] = (sf_run / 8500.0) * 100.0;   // Seagoing SF limit = 8,500 MT
            out.bm_stations[s] = (bm_run / 195000.0) * 100.0; // Seagoing BM limit = 195,000 MT-m

            if (std::abs(out.sf_stations[s]) > std::abs(peak_sf)) peak_sf = out.sf_stations[s];
            if (std::abs(out.bm_stations[s]) > std::abs(peak_bm)) peak_bm = out.bm_stations[s];
        }
        out.max_sf_pct = std::clamp(std::abs(peak_sf), 0.0, 150.0);
        out.max_bm_pct = std::clamp(peak_bm, -150.0, 150.0);
        out.harbor_sf_pct = out.max_sf_pct * 0.76; // Harbor permissible limits are ~30% higher
        out.harbor_bm_pct = out.max_bm_pct * 0.76;
    }
};
