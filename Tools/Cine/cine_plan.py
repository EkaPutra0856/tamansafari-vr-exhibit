# Cine planner (plain Python, no UE): camera, Duma and visitor timelines for the two L_Zoo_Cine films.
#   python cine_plan.py            -> Saved/Cine/plan_cinematic.json, plan_fpv.json, probe_paths_in.json, maps/plan_*.png
#   python cine_plan.py --check    -> also reads probe_paths_out.json (zoo.cine.Probe) and reports clearance problems
# Ground heights come from the runtime probe grid (Saved/Cine/grid_in.json + grid_out.json).
import json, math, os, sys, random
import numpy as np

ROOT = r"D:\Projek UE\ZooEka2VR"
D = os.path.join(ROOT, "Saved", "Cine")

# ------------------------------------------------------------------ ground (landscape) and top surface from the probe grids
class Grid:
    def __init__(self, fin, fout, step):
        pts = json.load(open(os.path.join(D, fin)))["ground"]; cols = json.load(open(os.path.join(D, fout)))["ground"]
        self.x = sorted(set(p[0] for p in pts)); self.y = sorted(set(p[1] for p in pts)); self.s = step
        self.land = np.zeros((len(self.y), len(self.x))); self.top = np.zeros_like(self.land)
        xi = {v: i for i, v in enumerate(self.x)}; yi = {v: i for i, v in enumerate(self.y)}
        for (x, y), col in zip(pts, cols):
            j, i = yi[y], xi[x]
            lnd = [z for z, a in col if a == "Landscape"]
            self.land[j, i] = lnd[0] if lnd else (col[-1][0] if col else 0.0)
            self.top[j, i] = max(col[0][0], self.land[j, i]) if col else self.land[j, i]
    def inside(self, x, y): return self.x[0] <= x <= self.x[-1] and self.y[0] <= y <= self.y[-1]
    def bilinear(self, A, x, y):
        fx = (x - self.x[0]) / self.s; fy = (y - self.y[0]) / self.s
        i = int(np.clip(math.floor(fx), 0, len(self.x) - 2)); j = int(np.clip(math.floor(fy), 0, len(self.y) - 2))
        tx = float(np.clip(fx - i, 0, 1)); ty = float(np.clip(fy - j, 0, 1))
        return float(A[j, i] * (1 - tx) * (1 - ty) + A[j, i + 1] * tx * (1 - ty) + A[j + 1, i] * (1 - tx) * ty + A[j + 1, i + 1] * tx * ty)
    def top_max(self, x, y, r):
        i0 = int(np.clip((x - r - self.x[0]) // self.s, 0, len(self.x) - 1)); i1 = int(np.clip((x + r - self.x[0]) // self.s + 1, 0, len(self.x) - 1))
        j0 = int(np.clip((y - r - self.y[0]) // self.s, 0, len(self.y) - 1)); j1 = int(np.clip((y + r - self.y[0]) // self.s + 1, 0, len(self.y) - 1))
        return float(self.top[j0:j1 + 1, i0:i1 + 1].max())

FINE = Grid("grid_in.json", "grid_out.json", 200.0)
WIDE = Grid("wide_in.json", "wide_out.json", 400.0)

def _grid(x, y): return FINE if FINE.inside(x, y) else WIDE
def land(x, y): g = _grid(x, y); return g.bilinear(g.land, x, y)
def canopy(x, y, r=500.0): return max(FINE.top_max(x, y, r) if FINE.inside(x, y) else -1e9, WIDE.top_max(x, y, r))

def P(p):
    """(x, y, z, mode) -> absolute point. mode 'a' absolute, 'h' above the landscape, 't' above the highest surface within 5 m."""
    x, y, z = p[0], p[1], p[2]
    mode = p[3] if len(p) > 3 else "a"
    base = land(x, y) if mode == "h" else canopy(x, y) if mode == "t" else 0.0
    return np.array([x, y, base + z], dtype=float)

# ------------------------------------------------------------------ curves
def catmull(points, samples=60, alpha=0.5):
    """Centripetal Catmull-Rom through all points -> dense polyline (N x 3)."""
    pts = [np.asarray(p, float) for p in points]
    if len(pts) == 1: return np.array([pts[0]])
    if len(pts) == 2: return np.array([pts[0] + (pts[1] - pts[0]) * t for t in np.linspace(0, 1, samples)])
    ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    out = []
    for k in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[k - 1], ext[k], ext[k + 1], ext[k + 2]
        t0 = 0.0
        t1 = t0 + max(np.linalg.norm(p1 - p0), 1e-3) ** alpha
        t2 = t1 + max(np.linalg.norm(p2 - p1), 1e-3) ** alpha
        t3 = t2 + max(np.linalg.norm(p3 - p2), 1e-3) ** alpha
        for t in np.linspace(t1, t2, samples, endpoint=(k == len(ext) - 3)):
            a1 = (t1 - t) / (t1 - t0) * p0 + (t - t0) / (t1 - t0) * p1
            a2 = (t2 - t) / (t2 - t1) * p1 + (t - t1) / (t2 - t1) * p2
            a3 = (t3 - t) / (t3 - t2) * p2 + (t - t2) / (t3 - t2) * p3
            b1 = (t2 - t) / (t2 - t0) * a1 + (t - t0) / (t2 - t0) * a2
            b2 = (t3 - t) / (t3 - t1) * a2 + (t - t1) / (t3 - t1) * a3
            out.append((t2 - t) / (t2 - t1) * b1 + (t - t1) / (t2 - t1) * b2)
    return np.array(out)

def arclen(poly):
    seg = np.linalg.norm(np.diff(poly, axis=0), axis=1)
    return np.concatenate([[0.0], np.cumsum(seg)])

def at_s(poly, S, s):
    s = float(np.clip(s, 0, S[-1]))
    i = int(np.clip(np.searchsorted(S, s) - 1, 0, len(S) - 2))
    t = (s - S[i]) / max(S[i + 1] - S[i], 1e-9)
    return poly[i] + (poly[i + 1] - poly[i]) * t

def smoothstep(t): return t * t * (3 - 2 * t)
def ease(t, e): return (1 - e) * t + e * smoothstep(t)

def look_rot(cam, tgt):
    d = np.asarray(tgt, float) - np.asarray(cam, float)
    yaw = math.degrees(math.atan2(d[1], d[0])); pitch = math.degrees(math.atan2(d[2], math.hypot(d[0], d[1])))
    return pitch, yaw

def unwrap(vals):
    out = list(vals)
    for i in range(1, len(out)):
        while out[i] - out[i - 1] > 180: out[i] -= 360
        while out[i] - out[i - 1] < -180: out[i] += 360
    return out

def gauss_smooth(a, sigma):
    if sigma <= 0: return np.asarray(a, float)
    a = np.asarray(a, float); r = int(3 * sigma)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2); k /= k.sum()
    pad = np.pad(a, [(r, r)] + [(0, 0)] * (a.ndim - 1), mode="edge")
    if a.ndim == 1: return np.convolve(pad, k, mode="valid")
    return np.stack([np.convolve(pad[:, c], k, mode="valid") for c in range(a.shape[1])], axis=1)

# ------------------------------------------------------------------ speed-profiled path (FPV, Duma)
def timed_path(ctrl, fps, t_start=0.0):
    """ctrl: list of dicts {p: (x,y,z,mode), v: m/s, look?, lw?, name?}. Returns frames list of dicts (t, pos) + named times."""
    pts = [P(c["p"]) for c in ctrl]
    poly = catmull(pts, samples=80); S = arclen(poly)
    # arc length of every control point on the polyline (closest sample)
    sc = []
    for p in pts:
        sc.append(float(S[int(np.argmin(np.linalg.norm(poly - p, axis=1)))]))
    sc = np.maximum.accumulate(np.array(sc))
    vc = np.array([c["v"] * 100.0 for c in ctrl])
    def v_at(s):
        i = int(np.clip(np.searchsorted(sc, s) - 1, 0, len(sc) - 2))
        t = (s - sc[i]) / max(sc[i + 1] - sc[i], 1e-6)
        return vc[i] + (vc[i + 1] - vc[i]) * smoothstep(float(np.clip(t, 0, 1)))
    # time along the path (fine integration)
    ss = np.linspace(0, S[-1], 20000); vs = np.array([v_at(s) for s in ss])
    tt = np.concatenate([[0.0], np.cumsum(np.diff(ss) / ((vs[1:] + vs[:-1]) * 0.5))])
    T = tt[-1]
    n = int(math.floor(T * fps)) + 1
    frames = []
    for f in range(n):
        t = f / fps; s = float(np.interp(t, tt, ss))
        frames.append({"t": t_start + t, "s": s, "pos": at_s(poly, S, s)})
    named = {c["name"]: t_start + float(np.interp(sc[i], ss, tt)) for i, c in enumerate(ctrl) if c.get("name")}
    return frames, named, poly, sc, ss, tt

# ------------------------------------------------------------------ FPV drone (Video B)
FPV_FPS = 60
GIR2 = (-200.0, 3300.0)            # BP_Giraffe_2, wander radius 0 in L_Zoo_Cine (orbit target)
HOLO_G = (850.0, 2750.0)           # giraffe hologram entrance start (the IN effect plays here)
HOLO_W = (2950.0, -4320.0)         # wombat hologram body (wombat_place.py)

def orbit(cx, cy, r, a0, a1, z, v, n=None, mode="h", look=None, lw=0.0, name0=None):
    n = n or max(3, int(abs(a1 - a0) / 40) + 1)
    out = []
    for i in range(n):
        a = math.radians(a0 + (a1 - a0) * i / (n - 1))
        d = {"p": (cx + r * math.cos(a), cy + r * math.sin(a), z, mode), "v": v}
        if look is not None: d["look"] = look; d["lw"] = lw
        if i == 0 and name0: d["name"] = name0
        out.append(d)
    return out

def fpv_ctrl():
    g_look = (GIR2[0], GIR2[1], 330.0, "h")
    hg_look = (860.0, 2350.0, 300.0, "h")   # where the hologram stands while it materializes (seen in the preview)
    hw_look = (HOLO_W[0], HOLO_W[1] + 70.0, -20.0, "a")
    park = (-1000, 500, 0, "a")
    c = [
        # 1. high over the south-west forest, skim the canopy, dive into the entrance road
        {"p": (-10200, -11600, 4300, "a"), "v": 8, "name": "start"},
        {"p": (-9500, -10200, 3900, "a"), "v": 8.5},
        {"p": (-9300, -8200, 3300, "a"), "v": 9},
        {"p": (-9000, -6400, 2900, "a"), "v": 10.1},
        {"p": (-8700, -4600, 2700, "a"), "v": 11, "name": "canopy"},
        {"p": (-8000, -3000, 2600, "a"), "v": 12},
        {"p": (-7300, -1700, 2650, "a"), "v": 12.9, "name": "dive_top"},
        {"p": (-6900, -700, 1350, "h"), "v": 18},
        {"p": (-6400, -250, 450, "h"), "v": 18},
        {"p": (-5900, -120, 240, "h"), "v": 17},
        {"p": (-5520, -90, 220, "h"), "v": 11.8, "name": "road"},
        # 2. through the tusk gate, round the pendopo, through the papan
        {"p": (-4980, 0, 230, "h"), "v": 10.1},
        {"p": (-4600, 30, 250, "h"), "v": 9.4, "name": "tusks"},
        {"p": (-4300, 240, 285, "h"), "v": 9.3},
        {"p": (-4000, 495, 305, "h"), "v": 8.5, "name": "pendopo"},
        {"p": (-3620, 530, 305, "h"), "v": 8.5},
        {"p": (-3320, 340, 325, "h"), "v": 9.3},
        {"p": (-3100, 260, 330, "h"), "v": 9.4, "name": "papan"},
        # 3. low along the visitor path, hop the fence, skim the canal westwards
        {"p": (-2780, 160, 240, "h"), "v": 10.1},
        {"p": (-2350, 180, 190, "h"), "v": 10.9, "name": "path"},
        {"p": (-1700, 340, 190, "h"), "v": 11.8},
        {"p": (-1100, 520, 180, "h"), "v": 11.8},
        {"p": (-600, 600, 230, "h"), "v": 10.9},
        {"p": (-350, 1000, 380, "h"), "v": 10.1, "name": "fence"},
        {"p": (-450, 1500, 80, "a"), "v": 10.1},
        {"p": (-900, 1560, 60, "a"), "v": 11.8, "name": "canal"},
        {"p": (-1600, 1550, 60, "a"), "v": 12.5},
        {"p": (-2200, 1560, 80, "a"), "v": 11.8},
        # 4. up the west side of the giraffe habitat, round the oaks
        {"p": (-2550, 2050, 260, "a"), "v": 10.1},
        {"p": (-2600, 2800, 320, "h"), "v": 10.1},
        {"p": (-2550, 3600, 350, "h"), "v": 10.1},
        {"p": (-2250, 4350, 380, "h"), "v": 10.1},
        {"p": (-1600, 4750, 400, "h"), "v": 9.4},
        {"p": (-950, 4450, 350, "h"), "v": 9.3},
        {"p": (-850, 3800, 300, "h"), "v": 8.5},
    ]
    # 5. fast orbit round the standing giraffe (2.25 loops, counter-clockwise), looking in at it
    c += orbit(GIR2[0], GIR2[1], 620, 180, 990, 300, 9, n=22, look=g_look, lw=0.7, name0="orbit")
    c[-1]["name"] = "orbit_end"
    c += [
        # 6. past the giraffe hologram while it materializes, above the real giraffes, then the east side
        {"p": (250, 2250, 500, "h"), "v": 8.5, "look": hg_look, "lw": 0.4},
        {"p": (850, 1950, 620, "h"), "v": 8.5, "name": "holo_g", "look": hg_look, "lw": 0.85},
        {"p": (1450, 1900, 620, "h"), "v": 9.3, "look": hg_look, "lw": 0.4},
        {"p": (2200, 1950, 560, "h"), "v": 9.4},
        {"p": (3050, 1750, 300, "a"), "v": 10.1},
        {"p": (3350, 1100, 450, "h"), "v": 10.9},
        {"p": (3100, 300, 230, "h"), "v": 10.9},
        # 7. over the pond: chase Duma low over the water and overtake him
        {"p": (2350, -350, 150, "a"), "v": 11.8, "name": "pond_in"},
        {"p": (1900, -800, 80, "a"), "v": 12.5},
        {"p": (1880, -1100, 70, "a"), "v": 12.5, "name": "chase"},
        {"p": (1960, -1500, 85, "a"), "v": 12.5},
        {"p": (2130, -2000, 120, "a"), "v": 11.8, "name": "overtake"},
        {"p": (2330, -2550, 260, "a"), "v": 10.1},
        {"p": (2380, -3080, 400, "a"), "v": 9.3},
        # 8. into the wombat pen past the hologram as it materializes
        # (AR_Wombat_Overlook bounds x 2296..3674, y -4977..-3687, top z 668: stay outside them, the scale-6.6 hologram fills it)
        {"p": (2550, -3250, 380, "a"), "v": 8.5},
        {"p": (2950, -3200, 420, "a"), "v": 8, "name": "holo_w", "look": hw_look, "lw": 0.6},
        {"p": (3450, -3350, 520, "a"), "v": 8, "look": hw_look, "lw": 0.45},
        {"p": (4100, -3900, 700, "a"), "v": 8.5},
        {"p": (3500, -4650, 1050, "a"), "v": 9, "look": hw_look, "lw": 0.55},
        # 9. west past the rest-area gazebo, skim the south forest, pull up over the whole park
        {"p": (1950, -4400, 600, "h"), "v": 10.9},
        {"p": (1350, -3650, 420, "h"), "v": 10.9},
        {"p": (500, -3600, 400, "h"), "v": 10.9},
        {"p": (-300, -3950, 400, "h"), "v": 10.1, "name": "gazebo"},
        {"p": (-1200, -4600, 450, "h"), "v": 11},
        {"p": (-2400, -4900, 800, "h"), "v": 11, "name": "south_forest"},
        {"p": (-3600, -4400, 700, "h"), "v": 10.1, "look": (-3750, 0, 300, "a"), "lw": 0.3},
        {"p": (-4300, -3600, 1000, "h"), "v": 9, "look": (-3750, 0, 300, "a"), "lw": 0.7},
        {"p": (-4700, -2900, 1800, "h"), "v": 4.5, "look": (-3750, 0, 300, "a"), "lw": 0.9},
        {"p": (-4900, -2500, 2600, "h"), "v": 3, "look": (-2500, 1200, 0, "a"), "lw": 0.95, "name": "end"},
    ]
    return c

def noise1(t, seed, freqs):
    rnd = random.Random(seed); v = 0.0
    for f, a in freqs:
        v += a * math.sin(2 * math.pi * f * t + rnd.uniform(0, 6.283))
    return v

def build_fpv():
    ctrl = fpv_ctrl()
    frames, named, poly, sc, ss, tt = timed_path(ctrl, FPV_FPS)
    n = len(frames); fps = FPV_FPS
    pos = np.array([f["pos"] for f in frames])
    vel = np.gradient(pos, 1.0 / fps, axis=0); vel = gauss_smooth(vel, 3)
    acc = np.gradient(vel, 1.0 / fps, axis=0); acc = gauss_smooth(acc, 9)
    # look-target blend weights along the path (smooth in arc length)
    lw_c = np.array([c.get("lw", 0.0) for c in ctrl]); look_c = [P(c["look"]) if c.get("look") else None for c in ctrl]
    yaw, pitch, roll = [], [], []
    for k, fr in enumerate(frames):
        s = fr["s"]
        i = int(np.clip(np.searchsorted(sc, s) - 1, 0, len(sc) - 2)); u = smoothstep(float(np.clip((s - sc[i]) / max(sc[i + 1] - sc[i], 1e-6), 0, 1)))
        w = lw_c[i] * (1 - u) + lw_c[i + 1] * u
        tgt = None
        if look_c[i] is not None and look_c[i + 1] is not None: tgt = look_c[i] * (1 - u) + look_c[i + 1] * u
        elif look_c[i] is not None: tgt = look_c[i]; w = lw_c[i] * (1 - u)
        elif look_c[i + 1] is not None: tgt = look_c[i + 1]; w = lw_c[i + 1] * u
        h = vel[k] / max(np.linalg.norm(vel[k]), 1e-6)
        d = h
        if tgt is not None and w > 0:
            g = tgt - pos[k]; g = g / max(np.linalg.norm(g), 1e-6)
            d = h * (1 - w) + g * w; d = d / np.linalg.norm(d)
        yaw.append(math.degrees(math.atan2(d[1], d[0]))); pitch.append(math.degrees(math.asin(np.clip(d[2], -1, 1))))
    yaw = np.array(unwrap(yaw)); pitch = np.array(pitch)
    # bank from lateral acceleration, nose down while accelerating (drone tilts into its acceleration)
    for k in range(n):
        ry = math.radians(yaw[k]); fwd = np.array([math.cos(ry), math.sin(ry), 0.0]); right = np.array([-math.sin(ry), math.cos(ry), 0.0])
        a_lat = float(np.dot(acc[k], right)) / 100.0; a_fwd = float(np.dot(acc[k], fwd)) / 100.0
        roll.append(float(np.clip(math.degrees(math.atan2(a_lat, 9.81)) * 0.9, -40, 40)))
        pitch[k] += float(np.clip(-1.6 * a_fwd, -9, 9))
    roll = gauss_smooth(np.array(roll), 10); pitch = gauss_smooth(pitch, 6); yaw = gauss_smooth(yaw, 4)
    # micro vibration (props + wind): small, mostly in rotation
    keys = []
    for k in range(n):
        t = k / fps
        sp = np.linalg.norm(vel[k]) / 100.0; amp = 0.6 + 0.4 * min(sp / 18.0, 1.0)
        jp = amp * (noise1(t, 1, [(0.7, 0.35), (2.3, 0.18), (7.9, 0.07), (13.1, 0.04)]))
        jy = amp * (noise1(t, 2, [(0.5, 0.40), (1.9, 0.16), (8.7, 0.06), (12.3, 0.04)]))
        jr = amp * (noise1(t, 3, [(0.6, 0.55), (2.9, 0.20), (9.3, 0.08)]))
        jz = amp * noise1(t, 4, [(1.1, 1.2), (6.1, 0.35)])
        p = pos[k]
        keys.append([round(p[0], 2), round(p[1], 2), round(p[2] + jz, 2), round(float(np.clip(roll[k] + jr, -40, 40)), 3), round(pitch[k] + jp, 3), round(yaw[k] + jy, 3)])
    speeds = np.linalg.norm(vel, axis=1) / 100.0
    T = (n - 1) / fps
    # AR + Duma timing from the flight
    t_hg = named["holo_g"]; t_hw = named["holo_w"]; t_chase = named["chase"]
    visitor = [[0.0, -5320, -60, 100]]
    visitor += [[t_hg - 3.6, 950, 760, 200], [t_hg - 2.4, 950, 1350, 300]]           # deck arrival (route ends), then the viewing zone
    visitor += [[t_hw - 4.0, 1720, -3300, 100], [t_hw - 2.6, 1930, -4350, 170]]      # wombat place, then the overlook zone
    duma = duma_chase(t_chase, frames, fps)
    return {"fps": fps, "frames": n, "duration": T, "keys": keys, "speed": [round(float(s), 2) for s in speeds],
            "named": {k: round(v, 2) for k, v in named.items()}, "visitor": visitor, "duma": [duma],
            "poly": pos[::6, :].round(1).tolist()}

def duma_chase(t_chase, frames, fps):
    """Duma flies south over the pond ahead of the drone; the drone (16 m/s) catches up from behind and overtakes him."""
    t0 = t_chase - 2.6                         # spawn (materialize) well ahead of the drone
    ctrl = [
        {"p": (1900, -1250, 95, "a"), "v": 2.0},
        {"p": (1960, -1550, 100, "a"), "v": 7.5},
        {"p": (2080, -2050, 150, "a"), "v": 9.0},
        {"p": (2260, -2600, 260, "a"), "v": 9.0},
        {"p": (2350, -3150, 430, "a"), "v": 8.0},
        {"p": (2600, -3500, 420, "a"), "v": 6.0},
        {"p": (3000, -3600, 450, "a"), "v": 4.0},
    ]
    return duma_track("Duma_Chase", ctrl, t0, hover=1.3, fps=fps, end_hold=1.0)

def duma_track(name, ctrl, t_spawn, hover, fps, end_hold=0.5, bob=6.0):
    fr, named, *_ = timed_path(ctrl, fps, t_start=t_spawn + hover)
    pos = np.array([f["pos"] for f in fr])
    keys = []
    first = pos[0]; d0 = pos[min(5, len(pos) - 1)] - pos[0]; yaw0 = math.degrees(math.atan2(d0[1], d0[0]))
    nh = int(round(hover * fps))
    for k in range(nh): keys.append([first[0], first[1], first[2] + bob * math.sin(k / fps * 2.4), 0.0, yaw0])
    vel = np.gradient(pos, axis=0); vel = gauss_smooth(vel, 4)
    yaws = unwrap([math.degrees(math.atan2(v[1], v[0])) if np.linalg.norm(v[:2]) > 0.05 else yaw0 for v in vel])
    yaws = gauss_smooth(np.array(yaws), 6)
    for k in range(len(pos)):
        p = pos[k]; tt = (nh + k) / fps
        keys.append([p[0], p[1], p[2] + bob * 0.5 * math.sin(tt * 2.4), 0.0, yaws[k]])
    last = keys[-1]
    for k in range(int(round(end_hold * fps))): keys.append([last[0], last[1], last[2] + bob * math.sin(k / fps * 2.4), 0.0, last[4]])
    f0 = int(round(t_spawn * fps))
    return {"name": name, "f0": f0, "f1": f0 + len(keys), "keys": [[round(v, 2) for v in k] for k in keys]}

# ------------------------------------------------------------------ Cinematic (Video A)
CINE_FPS = 30
HEAD_G4 = (190.0, 0.0, 320.0)       # BP_Giraffe_4 head, actor space
BODY_W = (-271.0, -37.0, -20.0)    # wombat body, actor space (SK_Wombat offset)

def arc(cx, cy, r, a0, a1, z, mode="h", n=5):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / (n - 1))), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / (n - 1))), z, mode) for i in range(n)]

# Card centres in the world with the panel facing the card-shot camera (logged by ZooCinematicGameMode "Card placement";
# the panel turns to face the camera, so the close-ups stay on the logged camera ray): MEDIA, KARAKTERISTIK, GENERAL DESCRIPTION, HABITAT
CARDS_G = [(1151.6, 2450.3, 694.3), (1150.4, 2476.0, 425.7), (589.5, 2425.4, 694.3), (588.5, 2453.6, 425.7)]
CARDS_W = [(2468.5, -4532.6, 550.9), (2419.0, -4488.9, 289.2), (2841.0, -4111.0, 550.7), (2789.4, -4065.8, 289.4)]
CARD_STEP = 6.5
G_CLICKS = [95.0 + i * CARD_STEP for i in range(4)]
W_CLICKS = [151.0 + i * CARD_STEP for i in range(4)]

def card_looks(cards, t0, t1, clicks):
    """Look keys (time, point): on each card from just before its click; pans take a second."""
    keys = [(t0, cards[0])]
    for i in range(1, len(cards)):
        keys += [(clicks[i] - 0.9, cards[i - 1]), (clicks[i] + 0.1, cards[i])]
    return keys + [(t1, cards[-1])]

def cine_shots():
    S = []
    def shot(name, t0, t1, desc, **k): S.append(dict(name=name, t0=t0, t1=t1, desc=desc, **k))
    shot("01_Aerial_Establish", 0, 10, "Aerial pembuka dari barat daya: hutan, gerbang gading, pendopo, papan Taman Safari; kamera turun perlahan.",
         cam=[(-9500, -5200, 5200, "a"), (-8200, -3000, 4300, "a")], look=[(-3500, 300, 300, "a"), (-3800, 0, 300, "a")], focal=24, fstop=8, e=0.7)
    shot("02_Gate_Dolly", 10, 20, "Dolly rendah di jalan masuk menuju gerbang gading; rute AR PathGuide menyala di tanah.",
         cam=[(-7000, -180, 150, "h"), (-5700, -70, 170, "h")], look=[(-4500, 0, 520, "a"), (-4150, 0, 420, "a")], focal=20, fstop=5.6, e=0.4)
    shot("03_Pendopo_Orbit", 20, 29, "Orbit pelan pendopo dari sisi utara, gading dan papan di latar.",
         cam=arc(-3750, 0, 1150, 112, 75, 280), look=[(-3750, 0, 380, "a")] * 2, focal=30, fstop=5.6, e=0.5)
    shot("04_Duma_Path", 29, 39, "Duma (pemandu hologram) muncul di jalur lalu terbang memimpin ke timur dengan trail pita; rute AR di tanah.",
         cam=[(-2850, -260, 160, "h"), (-2080, 60, 165, "h")], look="duma:Duma_Path", focal=28, fstop=4, e=0.4)
    shot("05_Duma_Follow", 39, 51, "Kamera mengejar Duma di atas kanal dan mendekat (zoom in), lalu berhenti dan diam; Duma terus terbang menjauh ke habitat jerapah.",
         follow="Duma_Canal", follow_until=45.5, focal=35, fstop=5.6)
    shot("06_Giraffe_Walk", 51, 61, "Tracking jerapah berjalan di habitat (kamera mengikuti dari tenggara).",
         cam=[(2150, 3500, 250, "h"), (1850, 3250, 250, "h")], track=("BP_Giraffe_3", (60.0, 0.0, 280.0)), focal=30, fstop=4, e=0.4)
    shot("07_Giraffe_LowAngle", 61, 68, "Low-angle jerapah dari dekat tanah, langit di belakang.",
         cam=[(320, 3480, 75, "h"), (240, 3440, 80, "h")], track=("BP_Giraffe_2", (120.0, 0.0, 260.0)), focal=20, fstop=4, e=0.5)
    shot("08_Giraffe_HeadCU", 68, 76, "Close-up kepala jerapah, lensa panjang, DoF tipis.",
         cam=[(1290, 2200, 310, "a"), (1320, 2160, 312, "a")], track=("BP_Giraffe_4", HEAD_G4), focal=85, fstop=2.8, e=0.5)
    shot("09_AR_Giraffe_Reveal", 76, 85, "Reveal AR dari dek: cincin scan di tanah lalu hologram jerapah materialize (kamera sudah siap sebelum muncul).",
         cam=[(1080, 1060, 360, "a"), (1050, 1230, 360, "a")], look=[(850, 2700, 330, "a"), (850, 2620, 330, "a")], focal=22, fstop=5.6, e=0.5)
    shot("10_AR_Giraffe_Walk", 85, 94, "Hologram berjalan masuk; jerapah nyata mendekat ke exhibit.",
         cam=[(250, 1450, 480, "a"), (400, 1600, 480, "a")], look=[(850, 2400, 450, "a"), (850, 2350, 460, "a")], focal=24, fstop=5.6, e=0.4)
    shot("11_AR_Giraffe_Cards", 94, 96, "Kartu info AR jerapah melayang di atas habitat; kartu MEDIA diklik.",
         cam=[(920, 1450, 460, "a"), (915, 1470, 462, "a")], look=[(870, 2440, 470, "a")] * 2, focal=22, fstop=8, e=0.5)
    shot("11b_AR_Giraffe_Cards_Open", 96, 120, "Close-up kartu jerapah dibuka satu per satu dan isinya di-scroll: MEDIA (video + PAUSE/EXTEND), KARAKTERISTIK, GENERAL DESCRIPTION, HABITAT.",
         cam=[(920, 1450, 460, "a"), (915, 1490, 462, "a")], look_t=card_looks(CARDS_G, 96, 120, G_CLICKS), focal=45, fstop=8, e=0.5)
    shot("12_Pond_Duma", 120, 130, "Glide rendah di atas kolam; Duma terbang menyeberang ke jembatan.",
         cam=[(1650, -1750, 95, "a"), (1950, -1650, 95, "a")], look="duma:Duma_Pond", focal=28, fstop=5.6, e=0.4)
    # no actor tracking here: the wombats burrow and turn fast, which whipped the look-at camera around in the Final render
    shot("13_Wombat_Pen", 130, 139, "Kandang wombat: wombat nyata di sekitar sarangnya.",
         cam=[(3700, -5350, 300, "a"), (3600, -5300, 310, "a")], look=[(3120, -4720, -100, "a"), (3080, -4680, -100, "a")], focal=24, fstop=8, e=0.4)
    shot("14_AR_Wombat_Reveal", 139, 150, "Dari overlook: cincin scan di lantai kandang, hologram wombat materialize, kartu info muncul.",
         cam=[(1500, -4150, 330, "a"), (1650, -4200, 320, "a")], look=[(2750, -4300, 230, "a"), (2750, -4300, 250, "a")], focal=20, fstop=5.6, e=0.5)
    shot("15_AR_Wombat_Cards", 150, 152, "Kartu info AR wombat di atas hologram; kartu MEDIA diklik.",
         cam=[(1500, -3300, 800, "a"), (1530, -3330, 795, "a")], look=[(2750, -4300, 200, "a"), (2750, -4300, 195, "a")], focal=20, fstop=8, e=0.5)
    shot("15b_AR_Wombat_Cards_Open", 152, 176, "Close-up kartu wombat dibuka satu per satu dan isinya di-scroll: MEDIA (video + PAUSE/EXTEND), KARAKTERISTIK, GENERAL DESCRIPTION, HABITAT.",
         cam=[(1863, -3623, 677, "a"), (1880, -3638, 683, "a")], look_t=card_looks(CARDS_W, 152, 176, W_CLICKS), focal=45, fstop=8, e=0.5)
    shot("16_Finale_Crane", 176, 189, "Crane naik dari tepi kolam ke aerial taman, cahaya golden hour, fade out.",
         cam=[(2050, -2500, 300, "a"), (1950, -2750, 1400, "a"), (1400, -3500, 2900, "a")], look=[(2200, -1000, 100, "a"), (1000, 0, 0, "a"), (0, 1500, 0, "a")], focal=24, fstop=8, e=0.6)
    return S

def cine_duma():
    out = []
    # 04: materializes on the path ahead, then leads east along the walkway
    out.append(duma_track("Duma_Path", [
        {"p": (-2380, 30, 175, "h"), "v": 0.5}, {"p": (-2150, 110, 185, "h"), "v": 2.5}, {"p": (-1700, 330, 200, "h"), "v": 3.0},
        {"p": (-1100, 520, 210, "h"), "v": 3.2}, {"p": (-400, 560, 230, "h"), "v": 3.2}], 29.4, hover=1.6, fps=CINE_FPS))
    # 05: crosses the canal from the west into the giraffe habitat
    out.append(duma_track("Duma_Canal", [
        {"p": (-2300, 1250, 150, "a"), "v": 4.5}, {"p": (-1300, 1500, 110, "a"), "v": 5.0}, {"p": (-300, 1700, 140, "a"), "v": 5.0},
        {"p": (300, 2200, 320, "a"), "v": 4.8}, {"p": (700, 2900, 450, "a"), "v": 5.0}, {"p": (1000, 3600, 750, "a"), "v": 5.0},
        {"p": (1250, 4600, 1500, "a"), "v": 5.5}], 36.8, hover=1.2, fps=CINE_FPS, end_hold=2.5))
    # 12: over the pond towards the bridge
    out.append(duma_track("Duma_Pond", [
        {"p": (1850, -900, 120, "a"), "v": 1.0}, {"p": (2150, -1150, 130, "a"), "v": 4.0}, {"p": (2500, -1250, 220, "a"), "v": 4.5},
        {"p": (2800, -1100, 340, "a"), "v": 4.5}, {"p": (2950, -600, 330, "a"), "v": 4.0}, {"p": (2900, -150, 300, "a"), "v": 3.5}], 120.4, hover=1.5, fps=CINE_FPS))
    return out

def duma_pos_at(duma, name, f):
    d = [x for x in duma if x["name"] == name][0]
    k = int(np.clip(f - d["f0"], 0, len(d["keys"]) - 1))
    return np.array(d["keys"][k][:3])

def follow_keys(sh, duma, fps, f0, f1):
    """Chase a flying Duma from behind and close in on him, then ease to a stop and hold while he flies on alone."""
    name = sh["follow"]; fu = int(round(sh["follow_until"] * fps)) - f0
    dp = np.array([duma_pos_at(duma, name, f) for f in range(f0, f1)])
    vel = gauss_smooth(np.gradient(dp, axis=0), 10)
    cams, d_prev = [], np.array([1.0, 0.0, 0.0])
    for i in range(fu):
        h = vel[i].copy(); h[2] = 0.0
        d = h / np.linalg.norm(h) if np.linalg.norm(h) > 0.5 else d_prev; d_prev = d
        dist = 650.0 + (300.0 - 650.0) * smoothstep(i / max(fu - 1, 1))          # zoom in: the camera closes the gap
        side = np.array([-d[1], d[0], 0.0])
        cams.append(dp[i] - d * dist + side * 60.0 + np.array([0.0, 0.0, 80.0]))
    cams = list(gauss_smooth(np.array(cams), 8))
    v = cams[-1] - cams[-2]; stop = 40                                          # brake over 1.3 s, then hold
    for k in range(f1 - f0 - fu):
        v = v * max(0.0, 1.0 - 1.0 / stop) if k < stop else v * 0.0
        cams.append(cams[-1] + v)
    hold = dp[-1] + np.array([0.0, 0.0, -600.0])                               # where he climbs out over the trees: the frame stays put
    looks, l = [], dp[0]
    for i in range(f1 - f0):
        l = l + (dp[i] - l) * 0.35
        w = smoothstep(min(max((i - fu - 18) / 50.0, 0.0), 1.0))   # keep him in frame while braking, then let go
        looks.append(l * (1 - w) + hold * w)
    return cams, looks

def card_demo(exhibit, t0, step=6.5, cards=4):
    """Unfold the cards one by one (panel marker X = exhibit, Y = card) and scroll each from top to bottom (Z %)."""
    steps, scroll = [], []
    for i in range(cards):
        t = t0 + i * step
        steps.append([t, exhibit, i + 1])
        scroll += [[t, 0.0], [t + 1.6, 0.0], [t + step - 0.7, 100.0], [t + step - 1.0 / CINE_FPS, 100.0]]
    steps.append([t0 + cards * step, exhibit, 0])
    return steps, scroll

def build_cinematic():
    fps = CINE_FPS; shots = cine_shots(); duma = cine_duma()
    N = int(shots[-1]["t1"] * fps)
    cams = []
    for sh in shots:
        f0, f1 = int(round(sh["t0"] * fps)), int(round(sh["t1"] * fps))
        if sh.get("follow"):
            cpos, clook = follow_keys(sh, duma, fps, f0, f1)
            keys = []
            for c, l in zip(cpos, clook):
                pitch, yaw = look_rot(c, l)
                keys.append([c[0], c[1], c[2], 0.0, pitch, yaw, float(np.linalg.norm(l - c))])
            yaws = unwrap([k[5] for k in keys])
            for k, y in zip(keys, yaws): k[5] = y
            keys[0][6] = keys[len(keys) // 2][6] = 600.0                       # manual focus: Duma at the end of the chase
            cams.append({"name": sh["name"], "f0": f0, "f1": f1, "focal": sh["focal"], "fstop": sh["fstop"], "track": None,
                         "keys": [[round(v, 3) for v in k] for k in keys]})
            continue
        cp = [P(p) for p in sh["cam"]]
        poly = catmull(cp, 60); S = arclen(poly)
        lp = None
        if isinstance(sh.get("look"), list):
            lpts = [P(p) for p in sh["look"]]; lpoly = catmull(lpts, 60); LS = arclen(lpoly)
        keys = []
        prev_look = None
        for f in range(f0, f1):
            tau = (f - f0) / max(f1 - f0 - 1, 1); u = ease(tau, sh.get("e", 0.5))
            c = at_s(poly, S, u * S[-1])
            if sh.get("look_t"):
                t = f / fps; lt = sh["look_t"]
                j = max([i for i in range(len(lt)) if lt[i][0] <= t] or [0]); j = min(j, len(lt) - 2)
                w = smoothstep(min(max((t - lt[j][0]) / max(lt[j + 1][0] - lt[j][0], 1e-6), 0.0), 1.0))
                l = np.array(lt[j][1]) * (1 - w) + np.array(lt[j + 1][1]) * w
            elif isinstance(sh.get("look"), list): l = at_s(lpoly, LS, u * LS[-1]) if LS[-1] > 0 else lpoly[0]
            elif isinstance(sh.get("look"), str):
                tgt = duma_pos_at(duma, sh["look"].split(":")[1], f)
                l = tgt if prev_look is None else prev_look + (tgt - prev_look) * 0.12     # operator lag
            else:
                ax = sh["track"][0]; l = c + np.array([100.0, 0, 0])                       # replaced at runtime by look-at tracking
                l = np.array(TRACK_HOME[ax]) + np.array([0, 0, sh["track"][1][2]])
            prev_look = l
            pitch, yaw = look_rot(c, l)
            keys.append([c[0], c[1], c[2], 0.0, pitch, yaw, float(np.linalg.norm(l - c))])
        yaws = unwrap([k[5] for k in keys])
        for k, y in zip(keys, yaws): k[5] = y
        cams.append({"name": sh["name"], "f0": f0, "f1": f1, "focal": sh["focal"], "fstop": sh["fstop"],
                     "track": {"actor": sh["track"][0], "offset": list(sh["track"][1])} if sh.get("track") else None,
                     "keys": [[round(v, 3) for v in k] for k in keys]})
    visitor = [[0.0, -5320, -60, 100], [74.5, 950, 760, 200], [76.0, 950, 1350, 300],
               [120.0, 2850, -250, 150], [137.5, 1720, -3300, 100], [139.0, 1930, -4350, 170]]
    g_steps, g_scroll = card_demo(1, G_CLICKS[0], CARD_STEP)    # AR_Giraffe_Deck cards (up from about 85 s), shot 11
    w_steps, w_scroll = card_demo(2, W_CLICKS[0], CARD_STEP)    # AR_Wombat_Overlook cards (up from about 147 s), shot 15
    panels = {"steps": [[0.0, 0, 0]] + g_steps + w_steps, "scroll": [[0.0, 0.0]] + g_scroll + w_scroll}
    return {"fps": fps, "frames": N, "duration": N / fps, "cameras": cams, "duma": duma, "visitor": visitor, "panels": panels,
            "shots": [{k: sh[k] for k in ("name", "t0", "t1", "desc", "focal", "fstop")} for sh in shots]}

TRACK_HOME = {"BP_Giraffe_3": (900, 4200, 108), "BP_Giraffe_2": (-200, 3300, 44), "BP_Giraffe_4": (1900, 2300, -12), "BP_ARWombat_0": (2337, -4638, -118)}

# ------------------------------------------------------------------ probe + check
def probe_input(cine, fpv):
    paths = {}
    for c in cine["cameras"]:
        paths["A_" + c["name"]] = [k[:3] for k in c["keys"][::3]] + [c["keys"][-1][:3]]
    for d in cine["duma"]: paths["A_" + d["name"]] = [k[:3] for k in d["keys"][::3]]
    paths["B_FPV"] = [k[:3] for k in fpv["keys"]]
    for d in fpv["duma"]: paths["B_" + d["name"]] = [k[:3] for k in d["keys"][::3]]
    return {"max": 400, "radius": 60, "paths": paths}

def check(cine, fpv, inp):
    out = json.load(open(os.path.join(D, "probe_paths_out.json")))["paths"]
    bad = 0
    for name, pts in inp["paths"].items():
        res = out.get(name, [])
        step = 1.0 / FPV_FPS if name == "B_FPV" else 3.0 / (FPV_FPS if name.startswith("B_") else CINE_FPS)
        t0 = 0.0
        if name.startswith("A_") and not name.startswith("A_Duma"):
            t0 = [c for c in cine["cameras"] if "A_" + c["name"] == name][0]["f0"] / CINE_FPS
        dmin = min((r["d"] for r in res), default=999)
        hits = [(i, r) for i, r in enumerate(res) if "sweep" in r]
        lim = 60 if name == "B_FPV" else 35
        close = [(i, r) for i, r in enumerate(res) if r["d"] < lim]
        flag = "OK " if not hits and not close else "BAD"
        if flag == "BAD": bad += 1
        print("%s %-26s min dist %4d cm  sweep hits %3d  <%dcm %3d" % (flag, name, dmin, len(hits), lim, len(close)))
        shown = 0
        for i, r in (hits + close)[:400]:
            if shown >= 6: break
            p = pts[i]; print("      t %6.2f  (%d,%d,%d) d %d %s %s" % (t0 + i * step, p[0], p[1], p[2], r["d"], r.get("a", ""), ("SWEEP " + r["sweep"]) if "sweep" in r else ""))
            shown += 1
    return bad

def canopy_check(cine, fpv):
    """Foliage without collision is invisible to the probe: flag camera keys inside a tall-vegetation column (top grid)."""
    bad = 0
    def chk(name, keys, fps, f0):
        nonlocal bad
        hits = []
        for i, k in enumerate(keys):
            x, y, z = k[:3]
            if not FINE.inside(x, y): continue
            for dx, dy in ((0, 0), (120, 0), (-120, 0), (0, 120), (0, -120)):
                top = FINE.bilinear(FINE.top, x + dx, y + dy); g = land(x + dx, y + dy)
                if top - g > 250 and g + 150 < z < top + 80: hits.append(i); break
        if hits:
            bad += 1; i = hits[0]; print("CANOPY %-24s %d keys, first t %.2f at (%d,%d,%d)" % (name, len(hits), (f0 + i) / fps, *keys[i][:3]))
    for c in cine["cameras"]: chk(c["name"], c["keys"], CINE_FPS, c["f0"])
    chk("FPV", fpv["keys"], FPV_FPS, 0)
    return bad

def main():
    cine = build_cinematic(); fpv = build_fpv()
    json.dump(cine, open(os.path.join(D, "plan_cinematic.json"), "w"))
    json.dump(fpv, open(os.path.join(D, "plan_fpv.json"), "w"))
    inp = probe_input(cine, fpv)
    json.dump(inp, open(os.path.join(D, "probe_paths_in.json"), "w"))
    print("Cinematic %d frames (%.1f s), FPV %d frames (%.1f s @%d), FPV speed %.1f..%.1f m/s avg %.1f" % (
        cine["frames"], cine["duration"], fpv["frames"], fpv["duration"], FPV_FPS, min(fpv["speed"]), max(fpv["speed"]), sum(fpv["speed"]) / len(fpv["speed"])))
    print("FPV events:", fpv["named"])
    print("FPV roll range %.1f..%.1f" % (min(k[3] for k in fpv["keys"]), max(k[3] for k in fpv["keys"])))
    mp = {"FPV": fpv["poly"]}
    for c in cine["cameras"]: mp[c["name"][:2]] = [k[:3] for k in c["keys"][::10]]
    for d in cine["duma"] + fpv["duma"]: mp[d["name"]] = [k[:3] for k in d["keys"][::10]]
    json.dump(mp, open(os.path.join(D, "map_paths.json"), "w"))
    if "--check" in sys.argv: check(cine, fpv, inp)
    canopy_check(cine, fpv)

if __name__ == "__main__":
    main()
