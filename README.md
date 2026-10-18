# 3D VLGC Cargo & Ballast Operations Simulator (v1.0)

A real-time Naval Architecture, IGC Code Thermodynamics, and Cargo Control Room (CCR) Simulator for an **84,000 m3 Fully Refrigerated Very Large Gas Carrier (VLGC)**, built in **C++20** and **Three.js / WebGL**.

## Key Engineering Systems Modeled
* **Vessel & Tank Geometry**: 4 independent IMO Type A prismatic cargo tanks (Port/Starboard split with lower centerline bulkhead valves), 4 Vent Mast Risers, and L-shaped Water Ballast Tanks (WBT 1-4 P/S, FPK, APT) modeling Double Bottom, Narrow Middle Trunking, and Topside Wing free-surface transitions.
* **Three-Grade IGC Thermodynamics (thermo.hpp)**: Commercial Propane (C3), n-Butane (C4), and Anhydrous Ammonia (NH3) with Antoine vapor-liquid equilibrium, ASTM liquid/vapor densities, 950-metric-ton tank steel thermal mass, and Ch. 15 Maximum Filling Limits (98% FL).
* **Dual-System Cargo Plant (piping_pumps.hpp)**:
  * **8 Deepwell Cargo Pumps**: Centrifugal H-Q curves, discharge throttling valves, motor amperage, NPSH cavitation, and dry-run/overload auto-trips.
  * **4 Cargo Compressors**: Reliquefaction and Hot Gas modes across System 1 & System 2 headers.
  * **24 Condensate Return Valves**: Top Spray, Bottom Spray, and Bottom Sparge physics per tank half.
  * **Manifold & Relief Network**: Shore Chicksan arm connection, ESD/Gate valves, line PT/TT sensors, Pipeline Thermal Relief Valves (TRV > 10 bar g), and Tank MARVS (> 250 mbar g) venting to Mast Risers.
* **Safety & Loadicator (safety_fgd.hpp, hydro_stress.hpp)**: Fixed Gas Detection (FGD) across Hold Spaces 1-4, Compressor Room, and Electric Motor Room, full ESD-1 shutdown matrix, and 20-Station Still Water Shear Force (SWSF) & Bending Moment (SWBM) calculations.
* **12 Operational Presets (presets.hpp)**: Full Pre-Drydocking (Stripping, Warming-Up, Inerting, Aerating), Post-Drydocking (Drying, Gassing-Up, Cool-Down), Loading/Discharging, and Emergency Drills.

## Build & Run (Linux / Android Termux)
    clang++ -std=c++20 -O2 server.cpp -o vlgc_sim
    ./vlgc_sim

Open [http://127.0.0.1:8080](http://127.0.0.1:8080) in your browser.
