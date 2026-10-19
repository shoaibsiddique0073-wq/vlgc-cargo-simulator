#!/usr/bin/env python3
with open("index.html") as f: html = f.read()
with open("standalone_sim.js") as f: jssim = f.read()
with open("ui_3d.js") as f: js3d = f.read()
with open("ui_mimics.js") as f: jsmim = f.read()

bundle = html.replace('<script src="standalone_sim.js"></script>', f'<script>\n{jssim}\n</script>')
bundle = bundle.replace('<script src="ui_3d.js"></script>', f'<script>\n{js3d}\n</script>')
bundle = bundle.replace('<script src="ui_mimics.js"></script>', f'<script>\n{jsmim}\n</script>')

for path in ["vlgc_portable.html", "/sdcard/Download/vlgc_simulator_tap_to_run.html"]:
    try:
        with open(path, "w") as f: f.write(bundle)
        print("Saved:", path)
    except Exception:
        pass
