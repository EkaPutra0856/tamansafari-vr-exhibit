# Cine: contact sheets of rendered frames for review (time stamp + shot name on each tile).
# Usage: python contact_sheet.py Cinematic|FPV [Preview|Final] [every_n_frames] [cols]
import glob, json, os, re, sys
from PIL import Image, ImageDraw, ImageFont

SAVED = r"D:\Projek UE\ZooEka2VR\Saved\Cine"
FILM = sys.argv[1] if len(sys.argv) > 1 else "Cinematic"
TAG = sys.argv[2] if len(sys.argv) > 2 else "Preview"
EVERY = int(sys.argv[3]) if len(sys.argv) > 3 else 30
COLS = int(sys.argv[4]) if len(sys.argv) > 4 else 4
SEQ = "LS_Zoo_Cine_" + FILM
plan = json.load(open(os.path.join(SAVED, "plan_%s.json" % FILM.lower())))
fps = plan["fps"]
shots = [(c["f0"], c["f1"], c["name"]) for c in plan.get("cameras", [])]
def shot_of(f):
    for a, b, n in shots:
        if a <= f < b: return n
    return ""
frames = []
for p in sorted(glob.glob(os.path.join(SAVED, "Render", TAG, SEQ, SEQ + ".*.png"))):
    f = int(re.search(r"\.(\d+)\.png$", p).group(1))
    if f % EVERY == 0 and f < plan["frames"]: frames.append((f, p))   # skip stale frames of a longer earlier render
try: font = ImageFont.truetype("arial.ttf", 18)
except Exception: font = None
TW, TH = 480, 270; per = COLS * 4
out_dir = os.path.join(SAVED, "sheets"); os.makedirs(out_dir, exist_ok=True)
for s in range(0, len(frames), per):
    chunk = frames[s:s + per]; rows = (len(chunk) + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * TW, rows * TH), (0, 0, 0)); d = ImageDraw.Draw(sheet)
    for i, (f, p) in enumerate(chunk):
        im = Image.open(p).convert("RGB").resize((TW, TH)); x, y = (i % COLS) * TW, (i // COLS) * TH
        sheet.paste(im, (x, y))
        label = "%d  %.1fs  %s" % (f, f / fps, shot_of(f))
        d.rectangle([x, y, x + 9 * len(label) + 8, y + 22], fill=(0, 0, 0)); d.text((x + 4, y + 2), label, fill=(255, 255, 0), font=font)
    path = os.path.join(out_dir, "%s_%s_%02d.jpg" % (FILM, TAG, s // per)); sheet.save(path, quality=85); print(path, len(chunk))
