"""L_Zoo build planner. Writes Saved/ZooBuild/plan_<stage>.json consumed by the in-editor applier.

Usage (UE bundled Python):  python plan_zoo.py [stage ...]     (no args = all stages)
Stages: giraffe, wombat, pond, visitor, veg, bounds
Coordinates: metres, +X north, +Y east. UE yaw 0 faces +X.
"""
import math
import random
import sys

from zoo_common import (L, Plan, gt, ground, slope, path_edge_dist, paddock_sdf, in_animal_or_water, on_pad, pond_sdf,
                        core_out, poisson, mesh, seg_points, yaw_of)

G = L["giraffe"]
W = L["wombat"]
P = L["pond"]
E = L["entrance"]

# ------------------------------------------------------------------ assets
WP = "/Game/Marketplace/WoodenProps/StaticMeshes/SingleParts/"
WPC = "/Game/Marketplace/WoodenProps/StaticMeshes/Complex/"
CP = "/Game/Marketplace/CityPark/Meshes/"
QS = "/Game/Marketplace/Scene_QuarrySlate/Assets/MS/3D/"
QP = "/Game/Marketplace/Scene_QuarrySlate/Assets/MS/3D_Plants/"
MAE = "/Game/Marketplace/MAEOakForest/Meshes/Environments/"
BA = "/Game/Marketplace/BlackAlder/Geometry/PivotPainter/"
HB = "/Game/Marketplace/EuropeanHornbeam/Geometry/PivotPainter/"
HZ = "/Game/Marketplace/CommonHazel/Geometry/PivotPainter/"


def qs(n):
    return mesh(QS + n + "/SM_" + n)


PILLARLOG = mesh(WP + "PillarLog1")
PILLARLOG3 = mesh(WP + "PillarLog3")
SINGLELOG = mesh(WP + "SingleLog1")
FENCE2 = mesh(WP + "Fence2_1")
BARRICADE = mesh(WP + "Barricade1")
WOODPILLAR2 = mesh(WP + "SingleWoodenPillar2")
PLATFORM5 = mesh(WP + "Platform5")
PLATFORM3 = mesh(WP + "Platform3")
PLATFORM1 = mesh(WPC + "Platform1_Main")
CAGE = mesh(WP + "Cage")
BARREL = mesh(WP + "Barrel1")
BOX = mesh(WP + "Box1")
BUCKET = mesh(WP + "Bucket")
LADDER = mesh(WP + "Ladder")
BRIDGE = mesh(WP + "Bridge")
BALK = mesh(WP + "Balk1")

LEDGE_L = qs("Qua_Sla_Ledge_Rock_L_01")
LEDGE_M1 = qs("Qua_Sla_Ledge_Rock_M_01")
LEDGE_M3 = qs("Qua_Sla_Ledge_Rock_M_03")
LEDGE_S1 = qs("Qua_Sla_Ledge_Rock_S_01")
CL_LEDGE = qs("Qua_Sla_Cluster_Ledge_Rock_M_01")
CL_M6 = qs("Qua_Sla_Cluster_Rock_M_06")
CL_M7 = qs("Qua_Sla_Cluster_Rock_M_07")
CL_S2 = qs("Qua_Sla_Cluster_Rock_S_02")
CL_S5 = qs("Qua_Sla_Cluster_Rock_S_05")
ROCK_M1 = qs("Qua_Sla_Rock_M_01")
ROCK_M6 = qs("Qua_Sla_Rock_M_06")
ROCK_S16 = qs("Qua_Sla_Rock_S_16")
PILE_S1 = qs("Qua_Sla_Pile_Rock_S_01")
PATCH_TRACKS = qs("Qua_Sla_Patch_Tracks_Dirt_Coarse_01")
PATCH_MUD = qs("Qua_Sla_Patch_Mound_Mud_Dried_03")
PATCH_GRAVEL = qs("Qua_Sla_Patch_Gravel_Coarse_01")
TUSSOCKS = [mesh(QP + "Qua_Lim_Grass_Thatching_Common_Set_01/SM_Qua_Lim_Grass_Thatching_Common_Set_01_" + c) for c in "CEFKG"]
SPINDLE = [mesh(QP + "Tun_Nor_Bush_EuropeanSpindle_Set_01/SM_Tun_Nor_Bush_EuropeanSpindle_Set_01_" + c) for c in "CDEFG"]

OAKS = [mesh(MAE + "Trees/SM_Oak_Tree_" + c) for c in "abcdef"]
OAK_SAPLINGS = [mesh(MAE + "Trees/SM_Oak_Sapling_" + c) for c in "ab"]
MOSSY = [mesh(MAE + "Rocks/SM_Mossy_Rock_" + c) for c in "bcde"]
MAE_ROCKS = [mesh(MAE + "Rocks/SM_Rock_" + c) for c in "abc"]
LOGS = [mesh(MAE + "Logs/SM_Mossy_Log_a"), mesh(MAE + "Logs/SM_Oak_Log_b"), mesh(MAE + "Logs/SM_Oak_Log_c"), mesh(MAE + "Logs/SM_Oak_Log_d")]
FERNS = [mesh(MAE + "GroundPlants/Male_Fern/SM_Male_Fern_" + c) for c in "ab"]
HELLEBORE = [mesh(MAE + "GroundPlants/Green_Hellebore/SM_Green_Hellebore_a")]
LEAVES = [mesh(MAE + "Covers/SM_Dried_Oak_Leaves_" + c) for c in "ab"]
STICKS = [mesh(MAE + "Sticks/SM_Oak_Stick_a"), mesh(CP + "Ground/SM_StickDebris02"), mesh(CP + "Ground/SM_StickDebris03")]
WINTERBERRY = [mesh(MAE + "Bushes/Common_Winterberry/SM_Winterberry_" + c) for c in "ab"]
PRIVET = [mesh(MAE + "Bushes/Mock_Privet/SM_MockPrivet_a")]
CP_ROCKS = [mesh(CP + "Ground/SM_Rock0" + c) for c in "12345"]
CP_TALLGRASS = [mesh(CP + "Flora/Grass/SM_grass0" + c) for c in "234"] + [mesh(CP + "Flora/Grass/SM_grass09_5")]
CP_LOWGRASS = [mesh(CP + "Flora/Grass/SM_grass09_2"), mesh(CP + "Flora/Grass/SM_grass09_3"), mesh(CP + "Flora/Grass/SM_Grass13_1"), mesh(CP + "Flora/Grass/SM_Grass13_2")]
LILIES = [mesh(CP + "Flora/Grass/SM_WaterPlant0" + c) for c in "1234"]
CP_LEAVES = [mesh(CP + "Ground/SM_DryLeaves0" + c) for c in "2345"]

BA_FOREST = [mesh(BA + "SM_BlackAlder_Forest_%02d_PP" % i) for i in range(1, 10)]
BA_FIELD = [mesh(BA + "SM_BlackAlder_Field_%02d_PP" % i) for i in range(1, 6)]
HB_FOREST = [mesh(HB + "SM_EuropeanHornbeam_Forest_%02d_PP" % i) for i in range(1, 11)]
HB_FIELD = [mesh(HB + "SM_EuropeanHornbeam_Field_%02d_PP" % i) for i in range(1, 5)]
HB_SAPLING = [mesh(HB + "SM_EuropeanHornbeam_Sapling_%02d_PP" % i) for i in range(1, 9)]
HZ_FOREST = [mesh(HZ + "SM_CommonHazel_Forest_%02d_PP" % i) for i in range(1, 5)]
HZ_SAPLING = [mesh(HZ + "SM_CommonHazel_Sapling_%02d_PP" % i) for i in range(1, 5)]
HZ_SEEDLING = [mesh(HZ + "SM_CommonHazel_Seedling_%02d_PP" % i) for i in range(1, 5)]

LAMP = mesh(CP + "Props/SM_LampPost01")
BENCH = mesh(CP + "Props/SM_Bench03")
BIN = mesh(CP + "Props/SM_TrashCan02")
FOUNTAIN = mesh(CP + "Props/SM_DrinkingFountain")
BOWER = mesh(CP + "Props/SM_Bower02")
PIER = mesh(CP + "Props/SM_Pier01")
PIER_LEGS = mesh(CP + "Props/SM_Pier02")
PIER_RAIL = mesh(CP + "Props/SM_Pier03")
WATER = mesh(CP + "Ground/SM_WaterPlane")
WILLOW = mesh(CP + "Flora/Trees/SM_WeepingWillow01")
CUBE = "/Engine/BasicShapes/Cube.Cube"

GATE = "/Game/Zoo/Assets/Gate/Meshes/"
TUSK = mesh(GATE + "SM_Gate_Tusk")          # base block 5.7 x 5.6 m; tip 20 m up, sweeping 14.2 m toward local -X
PENDOPO = mesh(GATE + "SM_Gate_Pendopo")    # 7.7 x 8.0 x 7.5 m
PAPAN = mesh(GATE + "SM_Papan_Pintu_Masuk")  # 'Let's Go Safari' board with ladder posts, ~16.9 m wide x 12 m tall (baked 1.5x so it dwarfs the pendopo); width runs along local X, art faces local +Y (yaw 90 turns it to face the approach, -X)
STATUE_FRONT_YAW = 90.0  # the statue plaques face local +Y (the animal stands in profile along local X)

FOLIAGE_PROPS = {"CastShadow": True, "InstanceStartCullDistance": 0, "InstanceEndCullDistance": 0}
WPO_NEAR = {"WorldPositionOffsetDisableDistance": 6000}


def jitter(rng, a):
    return rng.uniform(-a, a)


def rounded_rect_loop(x0, x1, y0, y1, r, spacing):
    """Ordered points around a rounded rectangle (counter-clockwise from the SW straight on the south side)."""
    pts = []
    corners = [(x0 + r, y0 + r, math.pi), (x1 - r, y0 + r, -math.pi / 2), (x1 - r, y1 - r, 0.0), (x0 + r, y1 - r, math.pi / 2)]
    # edges: south (x=x0) from y0+r..y1-r is the WEST? keep generic: walk the boundary parametric
    per = 2 * (x1 - x0 - 2 * r) + 2 * (y1 - y0 - 2 * r) + 2 * math.pi * r
    n = int(per / spacing)
    out = []
    for k in range(n):
        s = per * k / n
        segs = [
            ("line", (x0, y0 + r), (x0, y1 - r)),                       # x = x0 (south side), y increasing
            ("arc", (x0 + r, y1 - r), math.pi, math.pi / 2),           # SE corner
            ("line", (x0 + r, y1), (x1 - r, y1)),                       # y = y1 (east side)
            ("arc", (x1 - r, y1 - r), math.pi / 2, 0.0),               # NE corner
            ("line", (x1, y1 - r), (x1, y0 + r)),                       # x = x1 (north side)
            ("arc", (x1 - r, y0 + r), 0.0, -math.pi / 2),              # NW corner
            ("line", (x1 - r, y0), (x0 + r, y0)),                       # y = y0 (west side)
            ("arc", (x0 + r, y0 + r), -math.pi / 2, -math.pi),         # SW corner
        ]
        rem = s
        for sg in segs:
            if sg[0] == "line":
                (ax, ay), (bx, by) = sg[1], sg[2]
                ln = math.hypot(bx - ax, by - ay)
                if rem <= ln:
                    t = rem / ln if ln else 0
                    out.append((ax + (bx - ax) * t, ay + (by - ay) * t))
                    break
                rem -= ln
            else:
                (cx, cy), a0, a1 = sg[1], sg[2], sg[3]
                ln = abs(a1 - a0) * r
                if rem <= ln:
                    a = a0 + (a1 - a0) * (rem / ln)
                    out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
                    break
                rem -= ln
    return out


def add_rail(plan, group, folder, ax, ay, az, bx, by, bz, mesh_path, base_len, thick):
    ln = math.hypot(bx - ax, by - ay)
    yaw = yaw_of(ax, ay, bx, by)
    pitch = math.degrees(math.atan2(bz - az, ln))
    plan.inst(group, folder, mesh_path, (ax + bx) / 2, (ay + by) / 2, (az + bz) / 2, yaw=yaw, pitch=pitch,
              scale=(ln / base_len * 1.03, thick, thick))


def visitor_rail(plan, ax, ay, bx, by, z_fixed=None, seg=3.3, zscale=0.8):
    """Rustic 3-rail visitor fence (WoodenProps Fence2_1) following the ground or a fixed height."""
    pts = seg_points(ax, ay, bx, by, seg)
    for (px, py), (qx, qy) in zip(pts[:-1], pts[1:]):
        ln = math.hypot(qx - px, qy - py)
        za = z_fixed if z_fixed is not None else ground(px, py)
        zb = z_fixed if z_fixed is not None else ground(qx, qy)
        yaw = yaw_of(px, py, qx, qy)
        pitch = math.degrees(math.atan2(zb - za, ln))
        cx, cy = (px + qx) / 2, (py + qy) / 2
        plan.inst("PATH_VisitorRails", "Paths", FENCE2, cx, cy, (za + zb) / 2 - 0.03, yaw=yaw, pitch=pitch,
                  scale=(ln / 3.34, 1.0, zscale))


# full (closed) slate boulders: mesh -> (length X, width Y, height Z) in metres at scale 1.
# The Quarry "Ledge" pieces are open-backed cliff facings and the "Cluster" pieces carry a pale ground plate,
# so free-standing rocks only use these.
BOULDER_DIMS = {ROCK_M6: (1.1, 0.6, 0.8), ROCK_M1: (1.4, 0.8, 0.5), ROCK_S16: (2.4, 1.4, 0.6)}


def boulder(plan, group, folder, m, x, y, yaw, sc, bury, pitch=0.0, roll=0.0):
    """Places a boulder sunk `bury` (fraction of its height) below the LOWEST ground under its footprint."""
    ln, wd, ht = BOULDER_DIMS[m]
    a = math.radians(yaw)
    ux, uy = math.cos(a) * ln * sc / 2, math.sin(a) * ln * sc / 2
    vx, vy = -math.sin(a) * wd * sc / 2, math.cos(a) * wd * sc / 2
    gmin = min(ground(x + i * ux + j * vx, y + i * uy + j * vy) for i in (-1, 0, 1) for j in (-1, 0, 1))
    plan.inst(group, folder, m, x, y, gmin - bury * ht * sc, yaw=yaw, pitch=pitch, roll=roll, scale=(sc, sc, sc))


PLANK6 = mesh(WP + "Plank6")   # flat board 0.24 x 3.00 x 0.09 m (bounds x -12..12, y -152..148, z 1..10 cm)
PLANK7 = mesh(WP + "Plank7")   # flat board 0.19 x 2.95 x 0.08 m (bounds x -10..9, y -148..147, z 1..9 cm)
BOARDS = [(PLANK7, 0.19, 2.95, -0.005, 0.09), (PLANK6, 0.24, 3.00, -0.02, 0.10)]  # mesh, width, length, y-centre offset, top


def deck_boards(plan, rng, x0, x1, y0, y1, top_z, group="GIR_DeckBoards", folder="Giraffe/Deck"):
    """Clean decking: boards run along Y in rows across X, alternating widths, joints staggered row to row."""
    x = x0
    k = 0
    while x < x1 - 0.1:
        m, w, ln, yoff, tz = BOARDS[k % 2]
        w = min(w, x1 - x)
        cxr = x + w / 2
        # staggered joints: first board length varies per row, then full boards, last one trimmed to fit
        y = y0
        seg = ln * rng.uniform(0.35, 1.0)
        n_in_row = 0
        while y < y1 - 0.02:
            L_ = min(seg, y1 - y)
            if y1 - (y + L_) < 0.5:
                L_ = y1 - y                      # absorb a short remainder into this board
            last = y + L_ >= y1 - 1e-6
            # the board ends are rounded/chipped: overlap each joint by 14 cm (alternating 3 mm lift) so no gap shows
            draw = L_ + (0.0 if last else 0.14)
            sy = draw / ln
            yaw = 0.0 if rng.random() < 0.5 else 180.0
            lift = 0.003 if n_in_row % 2 else 0.0
            plan.inst(group, folder, m, cxr, y + draw / 2 - yoff * sy * (1 if yaw == 0 else -1), top_z - tz + lift,
                      yaw=yaw, roll=jitter(rng, 0.4), scale=(w / BOARDS[k % 2][1], sy, 1.0))
            y += L_
            seg = ln
            n_in_row += 1
        x += w + 0.012
        k += 1


# ================================================================== GIRAFFE
def stage_giraffe():
    plan = Plan("giraffe")
    rng = random.Random(1)
    F_BAR, F_DECK, F_HAB = "Giraffe/Barrier", "Giraffe/Deck", "Giraffe/Habitat"

    # ---- tall timber paddock fence (posts + 3 rails), open along the moat and deck bay
    loop = rounded_rect_loop(gt.pad_x0, gt.pad_x1, gt.pad_y0, gt.pad_y1, G["paddock_corner_radius"], 3.2)
    kept = []
    for (x, y) in loop:
        if y < gt.pad_y0 + 0.6 and -26.3 < x < 16.0:
            kept.append(None)
            continue
        kept.append((x, y))
    # closures: across the moat's south end, and between the deck bay and the fence (x=16)
    closure = seg_points(-26.8, 7.8, -26.8, 14.3, 3.2)
    runs = []
    cur = []
    for p in kept + kept[:1]:
        if p is None:
            if cur:
                runs.append(cur)
            cur = []
        else:
            cur.append(p)
    if cur:
        runs.append(cur)
    runs.append(closure)
    gate_center = (29.0, gt.pad_y1)
    for run in runs:
        for i, (x, y) in enumerate(run):
            gz = ground(x, y)
            plan.inst("GIR_Fence", F_BAR, PILLARLOG, x, y, gz - 0.75, yaw=rng.uniform(0, 360),
                      scale=(0.8, 0.8, 1.0 + jitter(rng, 0.03)))
            if i + 1 < len(run):
                qx, qy = run[i + 1]
                if math.hypot(qx - x, qy - y) > 4.5:
                    continue
                if math.hypot((x + qx) / 2 - gate_center[0], (y + qy) / 2 - gate_center[1]) < 2.0:
                    continue  # keeper gate opening
                qz = ground(qx, qy)
                for h in (0.75, 1.55, 2.35):
                    add_rail(plan, "GIR_Fence", F_BAR, x, y, gz + h, qx, qy, qz + h, SINGLELOG, 3.41, 0.45)
    # keeper gate (Balk1) in the east fence
    plan.hero_actor("GIR_KeeperGate", BALK, gate_center[0], gate_center[1] + 0.1, ground(*gate_center) - 0.05, yaw=90,
                    scale=(0.8, 0.62, 0.6), folder=F_BAR)

    # ---- moat: rock-faced visitor wall (courses of slate ledges)
    mx0, mx1 = G["moat"]["x"]
    x = mx0 + 1.2
    i = 0
    while x < mx1 - 0.5:
        top = ground(x, 9.4)
        plan.inst("GIR_MoatRocks", F_BAR, LEDGE_L, x + jitter(rng, 0.4), 10.55 + jitter(rng, 0.15), -2.95,
                  yaw=(0 if i % 2 else 180) + jitter(rng, 6), pitch=jitter(rng, 3), roll=jitter(rng, 3),
                  scale=(0.95 + jitter(rng, 0.08), 0.9, 1.0 + jitter(rng, 0.08)))
        x2 = x + 2.15
        plan.inst("GIR_MoatRocks", F_BAR, LEDGE_M1, x2 + jitter(rng, 0.3), 10.3 + jitter(rng, 0.15), -1.15 + jitter(rng, 0.1),
                  yaw=(180 if i % 2 else 0) + jitter(rng, 8), pitch=jitter(rng, 4), roll=jitter(rng, 4),
                  scale=(0.95 + jitter(rng, 0.1), 0.85, 1.0 + jitter(rng, 0.1)))
        if top > 0.35:
            plan.inst("GIR_MoatRocks", F_BAR, LEDGE_M3, x + 1.0 + jitter(rng, 0.3), 10.1, top - 0.72,
                      yaw=jitter(rng, 10) + (0 if i % 2 else 180), roll=jitter(rng, 4), scale=(1.0, 0.8, 0.95))
        if top > 1.0:
            plan.inst("GIR_MoatRocks", F_BAR, LEDGE_M3, x2 + 0.6, 10.0, top - 0.55, yaw=jitter(rng, 12), scale=(0.9, 0.7, 0.8))
        x += 4.3
        i += 1
    # a few fallen boulders in the moat bottom
    for bx, m in ((-21.0, ROCK_M6), (-9.5, ROCK_M1), (-1.5, ROCK_M6)):
        boulder(plan, "GIR_MoatRocks", F_BAR, m, bx, 11.2, rng.uniform(0, 360), 0.9, 0.25)

    # ---- knoll retaining palisade (under/around the deck) + closure at x=16
    kz = lambda px: ground(px, 9.9)
    panels = [(3.5, 5.6, kz(4.5) + 1.0), (5.6, 9.5, 1.30), (9.5, 13.4, 1.30), (13.4, 16.1, kz(14.7) + 1.0)]
    for a, b, top in panels:
        base = min(ground((a + b) / 2, 10.9), ground((a + b) / 2, 12.0)) - 0.45
        plan.inst("GIR_Palisade", F_BAR, BARRICADE, (a + b) / 2, 10.45, base, yaw=90,
                  scale=(1.0, (b - a) / 3.62, (top - base) / 3.45))
    base = ground(15.6, 12.2) - 0.45
    top = max(ground(16.6, 12.2), 0.4) + 1.0
    plan.inst("GIR_Palisade", F_BAR, BARRICADE, 16.1, 12.15, base, yaw=0, scale=(1.0, 3.9 / 3.62, (top - base) / 3.45))

    # ---- feeding deck: plank floor on log posts, flush with the knoll top
    dk = G["deck"]
    cx, cy = dk["center"]
    floor_top = 1.82
    # Platform5's own boards are deliberately loose/crossed, so it sits 22 cm lower as the sub-frame
    # and a clean staggered board surface is laid on top at floor_top
    plan.hero_actor("GIR_Deck_Floor", PLATFORM5, cx + 0.11, cy + 0.26, floor_top - 0.46 - 0.22, folder=F_DECK)
    deck_boards(plan, rng, 5.72, 13.28, 9.75, 17.25, floor_top)
    posts = [(6.1, 13.4), (9.5, 13.4), (12.9, 13.4), (6.1, 16.9), (9.5, 16.9), (12.9, 16.9)]
    for (px, py) in posts:
        gz = ground(px, py)
        base = gz - 0.4
        plan.inst("GIR_DeckFrame", F_DECK, PILLARLOG, px, py, base, yaw=rng.uniform(0, 360),
                  scale=(0.85, 0.85, (floor_top - 0.64 - base) / 3.33))
    for (a, b) in [((6.1, 16.9), (9.5, 16.9)), ((12.9, 16.9), (9.5, 16.9)), ((6.1, 13.4), (6.1, 16.9)), ((12.9, 13.4), (12.9, 16.9))]:
        az = ground(*a) + 0.35
        add_rail(plan, "GIR_DeckFrame", F_DECK, a[0], a[1], az, b[0], b[1], floor_top - 0.6, SINGLELOG, 3.41, 0.5)
    # deck railing (N, E, S sides; the west side opens onto the knoll)
    visitor_rail(plan, 5.8, 17.2, 13.2, 17.2, z_fixed=floor_top, seg=3.7, zscale=0.78)
    visitor_rail(plan, 13.2, 9.9, 13.2, 17.2, z_fixed=floor_top, seg=3.7, zscale=0.78)
    visitor_rail(plan, 5.8, 9.9, 5.8, 17.2, z_fixed=floor_top, seg=3.7, zscale=0.78)
    # moat visitor rail (planting strip lies between rail and moat edge)
    visitor_rail(plan, -26.5, 7.55, 3.3, 7.55)
    visitor_rail(plan, 3.3, 7.55, 3.3, 9.7)

    # ---- browse feeders (tall bundled-log poles topped with hazel browse)
    for (fx, fy) in [(12.0, 21.0), (-9.0, 24.5)]:
        gz = ground(fx, fy)
        plan.inst("GIR_Feeders", F_HAB, PILLARLOG3, fx, fy, gz - 0.4, yaw=rng.uniform(0, 360), scale=(0.55, 0.55, 1.5))
        for r_ in (-22, 18):
            plan.inst("GIR_Feeders", F_HAB, HZ_SAPLING[1], fx + 0.1, fy, gz + 4.1, yaw=rng.uniform(0, 360), roll=r_, scale=(0.9, 0.9, 0.9))

    # ---- shade shelter (tall roof on posts + palisade back walls + hay rack)
    sx, sy = G["shelter"]["center"]
    sz = ground(sx, sy)
    plan.hero_actor("GIR_Shelter_Roof", PLATFORM1, sx - 0.25, sy + 0.05, sz - 0.15, scale=(1.0, 1.0, 1.32), folder=F_HAB)
    for wy in (sy - 1.85, sy + 1.85):
        b = ground(sx + 3.9, wy) - 0.3
        plan.inst("GIR_ShelterWalls", F_HAB, BARRICADE, sx + 3.95, wy, b, yaw=0, scale=(1.0, 1.02, 1.55))
    for wx in (sx - 1.85, sx + 1.85):
        b = ground(wx, sy + 3.9) - 0.3
        plan.inst("GIR_ShelterWalls", F_HAB, BARRICADE, wx, sy + 3.95, b, yaw=90, scale=(1.0, 1.02, 1.55))
    plan.inst("GIR_ShelterWalls", F_HAB, CAGE, sx + 3.55, sy, sz + 3.9, pitch=90, scale=(0.75, 0.75, 1.0))
    plan.inst("GIR_ShelterWalls", F_HAB, BOX, sx + 2.6, sy + 2.2, sz - 0.02, yaw=17)
    plan.inst("GIR_ShelterWalls", F_HAB, BUCKET, sx + 2.3, sy + 1.0, sz - 0.01, yaw=40)
    # keeper area just outside the gate
    kx, ky = gate_center[0], gate_center[1] + 3.0
    plan.inst("GIR_KeeperArea", F_HAB, BARREL, kx - 2.0, ky + 0.8, ground(kx - 2, ky + 0.8) - 0.02, yaw=10)
    plan.inst("GIR_KeeperArea", F_HAB, BARREL, kx - 1.0, ky + 1.3, ground(kx - 1, ky + 1.3) - 0.02, yaw=70, scale=(0.9, 0.9, 0.9))
    plan.inst("GIR_KeeperArea", F_HAB, BOX, kx + 2.6, ky + 0.9, ground(kx + 2.6, ky + 0.9) - 0.02, yaw=-12)
    plan.inst("GIR_KeeperArea", F_HAB, BOX, kx + 2.7, ky + 0.9, ground(kx + 2.7, ky + 0.9) + 1.0, yaw=8, scale=(0.9, 0.9, 0.9))

    # ---- paddock specimen oaks with log trunk guards
    for (tx, ty), m, sc, yw in zip(G["trees"], [OAKS[0], OAKS[1], OAKS[4]], [0.8, 0.68, 0.66], [40, 200, 120]):
        gz = ground(tx, ty)
        plan.inst("GIR_Trees", F_HAB, m, tx, ty, gz - 0.1, yaw=yw, scale=(sc, sc, sc))
        ring = []
        for k in range(8):
            a = 2 * math.pi * k / 8
            px, py = tx + 1.5 * math.cos(a), ty + 1.5 * math.sin(a)
            ring.append((px, py))
            plan.inst("GIR_TreeGuards", F_HAB, WOODPILLAR2, px, py, ground(px, py) - 0.3, yaw=rng.uniform(0, 360), scale=(0.65, 0.65, 2.9))
        for k in range(8):
            (ax, ay), (bx, by) = ring[k], ring[(k + 1) % 8]
            for h in (0.9, 1.75):
                add_rail(plan, "GIR_TreeGuards", F_HAB, ax, ay, ground(ax, ay) + h, bx, by, ground(bx, by) + h, SINGLELOG, 3.41, 0.3)

    # ---- slate outcrop on the mound: half-buried full boulders sharing one strike (~35 deg) and a gentle dip,
    #      so it reads as bedrock breaking the surface rather than dropped props
    ox, oy = G["mound"]["center"]
    outcrop = [(ROCK_M6, 0.6, 0.9, 35, 2.1, 0.35, 10), (ROCK_M1, -1.6, -0.5, 30, 1.9, 0.4, 8), (ROCK_S16, 2.4, -1.4, 40, 1.2, 0.45, 6),
               (ROCK_M6, -0.9, 2.3, 55, 1.4, 0.35, -8), (ROCK_M1, 2.9, 1.9, 15, 1.2, 0.4, 5), (ROCK_M6, -2.9, 1.2, 80, 0.8, 0.3, 4),
               (ROCK_M1, 1.3, 3.4, 200, 0.7, 0.35, -5), (ROCK_S16, -2.8, -2.2, 25, 0.8, 0.45, 3)]
    for m, dx, dy, yw, sc, bury, rl in outcrop:
        boulder(plan, "GIR_Habitat", F_HAB, m, ox + dx, oy + dy, yw + jitter(rng, 4), sc, bury, pitch=jitter(rng, 3), roll=rl)
    for (bx, by, m, sc) in [(-21.0, 47.0, ROCK_M1, 1.1), (35.5, 30.5, ROCK_M1, 0.9), (-4.0, 49.5, ROCK_S16, 1.0), (30.5, 18.5, ROCK_S16, 0.9)]:
        plan.inst("GIR_Habitat", F_HAB, m, bx, by, ground(bx, by) - 0.08, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))
    plan.inst("GIR_Habitat", F_HAB, LOGS[2], -19.5, 40.5, ground(-19.5, 40.5) - 0.05, yaw=30)
    plan.inst("GIR_Habitat", F_HAB, LOGS[0], 24.5, 29.0, ground(24.5, 29.0) - 0.03, yaw=-50)
    # (wear tracks, mud and gravel are painted into the landscape; the Quarry patch meshes rendered as flat pale discs)

    # ---- dry tussocks along the fence band, around rocks and trees (keep anchors clear)
    anchors = list(G["anchors"].values())

    def tuss_ok(x, y):
        s = paddock_sdf(x, y)
        if s > -0.8 or x < gt.pad_x0 + 0.5:
            return False
        if any(math.hypot(x - a[0], y - a[1]) < 7.0 for a in anchors):
            return False
        near_feature = min([math.hypot(x - t[0], y - t[1]) for t in G["trees"]] + [math.hypot(x - ox, y - oy)]) < 5.5
        return s > -4.5 or near_feature

    for (x, y) in poisson(rng, gt.pad_x0, gt.pad_x1, gt.pad_y0, gt.pad_y1, 1.5, tuss_ok):
        if rng.random() < 0.55:
            sc = rng.uniform(1.8, 2.8)
            plan.inst("GIR_Tussocks", F_HAB, rng.choice(TUSSOCKS), x, y, ground(x, y) - 0.03, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- low planting strip between visitor rail and moat edge (keeps the view open)
    def strip_ok(x, y):
        return -26.0 < x < 2.8 and 7.9 < y < 9.7

    for (x, y) in poisson(rng, -26.5, 3.0, 7.8, 9.8, 1.0, strip_ok):
        r = rng.random()
        if r < 0.35:
            m, sc = rng.choice(CP_LOWGRASS), rng.uniform(0.9, 1.2)
        elif r < 0.6:
            m, sc = HELLEBORE[0], rng.uniform(0.9, 1.3)
        elif r < 0.85:
            m, sc = rng.choice(FERNS), rng.uniform(0.35, 0.5)
        else:
            m, sc = rng.choice(HZ_SEEDLING), rng.uniform(1.2, 1.6)
        plan.inst("GIR_MoatPlanting", F_HAB, m, x, y, ground(x, y) - 0.03, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- canal: water in the visitor moat + stone kerb on the paddock side (own RNG keeps every other instance unchanged)
    cn = G["canal"]
    (cx0, cx1), (cy0, cy1) = cn["x"], cn["y"]
    plan.hero_actor("GIR_Canal_Water", WATER, (cx0 + cx1) / 2, (cy0 + cy1) / 2, cn["water_z"],
                    scale=((cx1 - cx0) / 1.5, (cy1 - cy0) / 1.5, 1.0), folder="Giraffe/Water")   # SM_WaterPlane is 1.5 m square
    # ---- wrap: the canal bends round the enclosure as a U (south leg, west front, north leg). The terrain isn't carved, so
    # it is laid as short water tiles, each sitting 0.2 m over its own lowest ground; tiles over the knoll are skipped.
    w = cn["wrap"]
    hw, r, wy = w["half_width"], w["corner_r"], w["west_y"]
    sx, nx, ey = w["south_x"], w["north_x"], w["leg_end_y"]
    path = []   # (x, y, tangent angle in degrees), walked from the south leg end round to the north leg end, 0.2 m apart

    def seg(x0, y0, x1, y1):
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 0.2))
        ang = math.degrees(math.atan2(y1 - y0, x1 - x0))
        path.extend((x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n, ang) for k in range(n))

    def arc(cx, cy, a0, a1, turn):
        n = max(1, int(abs(a1 - a0) * math.pi / 180 * r / 0.2))
        for k in range(n):
            a = math.radians(a0 + (a1 - a0) * k / n)
            path.append((cx + r * math.cos(a), cy + r * math.sin(a), math.degrees(a) + turn))

    seg(sx, ey, sx, wy + r)                      # south leg, heading -Y
    arc(sx + r, wy + r, 180, 270, 90)            # corner into the west front
    seg(sx + r, wy, nx - r, wy)                  # west front, heading +X
    arc(nx - r, wy + r, -90, 0, 90)              # corner into the north leg
    seg(nx, wy + r, nx, ey)                      # north leg, heading +Y

    def ptv(k):
        return path[min(k, len(path) - 1)]

    k, TILE = 0, 2.4
    while k < len(path):
        x, y, ang = path[k]
        a = math.radians(ang)
        fx, fy, sx_, sy_ = math.cos(a), math.sin(a), -math.sin(a), math.cos(a)   # forward / left unit vectors
        gs = [ground(x + i * fx * 1.2 + j * sx_ * hw, y + i * fy * 1.2 + j * sy_ * hw) for i in (-1, 0, 1) for j in (-1, 0, 1)]
        inside_moat = cx0 <= x <= cx1 and cy0 <= y <= cy1   # the big hero plane already covers this
        if min(gs) < -0.3 and not inside_moat:
            plan.inst("GIR_CanalWrap", "Giraffe/Water", WATER, x, y, min(gs) + 0.2, yaw=ang,
                      scale=((TILE + 0.15) / 1.5, 2 * hw / 1.5, 1.0))
        k += int(TILE / 0.2)

    krng = random.Random(11)
    k = 0
    while k < len(path):   # long flat slabs along the inner (paddock side) edge, chained end to end
        x, y, ang = path[k]
        a = math.radians(ang)
        kx, ky = x - math.sin(a) * (hw + 0.2), y + math.cos(a) * (hw + 0.2)
        m = krng.choice((ROCK_S16, ROCK_S16, ROCK_M1))
        sc = krng.uniform(1.0, 1.35)
        adv = BOULDER_DIMS[m][0] * sc * 0.85
        if ground(kx, ky) < -0.3:
            boulder(plan, "GIR_CanalKerb", F_BAR, m, kx, ky + jitter(krng, 0.12) * math.cos(a), ang + krng.choice((0, 180)) + jitter(krng, 10), sc, 0.25)
        k += max(1, int(adv / 0.2))

    plan.group_props("GIR_Trees", F_HAB, WPO_NEAR)
    plan.group_props("GIR_Feeders", F_HAB, WPO_NEAR)
    for g in ("GIR_Tussocks", "GIR_MoatPlanting"):
        plan.group_props(g, F_HAB, {"CastShadow": True, "InstanceEndCullDistance": 9000})
    return plan.write()


# ================================================================== WOMBAT
def stage_wombat():
    plan = Plan("wombat")
    rng = random.Random(2)
    F_WALL, F_HAB = "Wombat/Wall", "Wombat/Habitat"
    x0, x1, y0, y1 = gt.wx0, gt.wx1, gt.wy0, gt.wy1

    # front palisade walls (south x=x0, east y=y1) - pit floor -1.8, parapet ~1 m above the path
    n = 6
    for k in range(n):
        py = y0 + (y1 - y0) * (k + 0.5) / n
        plan.inst("WOM_Palisade", F_WALL, BARRICADE, x0 + 0.15, py, -2.35, yaw=0, scale=(1.0, (y1 - y0) / n / 3.62 * 1.02, 1.0))
    n = 5
    for k in range(n):
        px = x0 + (x1 - x0) * (k + 0.5) / n
        plan.inst("WOM_Palisade", F_WALL, BARRICADE, px, y1 - 0.15, -2.35, yaw=90, scale=(1.0, (x1 - x0) / n / 3.62 * 1.02, 1.0))
    # back walls on the bank top (mostly hidden in hazel)
    for k in range(6):
        py = y0 + (y1 - y0) * (k + 0.5) / 6
        b = ground(x1 - 0.2, py) - 0.5
        plan.inst("WOM_Palisade", F_WALL, BARRICADE, x1 + 0.15, py, b, yaw=0, scale=(1.0, (y1 - y0) / 6 / 3.62 * 1.02, 0.6))
    for k in range(5):
        px = x0 + (x1 - x0) * (k + 0.5) / 5
        b = ground(px, y0 + 0.2) - 0.5
        plan.inst("WOM_Palisade", F_WALL, BARRICADE, px, y0 - 0.15, b, yaw=90, scale=(1.0, (x1 - x0) / 5 / 3.62 * 1.02, 0.6))

    # overlook bay floor (two plank platforms) + benches facing the exhibit
    ov = W["overlook"]["center"]
    for dy in (-1.55, 1.55):
        px, py = ov[0] - 0.05, ov[1] + dy
        plan.inst("WOM_Overlook", F_WALL, PLATFORM3, px + 0.04, py - 0.09, ground(px, py) - 0.02, yaw=0, scale=(0.95, 1.0, 1.0))

    # burrow mound: two den mouths facing the visitors - a slate lintel on two slate jambs with a back stone,
    # so the opening under the lintel stays in shadow; fresh sticks spill out front
    mx, my = W["mound"]["center"]
    for (bx, by, yaw_mouth) in [(29.4, -46.3, 180), (31.2, -43.4, 210)]:
        gz = ground(bx, by)
        a = math.radians(yaw_mouth + 90)
        side = (math.cos(a) * 0.85, math.sin(a) * 0.85)
        fdir = (math.cos(math.radians(yaw_mouth)), math.sin(math.radians(yaw_mouth)))
        plan.inst("WOM_Burrows", F_HAB, ROCK_M6, bx + side[0], by + side[1], gz - 0.15, yaw=yaw_mouth + 10, scale=(1.0, 1.0, 0.8))
        plan.inst("WOM_Burrows", F_HAB, ROCK_M6, bx - side[0], by - side[1], gz - 0.15, yaw=yaw_mouth - 170, scale=(0.9, 0.9, 0.8))
        # deep slab roof (1.4 m front-to-back): with the ~32 deg afternoon sun the back of the 0.38 m opening stays shaded
        plan.inst("WOM_Burrows", F_HAB, ROCK_S16, bx - fdir[0] * 0.3, by - fdir[1] * 0.3, gz + 0.38, yaw=yaw_mouth + 90 + jitter(rng, 4),
                  pitch=jitter(rng, 2), roll=-4.0, scale=(0.8, 1.0, 0.8))
        plan.inst("WOM_Burrows", F_HAB, ROCK_M6, bx - fdir[0] * 1.0, by - fdir[1] * 1.0, gz - 0.2, yaw=yaw_mouth + 90,
                  scale=(1.3, 1.2, 1.0))                                                       # back stone closes the den
        plan.inst("WOM_Burrows", F_HAB, STICKS[0], bx + fdir[0] * 1.2, by + fdir[1] * 1.2, ground(bx + fdir[0] * 1.2, by + fdir[1] * 1.2) - 0.02,
                  yaw=yaw_mouth + 60, scale=(0.9, 0.9, 0.9))
    boulder(plan, "WOM_Burrows", F_HAB, ROCK_S16, mx + 1.6, my - 0.8, 70, 0.85, 0.4, roll=6)
    boulder(plan, "WOM_Burrows", F_HAB, ROCK_M1, mx - 0.4, my + 2.9, 200, 0.9, 0.35)
    plan.inst("WOM_Burrows", F_HAB, LOGS[0], 26.3, -50.8, ground(26.3, -50.8) - 0.05, yaw=35, scale=(1.2, 1.2, 1.2))
    plan.inst("WOM_Burrows", F_HAB, LOGS[3], 34.0, -39.0, ground(34.0, -39.0) - 0.08, yaw=-20, scale=(0.9, 0.9, 0.9))
    boulder(plan, "WOM_Burrows", F_HAB, ROCK_M6, 22.8, -51.5, 10, 0.9, 0.3)
    plan.inst("WOM_Burrows", F_HAB, MOSSY[2], 35.2, -44.5, ground(35.2, -44.5) - 0.05, yaw=45)

    anchors = list(W["anchors"].values())

    def free(x, y, r):
        return all(math.hypot(x - a[0], y - a[1]) > r for a in anchors)

    def inside(x, y, m=0.4):
        return gt.sd_round_rect(x, y, x0, x1, y0, y1, W["corner_radius"]) < -m

    # hazel & ferns on the back bank (north and west) - backdrop only
    def bank_ok(x, y):
        return inside(x, y, 0.8) and (x > x1 - 4.8 or y < y0 + 4.8)

    for (x, y) in poisson(rng, x0, x1, y0, y1, 2.6, bank_ok):
        r = rng.random()
        if r < 0.45:
            m, sc = rng.choice(HZ_FOREST), rng.uniform(0.42, 0.58)
        elif r < 0.7:
            m, sc = rng.choice(HZ_SAPLING), rng.uniform(1.0, 1.4)
        else:
            m, sc = rng.choice(FERNS), rng.uniform(0.55, 0.8)
        plan.inst("WOM_Planting", F_HAB, m, x, y, ground(x, y) - 0.05, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))
    # tussocks, leaf litter and sticks on the floor (keep sight-lines to the anchors open)
    for (x, y) in poisson(rng, x0, x1, y0, y1, 1.3, lambda x, y: inside(x, y) and free(x, y, 2.2)):
        r = rng.random()
        if r < 0.45:
            sc = rng.uniform(1.2, 1.9)
            plan.inst("WOM_Floor", F_HAB, rng.choice(TUSSOCKS), x, y, ground(x, y) - 0.03, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))
        elif r < 0.6:
            plan.inst("WOM_Floor", F_HAB, rng.choice(STICKS), x, y, ground(x, y) - 0.01, yaw=rng.uniform(0, 360))
        elif r < 0.72:
            plan.inst("WOM_Floor", F_HAB, rng.choice(CP_LEAVES), x, y, ground(x, y) - 0.01, yaw=rng.uniform(0, 360))
    for (x, y) in poisson(rng, x0, x1, y0, y1, 3.2, lambda x, y: inside(x, y, 1.0)):
        if rng.random() < 0.6:
            plan.inst("WOM_Floor", F_HAB, rng.choice(LEAVES), x, y, ground(x, y) - 0.02, yaw=rng.uniform(0, 360), scale=(0.8, 0.8, 1.0))
    # bed outside the overlook (between the path and the forest)
    bed = [b for b in L["beds"] if b["name"] == "overlook_bed"][0]
    for (x, y) in poisson(rng, bed["x"][0], bed["x"][1], bed["y"][0], bed["y"][1], 1.4, lambda x, y: path_edge_dist(x, y) > 0.6):
        m = rng.choice(HZ_SAPLING + FERNS + HELLEBORE)
        sc = 0.6 if m in FERNS else 1.0
        plan.inst("WOM_Planting", F_HAB, m, x, y, ground(x, y) - 0.03, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    plan.group_props("WOM_Planting", F_HAB, WPO_NEAR)
    plan.group_props("WOM_Floor", F_HAB, {"InstanceEndCullDistance": 8000})
    return plan.write()


# ================================================================== POND
def stage_pond():
    plan = Plan("pond")
    rng = random.Random(3)
    F = "Pond"
    wz = P["water_z"]
    plan.hero_actor("POND_Water", WATER, 22.0, -12.5, wz, scale=(18.0, 21.0, 1.0), folder=F)
    # bridge (WoodenProps Bridge: long axis local X, deck ends ~0.33 m above pivot)
    a, b = P["bridge"]["a"], P["bridge"]["b"]
    bs = P["bridge"]["scale"]
    mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
    plan.hero_actor("POND_Bridge", BRIDGE, mid[0], mid[1] - 0.26 * bs[0], P["bridge"]["pivot_z"], yaw=90, scale=tuple(bs), folder=F)
    # viewing pier at the end of the spur (SW edge meets the path)
    pc = (12.25, -17.25)
    ptop = 0.12
    plan.hero_actor("POND_Pier", PIER, pc[0], pc[1], ptop, yaw=45, folder=F)
    plan.hero_actor("POND_PierLegs", PIER_LEGS, pc[0], pc[1], ptop, yaw=45, folder=F)
    for yw in (45, 135, 225):
        plan.inst("POND_PierRails", F, PIER_RAIL, pc[0], pc[1], ptop + 0.02, yaw=yw)
    # weeping willow on the north shore + black alders around
    plan.inst("POND_Trees", F, WILLOW, 37.5, -16.5, ground(37.5, -16.5) - 0.25, yaw=70, scale=(0.9, 0.9, 0.9))
    for (tx, ty, m, sc, yw) in [(7.0, -8.5, BA_FIELD[0], 0.72, 30), (14.5, -26.5, BA_FIELD[2], 0.66, 200), (34.0, -25.0, BA_FIELD[4], 0.62, 120),
                                (35.5, -6.0, BA_FOREST[3], 0.9, 80)]:
        plan.inst("POND_Trees", F, m, tx, ty, ground(tx, ty) - 0.1, yaw=yw, scale=(sc, sc, sc))

    # shoreline samples
    shore = []
    for i in range(0, 90):
        for j in range(0, 90):
            x, y = 6.0 + i * 0.4, -30.0 + j * 0.4
            s = pond_sdf(x, y)
            if 0.1 < s < 0.9:
                shore.append((x, y))

    def clear_of_crossings(x, y, r):
        pts = [tuple(a), tuple(b), (10.4, -19.0), pc]
        return all(math.hypot(x - q[0], y - q[1]) > r for q in pts) and path_edge_dist(x, y) > 0.8

    rng.shuffle(shore)
    placed = []
    for (x, y) in shore:
        if not clear_of_crossings(x, y, 3.2):
            continue
        if any(math.hypot(x - q[0], y - q[1]) < 2.1 for q in placed):
            continue
        placed.append((x, y))
        m = rng.choice(MOSSY + CP_ROCKS)
        sc = rng.uniform(0.6, 1.15)
        plan.inst("POND_Edge", F, m, x, y, ground(x, y) - 0.12 * sc, yaw=rng.uniform(0, 360), pitch=jitter(rng, 6), roll=jitter(rng, 6), scale=(sc, sc, sc))
    # reeds / tall grass in clusters
    centers = []
    for (x, y) in shore:
        if clear_of_crossings(x, y, 4.5) and all(math.hypot(x - c[0], y - c[1]) > 7.0 for c in centers):
            centers.append((x, y))
    for (cx_, cy_) in centers[:9]:
        for _ in range(rng.randint(4, 7)):
            x, y = cx_ + jitter(rng, 1.6), cy_ + jitter(rng, 1.6)
            if pond_sdf(x, y) < -0.8 or not clear_of_crossings(x, y, 3.0):
                continue
            m = rng.choice(CP_TALLGRASS + TUSSOCKS)
            sc = rng.uniform(0.9, 1.3) if m in CP_TALLGRASS else rng.uniform(1.6, 2.2)
            plan.inst("POND_Plants", F, m, x, y, max(ground(x, y), wz) - 0.05, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))
    # lily pads (keep the bridge corridor open)
    for (lx, ly) in [(15.8, -12.5), (13.0, -19.2), (21.0, -18.5), (31.8, -15.8), (32.5, -8.2), (19.0, -6.0), (24.0, -19.8), (16.5, -9.5)]:
        if abs(ly - (-12)) < 1 and abs(lx - 28.5) < 3:
            continue
        plan.inst("POND_Lilies", F, rng.choice(LILIES), lx, ly, wz + 0.01, yaw=rng.uniform(0, 360), scale=(0.9, 0.9, 1.0))
    plan.group_props("POND_Trees", F, WPO_NEAR)
    plan.group_props("POND_Plants", F, WPO_NEAR)
    plan.group_props("POND_Lilies", F, {"CastShadow": False})
    return plan.write()


# ================================================================== ENTRANCE (tusk gateway)
def _in_box(b, x, y):
    return b[0] <= x <= b[1] and b[2] <= y <= b[3]


def entrance_clear(group, x, y):
    """True where nothing may be planted: tusk bases, statues, sign posts; no tree crowns inside the arch."""
    if group in ("VEG_Forest", "VEG_Groves", "VEG_Specimens") and _in_box(E["clear_trees"], x, y):
        return True
    return (group.startswith("VEG_") or group == "ENT_Beds") and any(_in_box(b, x, y) for b in E["clear"])


def footprint_ground(x, y, hx, hy):
    """Lowest ground under a footprint (so a block never floats on its downhill side)."""
    return min(ground(x + i * hx, y + j * hy) for i in (-1, 0, 1) for j in (-1, 0, 1))


def entrance_gate(plan, folder):
    # two tusks turned across the plaza mouth so their tips cross ~18.5 m up
    t = E["tusks"]
    for side, sgn, yaw in (("W", -1, -90.0), ("E", 1, 90.0)):
        y = t["base_y"] * sgn
        plan.hero_actor("ENT_Tusk_" + side, TUSK, t["x"], y, footprint_ground(t["x"], y, 2.85, 2.85) - 0.05, yaw=yaw, folder=folder)

    # 'Let's Go Safari' board behind the pendopo (art faces the approach, -X) and the two side billboards outside the tusks
    pp = E["papan"]
    plan.hero_actor("ENT_Papan", PAPAN, pp["pos"][0], pp["pos"][1], footprint_ground(pp["pos"][0], pp["pos"][1], 1.0, 8.5) - 0.03,
                    yaw=pp["yaw"], folder=folder)
    for b in E["billboards"]:
        x, y = b["pos"]
        plan.hero_actor("ENT_Billboard_" + b["name"], mesh(GATE + "SM_Billboard_" + b["mesh"]), x, y,
                        footprint_ground(x, y, 1.1, 2.5) - 0.03, yaw=b["yaw"], folder=folder)

    # pendopo in the plaza centre; the trail splits around it (path_guide connectors)
    p = E["pendopo"]
    plan.hero_actor("ENT_Pendopo", PENDOPO, p["center"][0], p["center"][1], footprint_ground(p["center"][0], p["center"][1], 3.85, 4.0) - 0.02,
                    yaw=p["yaw"], folder=folder)

    # statues on their plinths just behind each tusk base, plaques turned toward the arch
    fx, fy = E["statues_face"]
    for st in E["statues"]:
        x, y = st["pos"]
        plan.hero_actor("ENT_Statue_" + st["name"], mesh(GATE + "SM_Gate_Statue_" + st["name"]), x, y, footprint_ground(x, y, 1.1, 1.0) - 0.03,
                        yaw=yaw_of(x, y, fx, fy) - STATUE_FRONT_YAW, folder=folder)


# ================================================================== VISITOR (entrance, rest area, path furniture)
def bench(plan, label, x, y, face_yaw, folder):
    # SM_Bench03: long axis local Y, seat faces local +X (verified in the editor at the wombat overlook)
    plan.hero_actor(label, BENCH, x, y, ground(x, y) - 0.02, yaw=face_yaw, folder=folder)


def stage_visitor():
    plan = Plan("visitor")
    plan.skip = entrance_clear
    rng = random.Random(4)
    FE, FR, FPATH = "Entrance", "RestArea", "Paths"

    # ---- entrance: tusk gateway, sign frame, pendopo, statues
    entrance_gate(plan, FE)
    rng.uniform(0, 90), rng.uniform(0, 90)  # draws of the former log-gate pillars, kept so the rest of the stage is unchanged

    # ---- plaza furniture (the front lamps stand in the corners behind the tusk bases)
    for (bx, by, face) in [(-41.3, 9.7, -90), (-35.3, 9.7, -90), (-41.3, -9.7, 90), (-35.3, -9.7, 90)]:
        bench(plan, "ENT_Bench_%d_%d" % (abs(bx), abs(by)), bx, by, face, FE)
    for (lx, ly) in [(-42.7, 10.6), (-42.7, -10.6), (-32.4, 10.4), (-32.4, -10.4)]:
        plan.inst("PATH_Lamps", FPATH, LAMP, lx, ly, ground(lx, ly) - 0.02, yaw=rng.uniform(0, 360))
    for (bx, by) in [(-38.3, 10.4), (-38.3, -10.4)]:
        plan.inst("PATH_Bins", FPATH, BIN, bx, by, ground(bx, by) - 0.02, yaw=rng.uniform(0, 360))
    # plaza beds
    for nm in ("plaza_bed_e", "plaza_bed_w"):
        bed = [b for b in L["beds"] if b["name"] == nm][0]
        for (x, y) in poisson(rng, bed["x"][0], bed["x"][1], bed["y"][0], bed["y"][1], 1.5, lambda x, y: path_edge_dist(x, y) > 0.4):
            r = rng.random()
            if r < 0.3:
                m, sc = rng.choice(WINTERBERRY), rng.uniform(0.45, 0.6)
            elif r < 0.55:
                m, sc = rng.choice(SPINDLE), rng.uniform(0.6, 0.85)
            elif r < 0.8:
                m, sc = rng.choice(HZ_SAPLING), rng.uniform(0.8, 1.1)
            else:
                m, sc = rng.choice(FERNS), rng.uniform(0.45, 0.6)
            plan.inst("ENT_Beds", FE, m, x, y, ground(x, y) - 0.04, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- rest area: rustic gazebo, benches, drinking fountain
    rc = L["rest_area"]["center"]
    plan.hero_actor("REST_Gazebo", BOWER, rc[0], rc[1], ground(*rc) - 0.05, yaw=35, folder=FR)
    for (bx, by, face) in [(5.6, -26.2, 45), (-9.6, -26.0, 5), (-6.0, -38.4, 60)]:   # first one sits beside the pond spur
        bench(plan, "REST_Bench_%d_%d" % (abs(bx * 10), abs(by * 10)), bx, by, face, FR)
    plan.hero_actor("REST_DrinkingFountain", FOUNTAIN, 3.2, -36.2, ground(3.2, -36.2) - 0.02, folder=FR)
    for (bx, by) in [(5.2, -28.5), (-9.9, -31.5)]:
        plan.inst("PATH_Bins", FPATH, BIN, bx, by, ground(bx, by) - 0.02, yaw=rng.uniform(0, 360))

    # ---- benches at stops along the trail (facing the view)
    stops = [(-17.5, 3.0, 90), (-3.0, 2.9, 80), (6.6, 2.4, 60), (24.8, 4.6, -120), (23.0, -25.6, 20),
             (12.2, -17.25, 45)]
    for i, (bx, by, face) in enumerate(stops):
        if i == len(stops) - 1:
            plan.hero_actor("POND_PierBench", BENCH, bx, by, 0.14, yaw=face, folder="Pond")
        else:
            bench(plan, "PATH_Bench_%02d" % i, bx, by, face, FPATH)
    for (bx, by) in [(-15.8, 2.6), (8.2, 2.3), (21.6, -26.7)]:
        plan.inst("PATH_Bins", FPATH, BIN, bx, by, ground(bx, by) - 0.02, yaw=rng.uniform(0, 360))
    # wombat overlook benches
    ov = W["overlook"]["center"]
    for dy in (-1.8, 1.8):
        plan.hero_actor("WOM_Bench_%s" % ("S" if dy < 0 else "N"), BENCH, ov[0] - 1.05, ov[1] + dy, ground(ov[0] - 1.05, ov[1] + dy) + 0.12,
                        yaw=0, folder="Wombat/Wall")

    # ---- lamp posts every ~20 m along the loop, on the side away from animals/water
    for path in L["paths"]:
        if path["name"] in ("approach", "deck_link", "pond_spur"):
            continue
        pts = path["pts"]
        acc = 8.0
        side = 1
        for a, b in zip(pts[:-1], pts[1:]):
            ln = math.hypot(b[0] - a[0], b[1] - a[1])
            dirx, diry = (b[0] - a[0]) / ln, (b[1] - a[1]) / ln
            t = 0.0
            while t < ln:
                step = min(ln - t, 20.0 - acc)
                t += step
                acc += step
                if acc >= 20.0 - 1e-6:
                    acc = 0.0
                    px, py = a[0] + dirx * t, a[1] + diry * t
                    off = path["width"] / 2 + 0.8
                    placed = False
                    for s in (side, -side):
                        lx, ly = px - diry * off * s, py + dirx * off * s
                        if not in_animal_or_water(lx, ly, 1.5) and not on_pad(lx, ly, -1.0) and path_edge_dist(lx, ly) > 0.3:
                            plan.inst("PATH_Lamps", FPATH, LAMP, lx, ly, ground(lx, ly) - 0.02, yaw=rng.uniform(0, 360))
                            placed = True
                            side = -s
                            break
    return plan.write()


# ================================================================== VEGETATION
def stage_veg():
    plan = Plan("veg")
    plan.skip = entrance_clear
    rng = random.Random(5)
    F = "Vegetation"
    appr = [p for p in L["paths"] if p["name"] == "approach"][0]

    def appr_dist(x, y):
        best = 1e9
        for a, b in zip(appr["pts"][:-1], appr["pts"][1:]):
            vx, vy = b[0] - a[0], b[1] - a[1]
            ll = vx * vx + vy * vy
            t = max(0.0, min(1.0, ((x - a[0]) * vx + (y - a[1]) * vy) / ll))
            best = min(best, math.hypot(x - (a[0] + vx * t), y - (a[1] + vy * t)))
        return best

    # ---- forest belt on the hills
    def forest_ok(x, y):
        return core_out(x, y) >= 7.5 and appr_dist(x, y) > 7.0 and slope(x, y) < 1.3

    def pick_tree(x, y):
        r = rng.random()
        if core_out(x, y) < 22.0:
            # visible forest edge: open-grown (Field) crowns that reach low and close the view between trunks
            if r < 0.35:
                return rng.choice(HB_FIELD), rng.uniform(0.7, 0.95)
            if r < 0.55:
                return rng.choice(BA_FIELD), rng.uniform(0.65, 0.85)
            if r < 0.8:
                return rng.choice(OAKS), rng.uniform(0.55, 0.8)
            return rng.choice(HB_FOREST), rng.uniform(0.95, 1.2)
        if r < 0.3:
            return rng.choice(OAKS), rng.uniform(0.55, 0.8)
        if r < 0.72:
            return rng.choice(HB_FOREST), rng.uniform(0.95, 1.25)
        return rng.choice(BA_FOREST), rng.uniform(0.85, 1.1)

    pts = poisson(rng, -124, 124, -124, 124, 7.5, forest_ok)
    for (x, y) in pts:
        m, sc = pick_tree(x, y)
        plan.inst("VEG_Forest", F, m, x, y, ground(x, y) - 0.25, yaw=rng.uniform(0, 360), pitch=jitter(rng, 2), roll=jitter(rng, 2), scale=(sc, sc, sc))

    # ---- forest edge understory (shrubs + saplings) facing the zoo
    def edge_ok(x, y):
        d = core_out(x, y)
        return 1.5 <= d <= 18 and appr_dist(x, y) > 4.5 and not in_animal_or_water(x, y, 2.0) and path_edge_dist(x, y) > 1.5

    for (x, y) in poisson(rng, -70, 70, -80, 80, 3.6, edge_ok):
        r = rng.random()
        if r < 0.35:
            m, sc = rng.choice(HZ_FOREST), rng.uniform(0.45, 0.7)
        elif r < 0.5:
            m, sc = rng.choice(WINTERBERRY + PRIVET), rng.uniform(0.75, 1.0)
        elif r < 0.62:
            m, sc = rng.choice(SPINDLE), rng.uniform(0.9, 1.2)
        elif r < 0.82:
            m, sc = rng.choice(HB_SAPLING), rng.uniform(0.9, 1.3)
        else:
            m, sc = rng.choice(OAK_SAPLINGS), rng.uniform(1.0, 1.5)
        plan.inst("VEG_ForestEdge", F, m, x, y, ground(x, y) - 0.05, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- woodland mid-layer on the slopes (hazel, saplings) so the hills read as woodland, not open ground
    def mid_ok(x, y):
        d = core_out(x, y)
        return 14.0 <= d <= 48.0 and appr_dist(x, y) > 6.0 and slope(x, y) < 1.3

    for (x, y) in poisson(rng, -124, 124, -124, 124, 6.0, mid_ok):
        r = rng.random()
        if r < 0.5:
            m, sc = rng.choice(HZ_FOREST), rng.uniform(0.6, 0.9)
        elif r < 0.8:
            m, sc = rng.choice(HB_SAPLING), rng.uniform(1.1, 1.5)
        else:
            m, sc = rng.choice(OAK_SAPLINGS), rng.uniform(1.2, 1.6)
        plan.inst("VEG_ForestMid", F, m, x, y, ground(x, y) - 0.05, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- forest floor clutter (logs, mossy rocks, ferns) near the edge where the camera looks
    def floor_ok(x, y):
        d = core_out(x, y)
        return 2.0 <= d <= 30 and appr_dist(x, y) > 3.0 and not in_animal_or_water(x, y, 1.0) and path_edge_dist(x, y) > 1.0

    for (x, y) in poisson(rng, -80, 80, -90, 90, 4.5, floor_ok):
        r = rng.random()
        if r < 0.45:
            m, sc = rng.choice(FERNS), rng.uniform(0.6, 0.95)
        elif r < 0.6:
            m, sc = rng.choice(LOGS), rng.uniform(0.8, 1.2)
        elif r < 0.78:
            m, sc = rng.choice(MOSSY + MAE_ROCKS), rng.uniform(0.6, 1.2)
        else:
            m, sc = rng.choice(LEAVES), 1.0
        plan.inst("VEG_ForestFloor", F, m, x, y, ground(x, y) - 0.06 * sc, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- groves inside the zoo (framing trees + shrubs)
    grove_species = {
        "west_lawn": HB_FIELD + [OAKS[2]], "plaza_se": HB_FIELD, "plaza_sw": HB_FIELD, "pond_north": BA_FIELD,
        "knoll_north": HB_FIELD[:2], "wombat_west": HZ_FOREST + [OAKS[3]], "between_pond_wombat": BA_FIELD[:3],
        "paddock_ne_backdrop": [OAKS[5]], "se_meadow": [OAKS[1], HB_FIELD[0], HB_FIELD[2]], "se_meadow_2": HB_FIELD[1:3],
    }

    def grove_ok(x, y):
        return (not in_animal_or_water(x, y, 3.0)) and path_edge_dist(x, y) > 2.5 and not on_pad(x, y, 1.5)

    for gr in L["groves"]:
        cx_, cy_, r_ = gr["c"][0], gr["c"][1], gr["r"]
        species = grove_species.get(gr["name"], HB_FIELD)
        tpts = poisson(rng, cx_ - r_, cx_ + r_, cy_ - r_, cy_ + r_, 6.5,
                       lambda x, y: math.hypot(x - cx_, y - cy_) < r_ and grove_ok(x, y))
        for (x, y) in tpts[:5]:
            m = rng.choice(species)
            if m in OAKS:
                sc = rng.uniform(0.55, 0.7)
            elif m in HZ_FOREST:
                sc = rng.uniform(0.55, 0.75)
            else:
                sc = rng.uniform(0.62, 0.8)
            plan.inst("VEG_Groves", F, m, x, y, ground(x, y) - 0.15, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))
        for (x, y) in poisson(rng, cx_ - r_ - 2, cx_ + r_ + 2, cy_ - r_ - 2, cy_ + r_ + 2, 2.4,
                              lambda x, y: r_ * 0.4 < math.hypot(x - cx_, y - cy_) < r_ + 1.5 and grove_ok(x, y)):
            if rng.random() < 0.55:
                m = rng.choice(HZ_SAPLING + HB_SAPLING + FERNS + SPINDLE)
                sc = rng.uniform(0.5, 0.7) if m in FERNS else rng.uniform(0.8, 1.2)
                plan.inst("VEG_GroveShrubs", F, m, x, y, ground(x, y) - 0.05, yaw=rng.uniform(0, 360), scale=(sc, sc, sc))

    # ---- specimen trees on the lawns
    for (x, y, m, sc, yw) in [(-12.0, -6.5, OAKS[3], 0.62, 60), (-26.0, 26.0, OAKS[0], 0.62, 10), (-30.0, -24.0, HB_FIELD[1], 0.8, 200),
                              (9.0, -42.0, HB_FIELD[3], 0.72, 90), (20.0, 12.5, HB_FIELD[2], 0.55, 300), (-20.5, 14.0, HB_FIELD[0], 0.7, 140)]:
        if in_animal_or_water(x, y, 2.0):
            continue
        plan.inst("VEG_Specimens", F, m, x, y, ground(x, y) - 0.15, yaw=yw, scale=(sc, sc, sc))

    for g in ("VEG_Forest", "VEG_Groves", "VEG_Specimens"):
        plan.group_props(g, F, WPO_NEAR)
    plan.group_props("VEG_ForestEdge", F, {"WorldPositionOffsetDisableDistance": 4000})
    plan.group_props("VEG_ForestMid", F, {"WorldPositionOffsetDisableDistance": 3000})
    plan.group_props("VEG_GroveShrubs", F, {"WorldPositionOffsetDisableDistance": 4000})
    plan.group_props("VEG_ForestFloor", F, {"InstanceEndCullDistance": 12000, "WorldPositionOffsetDisableDistance": 3000})
    return plan.write()


# ================================================================== BOUNDS + MARKERS
def stage_bounds():
    plan = Plan("bounds")
    F = "Boundary"
    segs = []

    def wall(ax, ay, bx, by, h=3.2):
        segs.append((ax, ay, bx, by, h))

    # visitor-side barriers
    wall(-26.8, 7.3, 3.3, 7.3)
    wall(3.3, 7.3, 3.3, 9.8)
    wall(3.3, 9.8, 5.7, 9.8)
    wall(5.7, 9.8, 5.7, 17.35)
    wall(5.7, 17.35, 13.3, 17.35)
    wall(13.3, 17.35, 13.3, 9.8)
    wall(13.3, 9.8, 16.2, 9.8)
    wall(16.2, 9.8, 16.2, 14.2)
    wall(-26.8, 7.3, -26.8, 14.3)
    # paddock fence line (north/east/south + west north of the knoll)
    loop = rounded_rect_loop(gt.pad_x0 - 0.3, gt.pad_x1 + 0.3, gt.pad_y0 - 0.3, gt.pad_y1 + 0.3, 6.3, 3.0)
    prev = None
    for (x, y) in loop + loop[:1]:
        skip = y < gt.pad_y0 + 0.4 and -26.5 < x < 16.2
        if prev and not skip:
            wall(prev[0], prev[1], x, y)
        prev = None if skip else (x, y)
    # wombat exhibit
    x0, x1, y0, y1 = gt.wx0, gt.wx1, gt.wy0, gt.wy1
    wall(x0 - 0.2, y0 - 0.2, x0 - 0.2, y1 + 0.2)
    wall(x0 - 0.2, y1 + 0.2, x1 + 0.2, y1 + 0.2)
    wall(x1 + 0.2, y1 + 0.2, x1 + 0.2, y0 - 0.2)
    wall(x1 + 0.2, y0 - 0.2, x0 - 0.2, y0 - 0.2)
    # pond edge (open at bridge ends and pier)
    ring = []
    for k in range(120):
        a = 2 * math.pi * k / 120
        # march outward from the pond centre until outside the water edge
        for rr in [i * 0.25 for i in range(4, 80)]:
            x, y = 21.0 + rr * math.cos(a), -13.0 + rr * math.sin(a)
            if pond_sdf(x, y) > 0.5:
                ring.append((x, y))
                break
    br = P["bridge"]
    openings = [tuple(br["a"]), tuple(br["b"]), (10.6, -19.2)]
    for p, q in zip(ring, ring[1:] + ring[:1]):
        mx_, my_ = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
        if any(math.hypot(mx_ - o[0], my_ - o[1]) < 2.6 for o in openings):
            continue
        wall(p[0], p[1], q[0], q[1], 2.0)
    # outer ring with the approach corridor
    cx0, cx1 = -47.0, 47.0
    cy0, cy1 = -58.0, 58.0
    wall(cx0, cy0, cx1, cy0)
    wall(cx1, cy0, cx1, cy1)
    wall(cx1, cy1, cx0, cy1)
    wall(cx0, cy1, cx0, 3.2)
    wall(cx0, -4.4, cx0, cy0)
    wall(cx0, 3.2, -61.0, 2.2)
    wall(cx0, -4.4, -61.0, -5.2)
    wall(-61.0, -5.2, -61.0, 2.2)

    for (ax, ay, bx, by, h) in segs:
        ln = math.hypot(bx - ax, by - ay)
        if ln < 0.05:
            continue
        cx_, cy_ = (ax + bx) / 2, (ay + by) / 2
        gz = min(ground(ax, ay), ground(bx, by), ground(cx_, cy_))
        top = max(ground(ax, ay), ground(bx, by), ground(cx_, cy_)) + h
        plan.inst("BND_InvisibleWalls", F, CUBE, cx_, cy_, (gz - 0.5 + top) / 2, yaw=yaw_of(ax, ay, bx, by),
                  scale=(ln / 1.0 * 1.02 / 1.0 / 1.0 * 1.0, 0.3, (top - gz + 0.5)))
    plan.group_props("BND_InvisibleWalls", F, {"bVisible": False, "bHiddenInGame": True, "CastShadow": False,
                                               "bAffectDistanceFieldLighting": False})

    # ---- markers (inert hooks for the AR/VR session)
    FM = "Markers"
    for nm, (x, y) in list(G["anchors"].items()) + list(W["anchors"].items()):
        tag = "Zoo.Giraffe" if nm.startswith("GIR") else "Zoo.Wombat"
        plan.hero_actor(nm, None, x, y, ground(x, y) + 0.05, folder=FM, actor_class="/Script/Engine.TargetPoint",
                        tags=["AnimalAnchor", tag])
    zones = [("GIR_Zone_Moat", (-12.0, 5.3), (13.5, 2.6), "Zoo.Giraffe", None),
             ("GIR_Zone_Deck", (9.5, 13.5), (3.9, 3.9), "Zoo.Giraffe", 1.82),
             ("WOM_Zone_Overlook", (17.6, -43.5), (2.2, 5.5), "Zoo.Wombat", None)]
    for nm, (x, y), (ex, ey), tag, zf in zones:
        z = zf if zf is not None else ground(x, y)
        plan.hero_actor(nm, None, x, y, z + 1.6, folder=FM, actor_class="/Script/Engine.TriggerBox", tags=["RevealZone", tag],
                        comp_props={"CollisionComp": {"BoxExtent": {"x": ex * 100, "y": ey * 100, "z": 170}}})
    for nm, v in L["viewpoints"].items():
        px, py, pz = v["pos"]
        lx, ly, lz = v["look"]
        dx, dy, dz = lx - px, ly - py, lz - pz
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
        plan.hero_actor(nm, None, px, py, pz if nm.endswith("Aerial") or nm.endswith("Deck") else ground(px, py) + 1.7,
                        yaw=yaw, pitch=pitch, folder="Cameras", actor_class="/Script/CinematicCamera.CineCameraActor")
    return plan.write()


STAGES = {"giraffe": stage_giraffe, "wombat": stage_wombat, "pond": stage_pond, "visitor": stage_visitor,
          "veg": stage_veg, "bounds": stage_bounds}

if __name__ == "__main__":
    names = sys.argv[1:] or list(STAGES.keys())
    for n in names:
        STAGES[n]()
