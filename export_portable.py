#!/usr/bin/env python3
"""Bundles index.html, ui_3d.js, and ui_mimics.js into a single shareable HTML file for WhatsApp/GitHub."""
with open("index.html") as f: html = f.read()
with open("ui_3d.js") as f: js3d = f.read()
with open("ui_mimics.js") as f: jsmim = f.read()

bundle = html.replace('<script src="/ui_3d.js"></script>', f'<script>\n{js3d}\n</script>')
bundle = bundle.replace('<script src="/ui_mimics.js"></script>', f'<script>\n{jsmim}\n</script>')

out_path = "/sdcard/Download/vlgc_simulator_ui.html"
with open("vlgc_portable.html", "w") as f: f.write(bundle)
try:
    with open(out_path, "w") as f: f.write(bundle)
    print(f"Exported portable bundle to ~/vlgc_sim/vlgc_portable.html AND {out_path}")
except Exception:
    print("Exported portable bundle to ~/vlgc_sim/vlgc_portable.html")
