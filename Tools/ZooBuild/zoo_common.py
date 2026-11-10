"""Shared helpers for the L_Zoo build planner: layout, terrain height lookup, sampling, output."""
import json
import math
import os
import random
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TERRAIN = os.path.join(HERE, "..", "ZooTerrain")
sys.path.insert(0, TERRAIN)
import gen_zoo_terrain as gt  # noqa: E402  (reuses layout + SDF helpers; main() is not run on import)

L = gt.L
OUT_DIR = os.path.abspath(os.path.join(HERE, "..", "..", "Saved", "ZooBuild"))
os.makedirs(OUT_DIR, exist_ok=True)


# ---------------------------------------------------------------- heightmap
def _load_heights():
    path = os.path.join(TERRAIN, "out", "zoo_height_505.png")
    data = open(path, "rb").read()
    pos = 8
    idat = b""
    w = h = 0
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        pos += 12 + ln
    raw = zlib.decompress(idat)
    stride = 1 + w * 2
    k = gt.ZSCALE / 128.0  # cm per unit
    H = []
    for j in range(h):
        row = raw[j * stride + 1:(j + 1) * stride]
        vals = struct.unpack(">%dH" % w, row)
        H.append([(v - 32768) * k / 100.0 for v in vals])  # metres
    return H


HGT = _load_heights()
N = gt.N
MPP = gt.MPP
OX, OY = gt.OX, gt.OY


def ground(x, y):
    """Terrain height (m) at world metres, bilinear on the imported heightmap."""
    fi = (x - OX) / MPP
    fj = (y - OY) / MPP
    i0 = max(0, min(N - 2, int(math.floor(fi))))
    j0 = max(0, min(N - 2, int(math.floor(fj))))
    tx = min(max(fi - i0, 0.0), 1.0)
    ty = min(max(fj - j0, 0.0), 1.0)
    a = HGT[j0][i0]
    b = HGT[j0][i0 + 1]
    c = HGT[j0 + 1][i0]
    d = HGT[j0 + 1][i0 + 1]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def slope(x, y, e=0.5):
    gx = (ground(x + e, y) - ground(x - e, y)) / (2 * e)
    gy = (ground(x, y + e) - ground(x, y - e)) / (2 * e)
    return math.hypot(gx, gy)


# ---------------------------------------------------------------- layout queries
def path_edge_dist(x, y):
    return gt.nearest_path(x, y)[0]


def in_paddock(x, y, margin=0.0):
    return gt.sd_round_rect(x, y, gt.pad_x0, gt.pad_x1, gt.pad_y0, gt.pad_y1, L["giraffe"]["paddock_corner_radius"]) < -margin


def paddock_sdf(x, y):
    return gt.sd_round_rect(x, y, gt.pad_x0, gt.pad_x1, gt.pad_y0, gt.pad_y1, L["giraffe"]["paddock_corner_radius"])


def in_wombat(x, y, margin=0.0):
    return gt.sd_round_rect(x, y, gt.wx0, gt.wx1, gt.wy0, gt.wy1, L["wombat"]["corner_radius"]) < -margin


def pond_sdf(x, y):
    return gt.pond_sdf(x, y)


def core_out(x, y):
    """Distance outside the core rectangle (0 inside)."""
    cx0, cx1 = L["core"]["x"]
    cy0, cy1 = L["core"]["y"]
    dx = max(cx0 - x, 0.0, x - cx1)
    dy = max(cy0 - y, 0.0, y - cy1)
    return math.hypot(dx, dy)


def in_animal_or_water(x, y, margin=1.0):
    g = L["giraffe"]
    if paddock_sdf(x, y) < margin:
        return True
    mx0, mx1 = g["moat"]["x"]
    if mx0 - margin <= x <= mx1 + margin and g["moat"]["wall_y"] - margin <= y <= gt.pad_y0:
        return True
    kx0, kx1 = g["knoll_wall"]["x"]
    if kx0 - margin <= x <= kx1 + margin and g["knoll_wall"]["y"] - margin <= y <= gt.pad_y0:
        return True
    if gt.sd_round_rect(x, y, gt.wx0, gt.wx1, gt.wy0, gt.wy1, L["wombat"]["corner_radius"]) < margin:
        return True
    if pond_sdf(x, y) < margin * 0.5:
        return True
    return False


def on_pad(x, y, margin=0.5):
    pz = L["plaza"]
    s = gt.sd_round_rect(x, y, pz["center"][0] - pz["half"][0], pz["center"][0] + pz["half"][0],
                         pz["center"][1] - pz["half"][1], pz["center"][1] + pz["half"][1], pz["radius"])
    if s < margin:
        return True
    ra = L["rest_area"]
    if math.hypot(x - ra["center"][0], y - ra["center"][1]) < ra["radius"] - 1.5 + margin:
        return True
    return False


# ---------------------------------------------------------------- sampling
def poisson(rng, x0, x1, y0, y1, rmin, accept, tries=30, seeds=None, max_pts=100000):
    """Bridson Poisson-disk sampling inside a box, filtered by accept(x, y)."""
    cell = rmin / math.sqrt(2)
    gw = int((x1 - x0) / cell) + 1
    gh = int((y1 - y0) / cell) + 1
    grid = {}
    pts = []
    active = []

    def fits(px, py):
        gi, gj = int((px - x0) / cell), int((py - y0) / cell)
        for di in range(-2, 3):
            for dj in range(-2, 3):
                q = grid.get((gi + di, gj + dj))
                if q and (q[0] - px) ** 2 + (q[1] - py) ** 2 < rmin * rmin:
                    return False
        return True

    def add(px, py):
        pts.append((px, py))
        active.append((px, py))
        grid[(int((px - x0) / cell), int((py - y0) / cell))] = (px, py)

    starts = seeds or []
    for _ in range(200):
        starts.append((rng.uniform(x0, x1), rng.uniform(y0, y1)))
    for sx, sy in starts:
        if accept(sx, sy) and fits(sx, sy):
            add(sx, sy)
    while active and len(pts) < max_pts:
        idx = rng.randrange(len(active))
        ax, ay = active[idx]
        found = False
        for _ in range(tries):
            ang = rng.uniform(0, 2 * math.pi)
            rad = rng.uniform(rmin, 2 * rmin)
            px, py = ax + rad * math.cos(ang), ay + rad * math.sin(ang)
            if x0 <= px <= x1 and y0 <= py <= y1 and accept(px, py) and fits(px, py):
                add(px, py)
                found = True
                break
        if not found:
            active.pop(idx)
    return pts


# ---------------------------------------------------------------- output model
class Plan:
    """Collects hero actors and ISM groups, then writes one JSON file per stage."""

    def __init__(self, stage):
        self.stage = stage
        self.hero = []
        self.groups = {}
        self.skip = None  # optional skip(group, x, y): instances it returns True for are dropped

    def hero_actor(self, label, asset, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, scale=(1, 1, 1), folder="", mat=None,
                   comp_props=None, actor_class=None, tags=None):
        self.hero.append({
            "label": label, "asset": asset, "class": actor_class,
            "loc": [round(x * 100, 1), round(y * 100, 1), round(z * 100, 1)],
            "rot": [round(pitch, 2), round(yaw, 2), round(roll, 2)],
            "scale": [round(s, 4) for s in scale], "folder": folder, "mat": mat,
            "comp_props": comp_props or {}, "tags": tags or [],
        })

    def inst(self, group, folder, mesh, x, y, z, yaw=0.0, pitch=0.0, roll=0.0, scale=(1, 1, 1), comp_props=None):
        if self.skip and self.skip(group, x, y):
            return
        g = self.groups.setdefault(group, {"label": group, "folder": folder, "meshes": {}, "comp_props": {}})
        m = g["meshes"].setdefault(mesh, [])
        m.append([round(x * 100, 1), round(y * 100, 1), round(z * 100, 1), round(pitch, 2), round(yaw, 2), round(roll, 2),
                  round(scale[0], 4), round(scale[1], 4), round(scale[2], 4)])
        if comp_props:
            g["comp_props"].update(comp_props)

    def group_props(self, group, folder, props):
        g = self.groups.setdefault(group, {"label": group, "folder": folder, "meshes": {}, "comp_props": {}})
        g["comp_props"].update(props)

    MAX_BYTES = 70000  # the in-editor read_file tool refuses files above 80 KB

    def write(self):
        """Writes plan_<stage>_NN.json parts (each < MAX_BYTES) and plan_<stage>_index.json listing them in order.
        A group too large for one part is split by mesh; later parts carry "append": true and a mesh index offset."""
        for old in os.listdir(OUT_DIR):
            if old.startswith("plan_%s" % self.stage) and old.endswith(".json"):
                os.remove(os.path.join(OUT_DIR, old))
        parts = []
        cur = {"stage": self.stage, "hero": list(self.hero), "groups": []}

        def size(obj):
            return len(json.dumps(obj, separators=(",", ":")))

        def flush():
            nonlocal cur
            if cur["hero"] or cur["groups"]:
                parts.append(cur)
            cur = {"stage": self.stage, "hero": [], "groups": []}

        for g in self.groups.values():
            meshes = [{"mesh": k, "inst": v} for k, v in g["meshes"].items()]
            chunk = []
            offset = 0
            first = True
            for i, m in enumerate(meshes):
                trial = chunk + [m]
                entry = {"label": g["label"], "folder": g["folder"], "comp_props": g["comp_props"], "meshes": trial,
                         "append": not first, "mesh_offset": offset}
                if chunk and size(cur) + size(entry) > self.MAX_BYTES:
                    cur["groups"].append({"label": g["label"], "folder": g["folder"], "comp_props": g["comp_props"],
                                          "meshes": chunk, "append": not first, "mesh_offset": offset})
                    flush()
                    first = False
                    offset = i
                    chunk = [m]
                else:
                    chunk = trial
            if chunk:
                entry = {"label": g["label"], "folder": g["folder"], "comp_props": g["comp_props"], "meshes": chunk,
                         "append": not first, "mesh_offset": offset}
                if size(cur) + size(entry) > self.MAX_BYTES:
                    flush()
                cur["groups"].append(entry)
        flush()
        names = []
        for k, p in enumerate(parts):
            name = "plan_%s_%02d.json" % (self.stage, k)
            with open(os.path.join(OUT_DIR, name), "w") as f:
                json.dump(p, f, separators=(",", ":"))
            names.append(name)
        with open(os.path.join(OUT_DIR, "plan_%s_index.json" % self.stage), "w") as f:
            json.dump({"stage": self.stage, "parts": names}, f)
        n_inst = sum(len(v) for g in self.groups.values() for v in g["meshes"].values())
        print("%s: %d hero, %d groups, %d instances -> %d part(s)" % (self.stage, len(self.hero), len(self.groups), n_inst, len(names)))
        return names


def mesh(path):
    """'/Game/a/b/SM_X' -> '/Game/a/b/SM_X.SM_X'"""
    return path if "." in path.split("/")[-1] else path + "." + path.split("/")[-1]


def seg_points(ax, ay, bx, by, spacing):
    d = math.hypot(bx - ax, by - ay)
    n = max(1, int(round(d / spacing)))
    return [(ax + (bx - ax) * t / n, ay + (by - ay) * t / n) for t in range(n + 1)]


def yaw_of(ax, ay, bx, by):
    return math.degrees(math.atan2(by - ay, bx - ax))
