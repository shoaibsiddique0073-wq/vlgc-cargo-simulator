#include "vlgc_engine.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>

VLGCEngine sim;
auto last_tick = std::chrono::steady_clock::now();

std::string load_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

void handle_cmd(const std::string& req) {
    auto pos = req.find("GET /state?");
    if (pos == std::string::npos) return;
    const char* q = req.c_str() + pos + 11;
    int a = 0, b = 0, c = 0; double v = 0.0;

    if (std::sscanf(q, "ct=%d&side=%d&v=%lf", &a, &b, &v) == 3 && a >= 0 && a < 4) {
        double max_fl = sim.th[a].max_fill_limit();
        sim.ct_m3[a][b & 1] = (std::clamp(v, 0.0, max_fl) / 100.0) * CT_GEOM[a].max_half_m3;
    } else if (std::sscanf(q, "bhd=%d", &a) == 1 && a >= 0 && a < 4) {
        sim.bhd_open[a] = !sim.bhd_open[a];
    } else if (std::sscanf(q, "pstart=%d&side=%d", &a, &b) == 2 && a >= 0 && a < 4) {
        if (!sim.fgd.esd1_active) {
            auto& p = sim.dwp[a][b & 1];
            p.running = !p.running;
            if (p.running) p.trip_code = 0;
        }
    } else if (std::sscanf(q, "pthr=%d&side=%d&v=%lf", &a, &b, &v) == 3 && a >= 0 && a < 4) {
        sim.dwp[a][b & 1].throttle_pct = std::clamp(v, 0.0, 100.0);
    } else if (std::sscanf(q, "fvlv=%d", &a) == 1 && a >= 0 && a < 4) {
        if (!sim.fgd.esd1_active) sim.man.fill_vlv[a] = !sim.man.fill_vlv[a];
    } else if (std::sscanf(q, "vsuc=%d", &a) == 1 && a >= 0 && a < 4) {
        sim.th[a].vap_suc_vlv = !sim.th[a].vap_suc_vlv;
    } else if (std::sscanf(q, "cin=%d", &a) == 1 && a >= 0 && a < 4) {
        sim.th[a].cond_in_vlv = !sim.th[a].cond_in_vlv;
    } else if (std::sscanf(q, "sconn=%d", &a) == 1 && (a == 0 || a == 1)) {
        sim.man.shore_conn[a] = !sim.man.shore_conn[a];
    } else if (std::sscanf(q, "mesd=%d", &a) == 1 && (a == 0 || a == 1)) {
        if (!sim.fgd.esd1_active) sim.man.esd_vlv[a] = !sim.man.esd_vlv[a];
    } else if (std::sscanf(q, "mgate=%d", &a) == 1 && (a == 0 || a == 1)) {
        sim.man.gate_vlv[a] = !sim.man.gate_vlv[a];
    } else if (std::sscanf(q, "mop=%d&m=%d", &a, &b) == 2 && (a == 0 || a == 1)) {
        if (!sim.fgd.esd1_active) sim.man.op_mode[a] = std::clamp(b, 0, 2);
    } else if (std::sscanf(q, "sbar=%d&v=%lf", &a, &v) == 2 && (a == 0 || a == 1)) {
        sim.man.shore_bar[a] = std::clamp(v, 0.5, 12.0);
    } else if (std::sscanf(q, "htr=%d", &a) == 1) {
        sim.man.heater_on = !sim.man.heater_on;
    } else if (std::sscanf(q, "bst=%d", &a) == 1) {
        sim.man.booster_on = !sim.man.booster_on;
    } else if (std::sscanf(q, "vret=%d&side=%d&br=%d", &a, &b, &c) == 3 && a >= 0 && a < 4) {
        sim.th[a].v_ret[b & 1][std::clamp(c, 0, 2)] = !sim.th[a].v_ret[b & 1][std::clamp(c, 0, 2)];
    } else if (std::sscanf(q, "grade=%d&g=%d", &a, &b) == 2 && a >= 0 && a < 4) {
        sim.th[a].grade_id = std::clamp(b, 0, 2);
        double def_t = (b == 0) ? -41.5 : (b == 1 ? -0.3 : -32.8);
        sim.th[a].t_liquid = def_t; sim.th[a].t_bot_shell = def_t + 0.3;
        sim.th[a].t_vapor  = def_t + 1.5; sim.th[a].t_top_shell = def_t + 2.5;
        sim.th[a].press_mbarg = sim.th[a].sat_press_mbarg(def_t);
    } else if (std::sscanf(q, "tshell=%d&v=%lf", &a, &v) == 2 && a >= 0 && a < 4) {
        sim.th[a].t_top_shell = v; sim.th[a].t_bot_shell = v - 1.5; sim.th[a].t_vapor = v - 0.5;
    } else if (std::sscanf(q, "sys=%d", &a) == 1 && a >= 0 && a < 4) {
        sim.th[a].sys_group = 1 - sim.th[a].sys_group;
    } else if (std::sscanf(q, "prg=%d&m=%d", &a, &b) == 2 && a >= 0 && a < 4) {
        sim.th[a].purge_mode = std::clamp(b, 0, 3);
    } else if (std::sscanf(q, "comp=%d&m=%d", &a, &b) == 2 && a >= 0 && a < 4) {
        if (!sim.fgd.esd1_active) sim.comp[a].mode = std::clamp(b, 0, 2);
    } else if (std::sscanf(q, "csys=%d", &a) == 1 && a >= 0 && a < 4) {
        sim.comp[a].sys_id = 1 - sim.comp[a].sys_id;
    } else if (std::sscanf(q, "xo=%d", &a) == 1) {
        if (a == 0) sim.vap_xover = !sim.vap_xover;
        if (a == 1) sim.cond_xover = !sim.cond_xover;
        if (a == 2) sim.liq_xover = !sim.liq_xover;
    } else if (std::sscanf(q, "leak=%d", &a) == 1 && a >= 0 && a < 6) {
        sim.fgd.zones[a].leak_sim = !sim.fgd.zones[a].leak_sim;
    } else if (std::sscanf(q, "esd=%d", &a) == 1) {
        if (a == 1) sim.fgd.trigger_manual_esd();
        else if (a == 0) sim.fgd.reset_esd();
        else if (a == 2) sim.fgd.comp_vent_on = !sim.fgd.comp_vent_on;
        else if (a == 3) sim.fgd.motor_rm_dp_pa = (sim.fgd.motor_rm_dp_pa > 20.0) ? 8.0 : 55.0;
        else if (a == 4) sim.fgd.fire_loop_ok = false;
    } else if (std::sscanf(q, "preset=%d", &a) == 1) {
        if (a == -1) sim.save_custom_preset();
        else sim.apply_preset(a);
    } else if (std::sscanf(q, "wbt=%d&v=%lf", &a, &v) == 2 && a >= 0 && a < 10) {
        sim.wbt_m3[a] = (std::clamp(v, 0.0, 100.0) / 100.0) * WBT_GEOM[a].max_m3;
    } else if (std::sscanf(q, "bp=%d&m=%d", &a, &b) == 2 && (a == 0 || a == 1)) {
        sim.bp_mode[a] = std::clamp(b, 0, 4);
    } else if (std::sscanf(q, "wtgt=%d", &a) == 1) {
        sim.wbt_target = std::clamp(a, 0, 4);
    } else if (std::sscanf(q, "spd=%lf", &v) == 1) {
        sim.time_speed = std::clamp(v, 0.0, 300.0);
    }
}

int main() {
    sim.step(0.0);
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{AF_INET, htons(8080), {INADDR_ANY}, {}};
    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, 12);
    std::cout << "Phase 6 Complete VLGC Simulator live at http://127.0.0.1:8080\n";

    char buf[2048];
    while (true) {
        int cli = accept(srv, nullptr, nullptr);
        std::memset(buf, 0, sizeof(buf));
        read(cli, buf, sizeof(buf) - 1);
        std::string req(buf), body, type = "application/json";

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_tick).count();
        last_tick = now;

        if (req.find("GET /state") != std::string::npos) {
            handle_cmd(req);
            sim.step(std::min(dt, 1.0));
            body = sim.to_json();
        } else if (req.find("GET /ui_3d.js") != std::string::npos) {
            type = "application/javascript"; body = load_file("ui_3d.js");
        } else if (req.find("GET /ui_mimics.js") != std::string::npos) {
            type = "application/javascript"; body = load_file("ui_mimics.js");
        } else if (req.find("GET /assets/") != std::string::npos) {
            auto s = req.find("/assets/") + 1;
            auto e = req.find(" ", s);
            std::string path = req.substr(s, e - s);
            if (path.find(".ogg") != std::string::npos) type = "audio/ogg";
            else if (path.find(".ttf") != std::string::npos) type = "font/ttf";
            else if (path.find(".gltf") != std::string::npos) type = "model/gltf+json";
            else type = "text/plain";
            body = load_file(path);
        } else {
            type = "text/html"; body = load_file("index.html");
        }
        std::string res = "HTTP/1.1 200 OK\r\nContent-Type: " + type +
                          "\r\nContent-Length: " + std::to_string(body.size()) +
                          "\r\nConnection: close\r\n\r\n" + body;
        write(cli, res.c_str(), res.size());
        close(cli);
    }
}
