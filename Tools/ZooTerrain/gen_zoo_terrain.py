"""Generate the L_Zoo landscape heightmap, paint-layer weightmaps and a review preview.

Run with UE's bundled Python (stdlib only):
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" gen_zoo_terrain.py

Reads zoo_layout.json (same folder). Writes to ./out:
  zoo_height_505.png      16-bit grayscale heightmap (import with Z scale 20)
  zoo_w_<Layer>.png       8-bit weightmaps, one per landscape layer, summing to 255
  zoo_preview.png         top-down review image (north up, east right)
  zoo_probe.json          terrain heights at key layout points (for post-import checks)

Pixel (col i, row j) -> world X = origin_x + i*m_per_px (north), Y = origin_y + j*m_per_px (east).
"""
import json
import math
import os
import struct
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
L = json.load(open(os.path.join(HERE, "zoo_layout.json"), encoding="utf-8-sig"))

N = L["landscape"]["size_px"]
MPP = L["landscape"]["m_per_px"]
OX, OY = L["landscape"]["origin_m"]
ZSCALE = L["landscape"]["z_scale"]

# Landscape layer names exactly as in M_Oak_Forest_Landscape, mapped to their zoo role.
LAYERS = [
    ("Auto_LYR", "lawn"),
    ("Oak_Forest_LYR_e", "dry"),
    ("Oak_Forest_LYR_a", "forest"),
    ("Oak_Forest_LYR_d", "under"),
    ("Red_Soil_LYR_a", "sand"),
    ("Oak_Forest_LYR_b", "soil"),
    ("Oak_Forest_LYR_c", "path"),
    ("Cliff_LYR_b", "rock"),
]


# ---------------------------------------------------------------- math helpers
def clamp(v, a=0.0, b=1.0):
    return a if v < a else b if v > b else v


def smoothstep(e0, e1, x):
    t = clamp((x - e0) / (e1 - e0))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def hash2(ix, iy, seed):
    h = (ix * 374761393 + iy * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFFFF) / float(0xFFFFFF)


def vnoise(x, y, seed):
    ix = math.floor(x)
    iy = math.floor(y)
    fx = x - ix
    fy = y - iy
    sx = fx * fx * (3 - 2 * fx)
    sy = fy * fy * (3 - 2 * fy)
    a = hash2(ix, iy, seed)
    b = hash2(ix + 1, iy, seed)
    c = hash2(ix, iy + 1, seed)
    d = hash2(ix + 1, iy + 1, seed)
    top = a + (b - a) * sx
    bot = c + (d - c) * sx
    return top + (bot - top) * sy


def fbm(x, y, seed, octaves=4):
    amp = 1.0
    tot = 0.0
    norm = 0.0
    f = 1.0
    for o in range(octaves):
        tot += amp * (vnoise(x * f, y * f, seed + o * 31) * 2.0 - 1.0)
        norm += amp
        amp *= 0.5
        f *= 2.0
    return tot / norm


def sd_round_rect(x, y, x0, x1, y0, y1, r):
    cx, cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    hx, hy = (x1 - x0) * 0.5 - r, (y1 - y0) * 0.5 - r
    qx, qy = abs(x - cx) - hx, abs(y - cy) - hy
    ox, oy = max(qx, 0.0), max(qy, 0.0)
    return math.hypot(ox, oy) + min(max(qx, qy), 0.0) - r


def sd_ellipse(x, y, cx, cy, rx, ry, rot_deg):
    a = math.radians(rot_deg)
    dx, dy = x - cx, y - cy
    lx = dx * math.cos(a) + dy * math.sin(a)
    ly = -dx * math.sin(a) + dy * math.cos(a)
    k = math.hypot(lx / rx, ly / ry)
    return (k - 1.0) * min(rx, ry)


def rect_bounds(poly):
    xs = [p[0] for p in poly]
    ys = [p[1] for p in poly]
    return min(xs), max(xs), min(ys), max(ys)


# ---------------------------------------------------------------- layout data
core_x0, core_x1 = L["core"]["x"]
core_y0, core_y1 = L["core"]["y"]
HL = L["hills"]

G = L["giraffe"]
pad_x0, pad_x1, pad_y0, pad_y1 = rect_bounds(G["paddock"])
moat = G["moat"]
knoll = G["knoll"]
kwall = G["knoll_wall"]

W = L["wombat"]
wx0, wx1, wy0, wy1 = rect_bounds(W["exhibit"])

P = L["pond"]

# path segments with bounding boxes: (ax, ay, az, bx, by, bz, halfw, bbox)
SEGS = []
for path in L["paths"]:
    pts = path["pts"]
    hw = path["width"] * 0.5
    for a, b in zip(pts[:-1], pts[1:]):
        pad = hw + 6.0
        SEGS.append((a[0], a[1], a[2], b[0], b[1], b[2], hw,
                     (min(a[0], b[0]) - pad, max(a[0], b[0]) + pad, min(a[1], b[1]) - pad, max(a[1], b[1]) + pad)))

GROVES = L.get("groves", [])
BEDS = L.get("beds", [])
MOUNDS = L.get("mounds", [])

# giraffe paddock wear: open sand where the animals will be presented / gather, and trampled desire lines
_anc = G["anchors"]
PADDOCK_SAND_ZONES = [(_anc["GIR_Anchor_Main"], 8.5), (_anc["GIR_Anchor_Feeder"], 5.5), (_anc["GIR_Anchor_Moat"], 7.0),
                      ([-9.0, 24.5], 4.5), (G["shelter"]["center"], 7.5), (G["wallow"]["center"], 5.5), ([29.0, 52.0], 4.5)]
PADDOCK_TRACKS = [
    [[29.0, 52.5], [33.0, 47.0], [24.0, 38.0], [17.0, 30.0], [12.0, 21.0]],   # gate -> shelter -> mound -> feeder
    [[12.0, 21.0], [2.0, 23.0], [-9.0, 24.5], [-7.0, 35.0], [-6.0, 42.0]],   # feeder -> second feeder -> wallow
    [[-6.0, 42.0], [-17.0, 36.0], [-22.0, 30.0]],                            # wallow -> west oak
    [[27.0, 25.0], [20.0, 22.0], [12.0, 21.0]],                              # east oak -> feeder
    [[2.0, 44.0], [14.0, 45.0], [24.0, 38.0]],                               # north oak -> mound
]


def seg_dist(x, y, a, b):
    vx, vy = b[0] - a[0], b[1] - a[1]
    ll = vx * vx + vy * vy
    t = 0.0 if ll == 0 else clamp(((x - a[0]) * vx + (y - a[1]) * vy) / ll)
    return math.hypot(x - (a[0] + vx * t), y - (a[1] + vy * t))


def nearest_path(x, y):
    """Returns (edge_distance, path_z) for the closest path segment, or (1e9, 0)."""
    best = 1e9
    bz = 0.0
    for ax, ay, az, bx, by, bz_, hw, bb in SEGS:
        if x < bb[0] or x > bb[1] or y < bb[2] or y > bb[3]:
            continue
        vx, vy = bx - ax, by - ay
        ll = vx * vx + vy * vy
        t = 0.0 if ll == 0 else clamp(((x - ax) * vx + (y - ay) * vy) / ll)
        px, py = ax + vx * t, ay + vy * t
        d = math.hypot(x - px, y - py) - hw
        if d < best:
            best = d
            bz = az + (bz_ - az) * t
    return best, bz


def hills_height(x, y):
    ox_n = max(x - core_x1, 0.0)
    ox_s = max(core_x0 - x, 0.0)
    oy_e = max(y - core_y1, 0.0)
    oy_w = max(core_y0 - y, 0.0)
    d = math.hypot(max(ox_n, ox_s), max(oy_e, oy_w))
    if d <= 0.0:
        return 0.0
    wsum = ox_n + ox_s + oy_e + oy_w
    H = (ox_n * HL["north"] + ox_s * HL["south"] + oy_e * HL["east"] + oy_w * HL["west"]) / wsum
    ramp = smoothstep(HL["start_margin"], HL["start_margin"] + HL["ramp"], d)
    undulate = 0.72 + 0.38 * (fbm(x / 38.0, y / 38.0, 101) * 0.5 + 0.5)
    bumps = 1.6 * fbm(x / 14.0, y / 14.0, 202) * smoothstep(HL["start_margin"], HL["start_margin"] + 12, d)
    return H * ramp * undulate + bumps


def paddock_floor(x, y):
    # rises gently away from the visitors (east) so the paddock reads like an amphitheatre
    f = G["floor_z"] + G.get("east_rise", 0.0) * smoothstep(pad_y0 + 4.0, pad_y1 - 2.0, y)
    f += 0.28 * fbm(x / 16.0, y / 16.0, 303)
    m = G["mound"]
    dm = math.hypot(x - m["center"][0], y - m["center"][1])
    f += m["height"] * (1.0 - smoothstep(0.0, m["radius"], dm)) ** 1.3
    wl = G["wallow"]
    dw = math.hypot(x - wl["center"][0], y - wl["center"][1])
    f -= wl["depth"] * (1.0 - smoothstep(0.0, wl["radius"], dw))
    return f


def pond_sdf(x, y):
    s = min(sd_ellipse(x, y, lb["center"][0], lb["center"][1], lb["radii"][0], lb["radii"][1], lb["rot_deg"])
            for lb in P["lobes"])
    return s + 0.7 * fbm(x / 5.0, y / 5.0, 404)


def pit_floor(x, y):
    f = W["floor_z"] + 0.12 * fbm(x / 5.0, y / 5.0, 505)
    m = W["mound"]
    dm = math.hypot(x - m["center"][0], y - m["center"][1])
    f += m["height"] * (1.0 - smoothstep(0.0, m["radius"], dm)) ** 1.2
    # back bank (north + west sides) rises to back_bank_z
    un = wx1 - x
    vw = y - wy0
    bank = max(1.0 - smoothstep(0.0, W["back_bank_width"], un), 1.0 - smoothstep(0.0, W["back_bank_width"], vw))
    return lerp(f, W["back_bank_z"] + 0.2 * fbm(x / 4.0, y / 4.0, 606), bank ** 1.6)


# ---------------------------------------------------------------- height field
def compute_height(x, y):
    # a) base lawn + hills
    h = 0.32 * fbm(x / 26.0, y / 26.0, 11) + 0.1 * fbm(x / 7.0, y / 7.0, 12)
    h += hills_height(x, y)
    for m in MOUNDS:
        dm = math.hypot((x - m["c"][0]) / m.get("sx", 1.0), (y - m["c"][1]) / m.get("sy", 1.0))
        if dm < m["r"]:
            h += m["h"] * (1.0 - smoothstep(0.0, m["r"], dm)) ** 1.4

    # b) knoll (restricted to the visitor side of its retaining wall)
    dk = math.hypot(x - knoll["center"][0], y - knoll["center"][1])
    kh = knoll["top_z"] * (1.0 - smoothstep(knoll["top_radius"], knoll["base_radius"], dk))
    wall_y = kwall["y"] if kwall["x"][0] <= x <= kwall["x"][1] else moat["wall_y"]
    if y < wall_y or x > kwall["x"][1]:
        h = max(h, kh)

    # c) pond bowl
    ps = pond_sdf(x, y)
    if ps < P["shore_width"]:
        if ps <= 0.0:
            h = lerp(P["water_z"] - 0.12, P["bed_z"], smoothstep(0.0, 4.5, -ps))
        else:
            h = lerp(P["water_z"] + 0.05, h, smoothstep(0.0, P["shore_width"], ps))

    # d) pads: plaza, rest area, wombat overlook
    pz = L["plaza"]
    s = sd_round_rect(x, y, pz["center"][0] - pz["half"][0], pz["center"][0] + pz["half"][0],
                      pz["center"][1] - pz["half"][1], pz["center"][1] + pz["half"][1], pz["radius"])
    if s < 4.0:
        h = lerp(pz["z"], h, smoothstep(0.0, 4.0, s))
    ra = L["rest_area"]
    s = math.hypot(x - ra["center"][0], y - ra["center"][1]) - ra["radius"]
    if s < 3.5:
        h = lerp(ra["z"], h, smoothstep(0.0, 3.5, s))
    ov = W["overlook"]
    s = sd_round_rect(x, y, ov["center"][0] - ov["size"][0] / 2, ov["center"][0] + ov["size"][0] / 2,
                      ov["center"][1] - ov["size"][1] / 2, ov["center"][1] + ov["size"][1] / 2, 0.5)
    if s < 1.5:
        h = lerp(0.1, h, smoothstep(0.0, 1.5, s))

    # e) paths: flat corridor + natural 1:1.6 shoulders
    d, pzv = nearest_path(x, y)
    if d < 6.0:
        blend = max(1.8, abs(h - pzv) * 1.6)
        h = lerp(pzv, h, smoothstep(0.25, 0.25 + blend, d))

    # f) giraffe paddock basin (soft on fence sides) + moat + deck bay
    s = sd_round_rect(x, y, pad_x0, pad_x1, pad_y0, pad_y1, G["paddock_corner_radius"])
    fz = None
    if s < 0.5:
        fz = paddock_floor(x, y)
        h = lerp(h, fz, 1.0 - smoothstep(-4.0, 0.5, s))
    if moat["x"][0] - 0.5 <= x <= moat["x"][1] and moat["wall_y"] <= y <= pad_y0 + 4.0:
        fz = paddock_floor(x, y)
        b0, b1 = moat["bottom_y"]
        if y <= b1:
            prof = moat["bottom_z"]
        elif y <= pad_y0:
            prof = lerp(moat["bottom_z"], fz, smoothstep(b1, pad_y0 + 0.3, y))
        else:
            prof = fz
        end_t = smoothstep(moat["x"][0] - 0.5, moat["x"][0] + 1.5, x)
        h = lerp(h, prof, end_t) if y <= pad_y0 else prof
    if kwall["x"][0] <= x <= kwall["x"][1] and kwall["y"] <= y <= pad_y0 + 4.0:
        h = paddock_floor(x, y)

    # g) wombat pit (hard front walls south/east, soft back bank)
    if wx0 <= x <= wx1 and wy0 <= y <= wy1:
        cr = W["corner_radius"]
        sr = sd_round_rect(x, y, wx0, wx1, wy0, wy1, cr)
        if sr <= 0.0:
            h = pit_floor(x, y)
    else:
        # berm outside the back (north/west) sides
        bx = x - wx1 if x > wx1 else 0.0
        by = wy0 - y if y < wy0 else 0.0
        if (bx > 0.0 or by > 0.0) and wx0 - 1 <= x <= wx1 + 5 and wy0 - 5 <= y <= wy1 + 1:
            dd = math.hypot(bx, by)
            h = lerp(max(h, W["back_bank_z"]), h, smoothstep(0.0, 4.5, dd))
    return h


# ---------------------------------------------------------------- weights
def coverages(x, y, h, slope):
    c = {}
    n1 = fbm(x / 9.0, y / 9.0, 707)
    n2 = fbm(x / 3.5, y / 3.5, 808)

    n3 = fbm(x / 6.0, y / 6.0, 909)

    # dry grass: wear bands beside paths + sunny patches on the lower hill margins only
    d, _ = nearest_path(x, y)
    dry = (1.0 - smoothstep(0.2, 1.4 + 0.6 * n2, d)) * 0.55 if d > -0.5 else 0.0
    hills = hills_height(x, y)
    dry = max(dry, smoothstep(0.15, 0.55, n1) * smoothstep(1.0, 3.0, hills) * (1.0 - smoothstep(3.5, 6.0, hills)) * 0.45)
    c["dry"] = dry

    # forest floor: hills belt + groves
    forest = smoothstep(2.0, 5.0, hills) * (0.75 + 0.25 * smoothstep(-0.3, 0.3, n1))
    for g in GROVES:
        dg = math.hypot(x - g["c"][0], y - g["c"][1]) - g["r"]
        forest = max(forest, 1.0 - smoothstep(-1.5, 1.5 + 0.8 * n2, dg))
    c["forest"] = forest

    # understory (hellebore) at forest edges, green ground-cover patches inside the woods, beds, wombat back bank
    under = forest * (1.0 - forest) * 2.2 * smoothstep(-0.2, 0.4, n2)
    under = max(under, forest * smoothstep(0.05, 0.4, n3) * 0.75)
    for b in BEDS:
        sb = sd_round_rect(x, y, b["x"][0], b["x"][1], b["y"][0], b["y"][1], 0.6)
        under = max(under, 1.0 - smoothstep(-0.3, 0.4 + 0.3 * n2, sb))
    c["under"] = clamp(under)

    # giraffe paddock: a grazed mix of green and dry grass, open sand where the animals gather,
    # trampled soil along the fence, on desire lines between features and under the trees
    s = sd_round_rect(x, y, pad_x0, pad_x1, pad_y0, pad_y1, G["paddock_corner_radius"])
    in_moat = moat["x"][0] <= x <= moat["x"][1] and moat["wall_y"] <= y <= pad_y0
    in_bay = kwall["x"][0] <= x <= kwall["x"][1] and kwall["y"] <= y <= pad_y0
    sand = 0.0
    soil = 0.0
    if in_moat or in_bay:
        sand = 1.0
        if in_moat and y <= moat["bottom_y"][1] + 0.6:
            soil = 1.0
    elif s < 0.5:
        inside = 1.0 - smoothstep(-0.6, 0.5, s)
        # grass mix: lawn stays as the base, dry grass takes roughly half in broad patches
        c["dry"] = max(c["dry"] * (1 - inside), inside * smoothstep(-0.2, 0.25, n1) * 0.9)
        # sand: the moat-side apron (the main viewing foreground) + gathering zones, with ragged noisy edges
        apron = 1.0 - smoothstep(5.0 + 3.0 * n2, 9.0 + 4.0 * n1, y - pad_y0)
        apron *= 1.0 - smoothstep(-6.0, 2.0, x - moat["x"][1] - 12.0)   # fades out east of the deck bay
        z = apron * 0.85
        for p, r in PADDOCK_SAND_ZONES:
            dp = math.hypot(x - p[0], y - p[1])
            z = max(z, 1.0 - smoothstep(r * 0.5, r * (1.0 + 0.3 * n2), dp))
        sand = clamp(z * (0.8 + 0.2 * smoothstep(-0.3, 0.3, n3))) * inside
        # trampled soil: pacing band along the fence (patchy), desire lines, under trees
        fence_band = smoothstep(-3.0 + 0.8 * n2, -1.0, s) * smoothstep(-0.35, 0.15, n3)
        soil = fence_band * 0.85
        for poly in PADDOCK_TRACKS:
            for a, b in zip(poly[:-1], poly[1:]):
                soil = max(soil, (1.0 - smoothstep(0.5, 1.5 + 0.6 * n2, seg_dist(x, y, a, b))) * 0.75)
        for p in G["trees"]:
            soil = max(soil, (1.0 - smoothstep(2.0, 4.5 + n2, math.hypot(x - p[0], y - p[1]))) * 0.8)
        wl = G["wallow"]
        soil = max(soil, 1.0 - smoothstep(wl["radius"] * 0.35, wl["radius"] * 0.8, math.hypot(x - wl["center"][0], y - wl["center"][1])))
        soil *= inside
    c["sand"] = sand

    # wombat pit: soil with leaf-litter patches; bank understory
    if wx0 <= x <= wx1 and wy0 <= y <= wy1 and sd_round_rect(x, y, wx0, wx1, wy0, wy1, W["corner_radius"]) <= 0:
        soil = max(soil, 0.85)
        c["forest"] = max(c["forest"], smoothstep(0.0, 0.5, n1) * 0.9)
        bank = max(1.0 - smoothstep(0.0, W["back_bank_width"], wx1 - x), 1.0 - smoothstep(0.0, W["back_bank_width"], y - wy0))
        c["under"] = max(c["under"], bank * 0.9)

    # pond: wet soil underwater and on the shoreline
    ps = pond_sdf(x, y)
    soil = max(soil, 1.0 - smoothstep(0.2, 1.2 + 0.6 * n2, ps))
    c["soil"] = soil

    # paths & pads
    pth = 1.0 - smoothstep(-0.15 + 0.2 * n2, 0.25 + 0.2 * n2, d)
    pz = L["plaza"]
    sp = sd_round_rect(x, y, pz["center"][0] - pz["half"][0], pz["center"][0] + pz["half"][0],
                       pz["center"][1] - pz["half"][1], pz["center"][1] + pz["half"][1], pz["radius"])
    pth = max(pth, 1.0 - smoothstep(-0.2, 0.3, sp))
    ra = L["rest_area"]
    sr = math.hypot(x - ra["center"][0], y - ra["center"][1]) - (ra["radius"] - 2.0)
    pth = max(pth, 1.0 - smoothstep(-0.2, 0.4 + 0.3 * n2, sr))
    ov = W["overlook"]
    so = sd_round_rect(x, y, ov["center"][0] - ov["size"][0] / 2, ov["center"][0] + ov["size"][0] / 2,
                       ov["center"][1] - ov["size"][1] / 2, ov["center"][1] + ov["size"][1] / 2, 0.5)
    pth = max(pth, 1.0 - smoothstep(-0.2, 0.2, so))
    if ps < 0.0 or (wx0 < x < wx1 and wy0 < y < wy1):
        pth = 0.0
    c["path"] = pth

    # rock on steep ground
    c["rock"] = smoothstep(0.7, 1.25, slope)
    return c


def paint(c):
    w = {"lawn": 1.0}
    for key in ["dry", "forest", "under", "sand", "soil", "path", "rock"]:
        v = clamp(c.get(key, 0.0))
        if v <= 0.0:
            continue
        for k in w:
            w[k] *= (1.0 - v)
        w[key] = w.get(key, 0.0) + v
    return w


# ---------------------------------------------------------------- PNG writer
def write_png(path, width, height, rows, bitdepth, color_type):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + r for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, bitdepth, color_type, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


# ---------------------------------------------------------------- main
def main():
    t0 = time.time()
    os.makedirs(OUT, exist_ok=True)
    H = [[0.0] * N for _ in range(N)]  # H[j][i] -> row j (Y), col i (X)
    for j in range(N):
        y = OY + j * MPP
        row = H[j]
        for i in range(N):
            row[i] = compute_height(OX + i * MPP, y)
    print("heights: %.1fs" % (time.time() - t0))

    # heightmap PNG (16-bit, big-endian)
    k = 128.0 / ZSCALE  # height units per cm
    rows = []
    for j in range(N):
        vals = [int(round(clamp(32768 + H[j][i] * 100.0 * k, 0, 65535))) for i in range(N)]
        rows.append(struct.pack(">%dH" % N, *vals))
    write_png(os.path.join(OUT, "zoo_height_505.png"), N, N, rows, 16, 0)

    # weights
    names = [ln for ln, _ in LAYERS]
    roles = [r for _, r in LAYERS]
    WM = {r: [bytearray(N) for _ in range(N)] for r in roles}
    for j in range(N):
        y = OY + j * MPP
        for i in range(N):
            x = OX + i * MPP
            i0, i1 = max(i - 1, 0), min(i + 1, N - 1)
            j0, j1 = max(j - 1, 0), min(j + 1, N - 1)
            gx = (H[j][i1] - H[j][i0]) / ((i1 - i0) * MPP)
            gy = (H[j1][i] - H[j0][i]) / ((j1 - j0) * MPP)
            slope = math.hypot(gx, gy)
            w = paint(coverages(x, y, H[j][i], slope))
            q = {r: int(w.get(r, 0.0) * 255.0) for r in roles}
            rem = 255 - sum(q.values())
            if rem:
                top = max(roles, key=lambda r: w.get(r, 0.0))
                q[top] += rem
            for r in roles:
                WM[r][j][i] = q[r]
    print("weights: %.1fs" % (time.time() - t0))
    for ln, r in LAYERS:
        write_png(os.path.join(OUT, "zoo_w_%s.png" % ln), N, N, [bytes(WM[r][j]) for j in range(N)], 8, 0)

    # preview: north up (row 0 = max X), east right (col = Y)
    colors = {"lawn": (92, 132, 58), "dry": (170, 160, 96), "forest": (70, 58, 40), "under": (52, 88, 44),
              "sand": (214, 186, 140), "soil": (120, 88, 60), "path": (190, 190, 180), "rock": (110, 110, 115)}
    prev = []
    for py in range(N):
        i = N - 1 - py  # X index
        rowb = bytearray()
        for jx in range(N):
            j = jx
            h = H[j][i]
            r = g = b = 0.0
            for ln, role in LAYERS:
                wv = WM[role][j][i] / 255.0
                cr, cg, cb = colors[role]
                r += cr * wv
                g += cg * wv
                b += cb * wv
            # hillshade (light from SW)
            i0, i1 = max(i - 1, 0), min(i + 1, N - 1)
            j0, j1 = max(j - 1, 0), min(j + 1, N - 1)
            gx = (H[j][i1] - H[j][i0]) / ((i1 - i0) * MPP)
            gy = (H[j1][i] - H[j0][i]) / ((j1 - j0) * MPP)
            shade = clamp(0.8 + (-gx * 0.7 - gy * 0.7) * 0.6, 0.35, 1.25)
            # water
            if pond_sdf(OX + i * MPP, OY + j * MPP) < 0 and h < P["water_z"]:
                r, g, b = 60, 110, 140
            r, g, b = r * shade, g * shade, b * shade
            # contour every 1 m
            if math.floor(h) != math.floor(H[j][max(i - 1, 0)]) or math.floor(h) != math.floor(H[max(j - 1, 0)][i]):
                r, g, b = r * 0.55, g * 0.55, b * 0.55
            rowb += bytes((int(clamp(r, 0, 255)), int(clamp(g, 0, 255)), int(clamp(b, 0, 255))))
        prev.append(rowb)

    clean = [bytearray(r) for r in prev]  # marker-free copy for the visitor map board

    def mark(xm, ym, col, rad=2):
        ci = int(round((xm - OX) / MPP))
        cj = int(round((ym - OY) / MPP))
        for dy in range(-rad, rad + 1):
            for dx in range(-rad, rad + 1):
                py = N - 1 - (ci + dy)
                px = cj + dx
                if 0 <= py < N and 0 <= px < N:
                    prev[py][px * 3:px * 3 + 3] = bytes(col)

    for nm, a in G["anchors"].items():
        mark(a[0], a[1], (255, 200, 0), 3)
    for nm, a in W["anchors"].items():
        mark(a[0], a[1], (255, 200, 0), 3)
    for nm, v in L["viewpoints"].items():
        mark(v["pos"][0], v["pos"][1], (255, 40, 40), 2)
    ps_ = L["player_start"]
    mark(ps_[0], ps_[1], (40, 120, 255), 4)
    dk = G["deck"]
    for xx in [dk["center"][0] - dk["size"][0] / 2, dk["center"][0] + dk["size"][0] / 2]:
        for yy in [dk["center"][1] - dk["size"][1] / 2, dk["center"][1] + dk["size"][1] / 2]:
            mark(xx, yy, (255, 255, 255), 1)
    br = P["bridge"]
    for t in range(0, 21):
        mark(lerp(br["a"][0], br["b"][0], t / 20), lerp(br["a"][1], br["b"][1], t / 20), (150, 90, 40), 1)
    write_png(os.path.join(OUT, "zoo_preview.png"), N, N, [bytes(r) for r in prev], 8, 2)

    # zoomed crop of the core (x3, nearest) for close review
    cx0, cx1 = -62.0, 50.0   # X range (south..north)
    cy0, cy1 = -64.0, 64.0   # Y range (west..east)
    z = 3
    for name, src_img in (("zoo_preview_core.png", prev), ("zoo_map_clean.png", clean)):
        crop = []
        for py in range(int((cx1 - cx0) / MPP) * z):
            xm = cx1 - (py / z) * MPP
            i = int((xm - OX) / MPP)
            src = src_img[N - 1 - i]
            rowb = bytearray()
            for px in range(int((cy1 - cy0) / MPP) * z):
                ym = cy0 + (px / z) * MPP
                j = int((ym - OY) / MPP)
                rowb += src[j * 3:j * 3 + 3]
            crop.append(bytes(rowb))
        write_png(os.path.join(OUT, name), len(crop[0]) // 3, len(crop), crop, 8, 2)

    # probes
    def hat(xm, ym):
        i = int(round((xm - OX) / MPP))
        j = int(round((ym - OY) / MPP))
        return round(H[j][i], 3)

    probes = {
        "plaza": hat(*L["plaza"]["center"]),
        "moat_bottom": hat(-12, 10.8),
        "moat_walk": hat(-12, 5.4),
        "knoll_top": hat(9, 5.0),
        "deck_ground": hat(*dk["center"]),
        "paddock_mid": hat(10, 34),
        "pond_centre": hat(23.5, -12),
        "pit_floor": hat(*W["anchors"]["WOM_Anchor_Open"]),
        "pit_path": hat(16.8, -43.5),
        "rest_area": hat(*L["rest_area"]["center"]),
        "north_hill": hat(90, 0),
        "east_hill": hat(0, 95),
    }
    probes["_heights_min_max"] = [round(min(min(r) for r in H), 2), round(max(max(r) for r in H), 2)]
    json.dump(probes, open(os.path.join(OUT, "zoo_probe.json"), "w"), indent=1)
    print(json.dumps(probes, indent=1))
    print("done: %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
