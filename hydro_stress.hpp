#pragma once
#include "ship_tables.hpp"
#include <cmath>
#include <algorithm>

struct HydroState {
    double disp_mt = 0.0, tf = 0.0, ta = 0.0, tm = 0.0, trim_m = 0.0;
    double tpc = 0.0, mctc = 0.0, lcb = 0.0, lcf = 0.0, lcg = 0.0;
    double kmt = 0.0, kg_solid = 0.0, fsc = 0.0, gm_fluid = 0.0;
    double tcg = 0.0, heel_deg = 0.0;
    double max_sf_pct = 0.0, max_bm_pct = 0.0;
    double harbor_sf_pct = 0.0, harbor_bm_pct = 0.0;
    std::array<double, 20> sf_stations{};
    std::array<double, 20> bm_stations{};
};

struct HullSolver {
    static constexpr double LBP = 222.0;
    static constexpr double LIGHTSHIP_MT = 18650.0;
    static constexpr double LS_LCG = -4.25, LS_VCG = 13.60;
    static constexpr double RHO_SW = 1.025;

    static void solve(
        const std::array<std::array<double, 2>, 4>& ct_vol_m3,
        const std::array<std::array<double, 2>, 4>& ct_mass_mt,
        const std::array<bool, 4>& bhd_open,
        const std::array<double, 4>& cargo_rho_t_m3,
        const std::array<double, 10>& wbt_vol_m3,
        HydroState& out
    ) {
        double sum_w = LIGHTSHIP_MT, sum_mx = LIGHTSHIP_MT * LS_LCG;
        double sum_my = 0.0, sum_mz = LIGHTSHIP_MT * LS_VCG, sum_fsm = 0.0;

        static constexpr std::array<double, 20> ls_shape = {
            1.28, 1.35, 1.22, 1.05, 0.96, 0.96, 0.96, 0.96, 0.96, 0.96,
            0.96, 0.96, 0.96, 0.96, 0.96, 0.95, 0.92, 0.90, 0.88, 0.85
        };
        std::array<double, 20> stn_w{};
        for (int s = 0; s < 20; ++s) stn_w[s] = (LIGHTSHIP_MT / 20.0) * ls_shape[s];

        static constexpr int ct_s0[4] = {13, 10, 6, 3};
        static constexpr int ct_s1[4] = {16, 13, 9, 6};

        for (int i = 0; i < 4; ++i) {
            double rho = cargo_rho_t_m3[i];
            for (int side = 0; side < 2; ++side) {
                double v = ct_vol_m3[i][side], w = ct_mass_mt[i][side];
                double fill_pct = (v / CT_GEOM[i].max_half_m3) * 100.0;
                double sign = (side == 0) ? -1.0 : 1.0;
                double tcg_half = sign * (CT_GEOM[i].half_beam * 0.46);
                double it_half = ct_half_inertia(i, fill_pct) * (bhd_open[i] ? 1.35 : 1.0);
                sum_w += w; sum_mx += w * CT_GEOM[i].lcg;
                sum_my += w * tcg_half; sum_mz += w * ct_vcg(fill_pct);
                sum_fsm += it_half * rho;
                int n_stn = ct_s1[i] - ct_s0[i] + 1;
                for (int s = ct_s0[i]; s <= ct_s1[i]; ++s) stn_w[s] += w / n_stn;
            }
        }

        for (int j = 0; j < 10; ++j) {
            double v = wbt_vol_m3[j], fill_pct = (v / WBT_GEOM[j].max_m3) * 100.0, w = v * RHO_SW;
            auto props = wbt_properties(j, fill_pct);
            sum_w += w; sum_mx += w * WBT_GEOM[j].lcg;
            sum_my += w * props[1]; sum_mz += w * props[0]; sum_fsm += props[2] * RHO_SW;
            if (j < 8) {
                int tk = j / 2, n_stn = ct_s1[tk] - ct_s0[tk] + 1;
                for (int s = ct_s0[tk]; s <= ct_s1[tk]; ++s) stn_w[s] += w / n_stn;
            } else if (j == 8) {
                stn_w[17] += w * 0.25; stn_w[18] += w * 0.40; stn_w[19] += w * 0.35;
            } else {
                stn_w[0] += w * 0.40; stn_w[1] += w * 0.35; stn_w[2] += w * 0.25;
            }
        }

        out.disp_mt = sum_w;
        HydroRow hr = interp_hydro(sum_w);
        out.tm = hr.draft; out.tpc = hr.tpc; out.mctc = hr.mctc;
        out.lcb = hr.lcb; out.lcf = hr.lcf; out.kmt = hr.kmt;
        out.lcg = sum_mx / sum_w; out.tcg = sum_my / sum_w;
        out.kg_solid = sum_mz / sum_w; out.fsc = sum_fsm / sum_w;
        out.gm_fluid = std::max(0.20, out.kmt - out.kg_solid - out.fsc);
        out.trim_m = (sum_w * (out.lcg - out.lcb)) / (out.mctc * 100.0);
        out.tf = out.tm + out.trim_m * ((LBP / 2.0 - out.lcf) / LBP);
        out.ta = out.tm - out.trim_m * ((LBP / 2.0 + out.lcf) / LBP);
        out.heel_deg = std::clamp(std::atan2(out.tcg, out.gm_fluid) * (180.0 / M_PI), -18.0, 18.0);

        double w_sum = 0.0, b_sum = 0.0, m_err = 0.0, x2_sum = 0.0;
        for (int s = 0; s < 20; ++s) w_sum += stn_w[s];
        for (int s = 0; s < 20; ++s) stn_w[s] *= (sum_w / w_sum);

        static constexpr std::array<double, 20> bonjean = {
            0.38, 0.62, 0.88, 1.06, 1.15, 1.18, 1.19, 1.19, 1.19, 1.19,
            1.19, 1.19, 1.19, 1.18, 1.14, 1.04, 0.86, 0.60, 0.36, 0.18
        };
        std::array<double, 20> stn_b{};
        for (int s = 0; s < 20; ++s) {
            stn_b[s] = bonjean[s] * std::max(0.2, 1.0 + 0.035 * out.trim_m * ((s - 9.5) / 9.5));
            b_sum += stn_b[s];
        }
        for (int s = 0; s < 20; ++s) stn_b[s] *= (sum_w / b_sum);
        for (int s = 0; s < 20; ++s) {
            double x = s - 9.5;
            m_err += (stn_b[s] - stn_w[s]) * x;
            x2_sum += bonjean[s] * x * x;
        }
        for (int s = 0; s < 20; ++s) stn_b[s] -= bonjean[s] * (s - 9.5) * (m_err / x2_sum);

        double sf_run = 0.0, bm_run = 0.0, peak_sf = 0.0, peak_bm = 0.0, dx = LBP / 20.0;
        for (int s = 0; s < 20; ++s) {
            double net = stn_b[s] - stn_w[s];
            sf_run += net; bm_run += (sf_run - 0.5 * net) * dx;
            out.sf_stations[s] = (sf_run / 11500.0) * 100.0;
            out.bm_stations[s] = (bm_run / 245000.0) * 100.0;
            if (std::abs(out.sf_stations[s]) > std::abs(peak_sf)) peak_sf = out.sf_stations[s];
            if (std::abs(out.bm_stations[s]) > std::abs(peak_bm)) peak_bm = out.bm_stations[s];
        }
        out.sf_stations[19] = 0.0; out.bm_stations[19] = 0.0;
        out.max_sf_pct = std::clamp(std::abs(peak_sf), 0.0, 150.0);
        out.max_bm_pct = std::clamp(peak_bm, -150.0, 150.0);
        out.harbor_sf_pct = out.max_sf_pct * 0.76; out.harbor_bm_pct = out.max_bm_pct * 0.76;
    }
};
