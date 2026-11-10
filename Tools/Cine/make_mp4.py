# Cine: MRQ PNG frames + ambience mix -> MP4 (H.264 CRF 16, yuv420p, AAC).
# Usage: python make_mp4.py Cinematic|FPV [Final|Preview]
#   Final   -> D:\Projek UE\ZooCinematic\<Film>\ZooCine_<Film>.mp4 (only this file is written there)
#   Preview -> Saved\Cine\Preview_<Film>.mp4 (rendered every 10th frame, played at fps/10)
# Audio: CityPark ambience exported by build_cine.py (Saved\Cine\Audio). FPV adds wind that follows the drone speed and
# whooshes at close passes, synthesized here from plan_fpv.json + probe_paths_out.json (numpy/scipy noise).
import glob, json, os, re, subprocess, sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter, sosfilt

ROOT = r"D:\Projek UE\ZooEka2VR"
SAVED = os.path.join(ROOT, "Saved", "Cine")
FFMPEG = r"C:\ffmpeg\bin\ffmpeg.exe"
FILM = sys.argv[1] if len(sys.argv) > 1 else "Cinematic"
TAG = sys.argv[2] if len(sys.argv) > 2 else "Final"
SEQ = "LS_Zoo_Cine_" + FILM
FRAMES = os.path.join(SAVED, "Render", TAG, SEQ)
PLAN = json.load(open(os.path.join(SAVED, "plan_%s.json" % FILM.lower())))
FPS = PLAN["fps"]; LENGTH = PLAN["frames"] / FPS
STEP = 10 if TAG == "Preview" else 1
OUT = os.path.join(r"D:\Projek UE\ZooCinematic", FILM, "ZooCine_%s.mp4" % FILM) if TAG == "Final" else os.path.join(SAVED, "Preview_%s.mp4" % FILM)
MIX = os.path.join(SAVED, "mix_%s.wav" % FILM)
SR = 48000

def wav(prefix):
    m = sorted(glob.glob(os.path.join(SAVED, "Audio", "**", prefix + "*.[wW][aA][vV]"), recursive=True))
    if not m: raise SystemExit("missing wav " + prefix + " (run build_cine.py)")
    return m[0]

def wind_track(path):
    """Wind noise whose level and brightness follow the FPV speed, plus a whoosh at each close pass."""
    rng = np.random.default_rng(7)
    n = int(LENGTH * SR)
    speed = np.interp(np.arange(n) / SR, np.arange(len(PLAN["speed"])) / FPS, PLAN["speed"])        # m/s
    white = rng.standard_normal((2, n))
    # two noise colours: low rumble (brown-ish) and a brighter airy band, mixed by speed
    rumble = sosfilt(butter(2, 220, "low", fs=SR, output="sos"), white, axis=1)
    air = sosfilt(butter(2, [700, 4200], "band", fs=SR, output="sos"), white, axis=1)
    s = np.clip(speed / 18.0, 0, 1.2)
    gust = 1.0 + 0.18 * np.sin(2 * np.pi * 0.31 * np.arange(n) / SR) + 0.12 * np.sin(2 * np.pi * 0.83 * np.arange(n) / SR + 1.3)
    out = (rumble * (0.9 * s ** 1.4) + air * (0.22 * s ** 2.2)) * gust
    # whoosh: band noise burst centred on frames where the camera passes close to geometry at speed
    probe = os.path.join(SAVED, "probe_paths_out.json")
    if os.path.exists(probe):
        d = np.array([r["d"] for r in json.load(open(probe))["paths"]["B_FPV"]], dtype=float)
        sp = np.array(PLAN["speed"][:len(d)])
        near = (d < 260) & (sp > 9)
        events, last = [], -1e9
        for i in np.nonzero(near)[0]:
            t = i / FPS
            if t - last > 0.9: events.append((t, float(sp[i]), float(d[i]))); last = t
        sos = butter(2, [400, 2600], "band", fs=SR, output="sos")
        for t, v, dist in events:
            a, b = int(max(0, (t - 0.45) * SR)), int(min(n, (t + 0.55) * SR))
            seg = sosfilt(sos, rng.standard_normal((2, b - a)), axis=1)
            x = np.linspace(-1, 1, b - a)
            env = np.exp(-((x + 0.1) / 0.32) ** 2) * min(v / 16.0, 1.2) * (1.0 - min(dist, 260) / 400.0)
            out[:, a:b] += seg * env * 1.6
        print("whoosh events:", len(events))
    out *= 0.5 / max(np.abs(out).max(), 1e-6)
    wavfile.write(path, SR, out.T.astype(np.float32))

def build_mix():
    forest, spring, water = wav("S_231537"), wav("S_345852"), wav("S_469009")
    L = LENGTH
    if FILM == "Cinematic":
        # forest birds bed; spring birds over the habitat shots; water at the canal and the pond (windows from the shot times)
        sh = {x["name"][:2]: x for x in PLAN["shots"]}
        fc = ("[0:a]aloop=loop=-1:size=2e9,atrim=0:{L},volume=24dB,volume=0.55[f];"
              "[1:a]aloop=loop=-1:size=2e9,atrim=0:{L},volume=24dB,volume='0.38*between(t,{b0},{b1})':eval=frame[s];"
              "[2:a]aloop=loop=-1:size=2e9,atrim=0:{L},volume=18dB,volume='0.55*(between(t,{c0},{c1})+between(t,{p0},{p1}))':eval=frame,lowpass=f=6000[w];"
              "[f][s][w]amix=inputs=3:duration=first:normalize=0,loudnorm=I=-21:TP=-2:LRA=11,afade=t=in:st=0:d=1.5,afade=t=out:st={fo}:d=3,alimiter=limit=0.9[a]").format(
              L=L, fo=L - 3, b0=sh["06"]["t0"] - 2, b1=sh["11"]["t1"], c0=sh["05"]["t0"] - 1, c1=sh["05"]["t1"] + 1, p0=sh["12"]["t0"] - 1, p1=sh["12"]["t1"] + 1)
        cmd = [FFMPEG, "-y", "-i", forest, "-i", spring, "-i", water, "-filter_complex", fc, "-map", "[a]", "-ar", str(SR), "-ac", "2", MIX]
    else:
        wind = os.path.join(SAVED, "wind_FPV.wav"); wind_track(wind)
        ev = PLAN["named"]
        fc = ("[0:a]aloop=loop=-1:size=2e9,atrim=0:{L},volume=24dB,volume=0.5[f];"
              "[1:a]aloop=loop=-1:size=2e9,atrim=0:{L},volume=18dB,volume='0.35*between(t,{c0},{c1})':eval=frame[w];"
              "[2:a]atrim=0:{L},volume=0.8[n];"
              "[f][w][n]amix=inputs=3:duration=first:normalize=0,loudnorm=I=-18:TP=-1.5:LRA=14,afade=t=in:st=0:d=0.8,afade=t=out:st={fo}:d=1.5,alimiter=limit=0.9[a]").format(
              L=L, c0=ev["canal"] - 1.5, c1=ev["holo_w"] - 1.0, fo=L - 1.5)
        cmd = [FFMPEG, "-y", "-i", forest, "-i", water, "-i", wind, "-filter_complex", fc, "-map", "[a]", "-ar", str(SR), "-ac", "2", MIX]
    subprocess.run(cmd, check=True)
    print("mix ->", MIX)

def encode():
    frames = sorted(glob.glob(os.path.join(FRAMES, SEQ + ".*.png")))
    frames = [f for f in frames if int(re.search(r"\.(\d+)\.png$", f).group(1)) < PLAN["frames"]]   # stale frames of a longer earlier render
    if not frames: raise SystemExit("no frames in " + FRAMES)
    first = int(re.search(r"\.(\d+)\.png$", frames[0]).group(1))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    if TAG == "Final":
        cmd = [FFMPEG, "-y", "-framerate", str(FPS), "-start_number", str(first), "-i", os.path.join(FRAMES, SEQ + ".%04d.png"), "-i", MIX,
               "-map", "0:v", "-map", "1:a", "-c:a", "aac", "-b:a", "192k", "-shortest"]
    else:   # every 10th frame exists: glob input, played at fps/10
        cmd = [FFMPEG, "-y", "-framerate", str(FPS / STEP), "-pattern_type", "sequence", "-start_number", str(first), "-i", os.path.join(FRAMES, SEQ + ".%04d.png")]
        lst = os.path.join(SAVED, "preview_%s.txt" % FILM)
        open(lst, "w").write("".join("file '%s'\nduration %f\n" % (f.replace("\\", "/"), STEP / FPS) for f in frames))
        cmd = [FFMPEG, "-y", "-f", "concat", "-safe", "0", "-i", lst]
    cmd += ["-c:v", "libx264", "-preset", "slow", "-crf", "16", "-pix_fmt", "yuv420p", "-r", str(FPS), "-movflags", "+faststart", OUT]
    subprocess.run(cmd, check=True)
    print("%d frames -> %s" % (len(frames), OUT))

FPV_LEGS = [  # (from event, to event, description)
    ("start", "canopy", "Mulai tinggi di atas hutan barat daya, turun ke arah pucuk kanopi"),
    ("canopy", "dive_top", "Skim kanopi menuju jalan masuk"),
    ("dive_top", "road", "Dive curam dari atas pepohonan ke jalan masuk (nose-down, ~18 m/s)"),
    ("road", "tusks", "Terbang rendah di jalan, masuk lewat celah gerbang gading"),
    ("tusks", "papan", "Belok mengitari sisi utara pendopo, lewat bawah papan Taman Safari"),
    ("papan", "fence", "Menyusuri jalur pengunjung, lompat pagar"),
    ("fence", "orbit", "Skim kanal ke barat, naik menyusuri sisi barat habitat jerapah dan mengitari pohon oak"),
    ("orbit", "orbit_end", "Orbit cepat 2,25 putaran mengelilingi jerapah (BP_Giraffe_2), roll ke dalam"),
    ("orbit_end", "pond_in", "Lewat dekat hologram jerapah saat materialize, kartu info AR; lalu sisi timur habitat dan kanal"),
    ("pond_in", "holo_w", "Rendah di atas kolam: kejar Duma dari belakang lalu menyalip (trail pita)"),
    ("holo_w", "gazebo", "Menyisir sisi utara kandang wombat saat hologram wombat materialize, memutar ke timur lalu melintas di atas hologram dan kartu info"),
    ("gazebo", "south_forest", "Menyapu ke barat melewati gazebo rest area dan halaman rest area"),
    ("south_forest", "end", "Pull-up tinggi dari tenggara gerbang, reveal gerbang gading, pendopo dan taman (fade out)"),
]

def shotlist():
    path = OUT.replace(".mp4", "_shotlist.md")
    def tc(t): return "%d:%05.2f" % (int(t // 60), t % 60)
    lines = ["# ZooCine %s - shot list" % FILM, "",
             "Sumber: `/Game/Cine/LS_Zoo_Cine_%s` di `/Game/Zoo/Maps/L_Zoo_Cine` (project ZooEka2VR). 1920x1080, %d fps, %.1f s, H.264 CRF 16 yuv420p + AAC." % (FILM, FPS, LENGTH), ""]
    if FILM == "Cinematic":
        lines += ["| # | Waktu | Shot | Lensa | Isi |", "|---|---|---|---|---|"]
        for i, s in enumerate(PLAN["shots"], 1):
            lines.append("| %d | %s - %s | %s | %d mm f/%g | %s |" % (i, tc(s["t0"]), tc(s["t1"]), s["name"], s["focal"], s["fstop"], s["desc"]))
    else:
        ev = PLAN["named"]; sp = PLAN["speed"]
        lines += ["Satu take kontinu, lensa 15 mm tanpa DoF, roll mengikuti percepatan lateral (maks 40 derajat), nose-down saat akselerasi, micro-shake, motion blur 0.3.", "",
                  "| # | Waktu | Gerak | Kecepatan (m/s) |", "|---|---|---|---|"]
        for i, (a, b, desc) in enumerate(FPV_LEGS, 1):
            seg = sp[int(ev[a] * FPS):max(int(ev[a] * FPS) + 1, int(ev[b] * FPS))]
            lines.append("| %d | %s - %s | %s | %.0f-%.0f |" % (i, tc(ev[a]), tc(ev[b]), desc, min(seg), max(seg)))
    open(path, "w", encoding="utf-8").write(chr(10).join(lines) + chr(10))
    print("shot list ->", path)

if __name__ == "__main__":
    if TAG == "Final": build_mix()
    encode()
    if TAG == "Final": shotlist()
