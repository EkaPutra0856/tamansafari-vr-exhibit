# Cine: top-down planning map from grid_out.json (runtime probe) + survey_cine.json, optional camera paths (paths.json).
# Usage: python plot_map.py [paths.json] [out.png] [x0 y0 x1 y1]
import json, sys, os
from PIL import Image, ImageDraw, ImageFont
D = r"D:\Projek UE\ZooEka2VR\Saved\Cine"
pts = json.load(open(os.path.join(D, "grid_in.json")))["ground"]
gnd = json.load(open(os.path.join(D, "grid_out.json")))["ground"]
sur = json.load(open(os.path.join(D, "survey_cine.json")))
paths = json.load(open(sys.argv[1])) if len(sys.argv) > 1 and sys.argv[1] != "-" else {}
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(D, "maps", "map.png")
X0, Y0, X1, Y1 = (-7400, -6600, 4600, 5600) if len(sys.argv) < 7 else [float(a) for a in sys.argv[3:7]]
S = 200.0; PX = 1200.0 / (X1 - X0)
W, H = int((X1 - X0) * PX), int((Y1 - Y0) * PX)
img = Image.new("RGB", (W, H), (0, 0, 0)); d = ImageDraw.Draw(img)
def P(x, y): return ((x - X0) * PX, (Y1 - y) * PX)   # +Y up on the image, +X right
for (x, y), col in zip(pts, gnd):
    land = [z for z, a in col if a == "Landscape"]; lz = land[0] if land else (col[-1][0] if col else 0)
    top = col[0][0] if col else lz; ta = col[0][1] if col else ""
    g = max(0, min(255, int(110 + lz / 6)))
    c = (g, g, g)
    if "Water" in ta: c = (40, 90, 200)
    elif top - lz > 30:
        h = top - lz
        c = (255, max(0, 220 - int(h / 6)), 0) if h < 1200 else (200, 0, 120)
        if any(k in ta for k in ("Tree", "Forest", "Field", "Alder", "Hornbeam", "Willow", "Oak", "Sapling", "Hazel")): c = (0, min(255, 90 + int(h / 10)), 0)
    a, b = P(x - S / 2, y + S / 2); e, f = P(x + S / 2, y - S / 2)
    d.rectangle([a, b, e, f], fill=c)
try: font = ImageFont.truetype("arial.ttf", 11)
except Exception: font = None
for a in sur["actors"]:
    if a["l"].startswith(("ENT_", "AR_", "BP_G", "BP_AR", "REST_Gazebo", "POND_Bridge", "GIR_Zone", "WOM_Zone", "PG_Dest", "GIR_Deck_Floor", "PlayerStart")):
        x, y = a["p"][0], a["p"][1]; px, py = P(x, y)
        d.ellipse([px - 3, py - 3, px + 3, py + 3], outline=(255, 255, 255)); d.text((px + 4, py - 6), a["l"][:18], fill=(255, 255, 255), font=font)
for ex in sur["exhibits"].values():
    pass
cols = [(255, 0, 255), (0, 255, 255), (255, 255, 0), (255, 128, 128), (128, 255, 128), (128, 128, 255), (255, 255, 255)]
for i, (name, pl) in enumerate(paths.items()):
    c = cols[i % len(cols)]
    xy = [P(p[0], p[1]) for p in pl]
    if len(xy) > 1: d.line(xy, fill=c, width=2)
    if xy: d.text((xy[0][0] + 3, xy[0][1] + 3), name[:14], fill=c, font=font)
# grid lines every 1000
for gx in range(int(X0 // 1000 * 1000), int(X1) + 1, 1000):
    a, _ = P(gx, 0); d.line([(a, 0), (a, H)], fill=(60, 60, 60)); d.text((a + 2, 2), str(gx), fill=(200, 200, 200), font=font)
for gy in range(int(Y0 // 1000 * 1000), int(Y1) + 1, 1000):
    _, b = P(0, gy); d.line([(0, b), (W, b)], fill=(60, 60, 60)); d.text((2, b + 2), str(gy), fill=(200, 200, 200), font=font)
img.save(out); print(out, W, H)
