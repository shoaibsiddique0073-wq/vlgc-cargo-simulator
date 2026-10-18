#pragma once
#include <array>
#include <cmath>
#include <algorithm>

struct HydroRow {
    double draft, disp_mt, tpc, mctc, lcb, lcf, kmt;
};

// 84,000 m3 VLGC Hydrostatic Table (LBP = 222.0m, Beam = 36.6m, Depth = 22.2m)
constexpr std::array<HydroRow, 10> HYDRO_TABLE = {{
    {4.0,  23400.0, 62.8,  790.0,  3.85,  0.40, 25.40},
    {5.0,  29750.0, 64.2,  835.0,  3.45, -0.25, 22.10},
    {6.0,  36250.0, 65.5,  882.0,  3.02, -0.95, 19.85},
    {7.0,  42880.0, 66.8,  930.0,  2.55, -1.70, 18.25},
    {8.0,  49630.0, 68.0,  980.0,  2.05, -2.45, 17.15},
    {9.0,  56500.0, 69.2, 1032.0,  1.52, -3.20, 16.45},
    {10.0, 63480.0, 70.3, 1086.0,  0.98, -3.95, 16.05},
    {11.0, 70570.0, 71.4, 1142.0,  0.42, -4.65, 15.88},
    {12.0, 77760.0, 72.4, 1200.0, -0.15, -5.30, 15.85},
    {13.5, 88720.0, 73.8, 1290.0, -0.98, -6.15, 16.02}
}};

inline HydroRow interp_hydro(double disp) {
    if (disp <= HYDRO_TABLE.front().disp_mt) return HYDRO_TABLE.front();
    if (disp >= HYDRO_TABLE.back().disp_mt)  return HYDRO_TABLE.back();
    for (size_t i = 0; i < HYDRO_TABLE.size() - 1; ++i) {
        const auto& a = HYDRO_TABLE[i];
        const auto& b = HYDRO_TABLE[i + 1];
        if (disp >= a.disp_mt && disp <= b.disp_mt) {
            double f = (disp - a.disp_mt) / (b.disp_mt - a.disp_mt);
            return {
                a.draft + f * (b.draft - a.draft), disp,
                a.tpc   + f * (b.tpc   - a.tpc),
                a.mctc  + f * (b.mctc  - a.mctc),
                a.lcb   + f * (b.lcb   - a.lcb),
                a.lcf   + f * (b.lcf   - a.lcf),
                a.kmt   + f * (b.kmt   - a.kmt)
            };
        }
    }
    return HYDRO_TABLE.back();
}

// Cargo Tank Geometry (4 Tanks, split P/S by Centerline Bulkhead: 10,500 m3 per half)
struct CargoTankGeom {
    double max_half_m3;
    double lcg;
    double half_beam;
};

constexpr std::array<CargoTankGeom, 4> CT_GEOM = {{
    {10500.0,  64.2, 14.2}, // CT1 (Forward, slightly narrower)
    {10500.0,  22.8, 16.4}, // CT2
    {10500.0, -19.5, 16.4}, // CT3
    {10500.0, -61.0, 15.8}  // CT4 (Aft)
}};

inline double ct_vcg(double fill_pct) {
    // Prismatic octagonal VCG (bottom hopper -> parallel body -> top sloped chamfer)
    return 1.65 + (fill_pct / 100.0) * 10.35 - 0.35 * std::sin((fill_pct / 100.0) * M_PI);
}

inline double ct_half_inertia(int idx, double fill_pct) {
    if (fill_pct < 1.5 || fill_pct > 96.5) return 45.0;
    double b = CT_GEOM[idx].half_beam;
    // Top chamfer & bottom hopper reduce free surface breadth near top/bottom
    double shape_factor = std::sin((fill_pct / 98.0) * M_PI);
    return (36.5 * std::pow(b * (0.65 + 0.35 * shape_factor), 3)) / 12.0;
}

// Ballast Tank Geometry: WBT 1P/S-4P/S (Bottom -> Narrow Trunking -> Topside) + FPK + APT
struct BallastTankGeom {
    const char* name;
    double max_m3, lcg, side_sign; // side_sign: -1 = Port, +1 = Stbd, 0 = Center
    bool is_l_shaped;
};

constexpr std::array<BallastTankGeom, 10> WBT_GEOM = {{
    {"WBT1P", 2400.0,  62.0, -1.0, true},  {"WBT1S", 2400.0,  62.0,  1.0, true},
    {"WBT2P", 2800.0,  21.0, -1.0, true},  {"WBT2S", 2800.0,  21.0,  1.0, true},
    {"WBT3P", 2800.0, -20.0, -1.0, true},  {"WBT3S", 2800.0, -20.0,  1.0, true},
    {"WBT4P", 2500.0, -60.0, -1.0, true},  {"WBT4S", 2500.0, -60.0,  1.0, true},
    {"FPK",   1850.0, 101.5,  0.0, false}, {"APT",   2100.0, -103.0, 0.0, false}
}};

// Returns {VCG, TCG, FreeSurfaceInertia_m4, ZoneID (0=Bottom, 1=Trunking, 2=Topside)}
inline std::array<double, 4> wbt_properties(int idx, double fill_pct) {
    const auto& g = WBT_GEOM[idx];
    if (!g.is_l_shaped) {
        double vcg = 4.5 + (fill_pct / 100.0) * 7.5;
        double it = (fill_pct > 2.0 && fill_pct < 96.0) ? 420.0 : 25.0;
        return {vcg, 0.0, it, 0.0};
    }
    if (fill_pct <= 45.0) {
        // Zone 0: Double Bottom / Lower Hopper (Wide breadth -> high FSM, low VCG)
        double f = fill_pct / 45.0;
        double it = (fill_pct > 1.5) ? 1150.0 * std::sin(f * M_PI * 0.85) : 20.0;
        return {1.15 + f * 1.65, g.side_sign * 8.4, it, 0.0};
    } else if (fill_pct <= 70.0) {
        // Zone 1: Narrow Middle Trunking (Tiny waterplane breadth -> minimal FSM!)
        double f = (fill_pct - 45.0) / 25.0;
        return {3.20 + f * 8.40, g.side_sign * 16.8, 85.0, 1.0};
    } else {
        // Zone 2: Upper Topside Wing Tank (High VCG, moderate FSM until pressed full)
        double f = (fill_pct - 70.0) / 30.0;
        double it = (fill_pct < 97.0) ? 680.0 * (1.0 - f * 0.6) : 15.0;
        return {12.20 + f * 5.80, g.side_sign * 14.5, it, 2.0};
    }
}
