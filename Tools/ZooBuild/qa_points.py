"""Writes Saved/ZooBuild/qa_points.json: path samples (every 2 m, with expected ground z) and
barrier probes (path point -> animal anchor) for the in-editor walkability/barrier check."""
import json
import math
import os

from zoo_common import L, OUT_DIR, ground

samples = []
for path in L["paths"]:
    pts = path["pts"]
    for a, b in zip(pts[:-1], pts[1:]):
        d = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(d / 2.0))
        for k in range(n):
            t = k / n
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            samples.append({"path": path["name"], "x": round(x, 2), "y": round(y, 2), "z": round(ground(x, y), 3)})

anchors = dict(L["giraffe"]["anchors"])
anchors.update(L["wombat"]["anchors"])
probes = []
for s in samples:
    for name, (ax, ay) in anchors.items():
        dist = math.hypot(ax - s["x"], ay - s["y"])
        if dist < 25.0:
            probes.append({"from": [s["x"], s["y"], s["z"]], "to": [ax, ay, ground(ax, ay)], "anchor": name, "path": s["path"]})

out = {"samples": samples, "probes": probes}
with open(os.path.join(OUT_DIR, "qa_points.json"), "w") as f:
    json.dump(out, f, separators=(",", ":"))
print("samples:", len(samples), "barrier probes:", len(probes), "bytes:", os.path.getsize(os.path.join(OUT_DIR, "qa_points.json")))
