# Places the Path Guide walkway network and destination anchors in the open level from zoo_layout.json.
# Run inside the editor's Python (Rider MCP ue_execute_python, or Tools > Execute Python Script).
# Re-running replaces everything it created before (actors in the "PathGuide" outliner folder).
#
#   exec(open(r"D:/UEProjects/Zoo/Tools/PathGuide/place_path_guide.py").read())
import json
import math

import unreal

LAYOUT = r"D:/UEProjects/Zoo/Tools/ZooTerrain/zoo_layout.json"
FOLDER = "PathGuide"
M = 100.0  # layout metres -> UE units (cm)
TRACE_ABOVE = 30.0 * M
TRACE_BELOW = 10.0 * M
Z_TOLERANCE = 1.5 * M  # a traced surface farther than this from the layout height is ignored (roof, tree canopy...)

L = json.load(open(LAYOUT))
PG = L["path_guide"]
CLIP_X = PG["walkway_clip_x_min"]

editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def load_class(path):
    cls = unreal.load_class(None, path)
    if cls is None:
        raise RuntimeError("Class not found (is the WzExploration module built and loaded?): " + path)
    return cls


WALKWAY_CLASS = load_class("/Script/WzExploration.PathGuideWalkway")
DESTINATION_CLASS = load_class("/Script/WzExploration.PathGuideDestination")


def surface_z(x_m, y_m, layout_z_m):
    """Walking surface height in cm: traced from above, falling back to the layout height."""
    fallback = layout_z_m * M
    start = unreal.Vector(x_m * M, y_m * M, fallback + TRACE_ABOVE)
    end = unreal.Vector(x_m * M, y_m * M, fallback - TRACE_BELOW)
    hit = unreal.SystemLibrary.line_trace_single(
        editor_world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)
    data = hit.to_tuple()
    if data[0]:  # blocking hit
        z = data[5].z  # impact point
        if abs(z - fallback) <= Z_TOLERANCE:
            return z
    return fallback


def clip_polyline(points, min_x):
    """Keeps the part of a polyline with x >= min_x (interpolating the crossing point)."""
    out = []
    for a, b in zip(points[:-1], points[1:]):
        ina, inb = a[0] >= min_x, b[0] >= min_x
        if not ina and not inb:
            continue
        if ina != inb:
            t = (min_x - a[0]) / (b[0] - a[0])
            cut = [min_x, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t]
            if ina:
                if not out:
                    out.append(a)
                out.append(cut)
            else:
                out.append(cut)
                out.append(b)
        else:
            if not out:
                out.append(a)
            out.append(b)
    return out


def remove_previous():
    removed = 0
    for actor in actors.get_all_level_actors():
        if not (actor.get_folder_path() == FOLDER or str(actor.get_actor_label()).startswith("PG_")):
            continue
        if isinstance(actor, (unreal.PathGuideWalkway, unreal.PathGuideDestination)):
            actors.destroy_actor(actor)
            removed += 1
    return removed


def spawn_walkway(name, points_m):
    if len(points_m) < 2:
        return None
    pts = [unreal.Vector(p[0] * M, p[1] * M, surface_z(p[0], p[1], p[2])) for p in points_m]
    actor = actors.spawn_actor_from_class(WALKWAY_CLASS, unreal.Vector(0, 0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    actor.set_actor_label("PG_Walkway_" + name)
    actor.set_folder_path(FOLDER)
    spline = actor.get_editor_property("spline")
    spline.set_spline_points(pts, unreal.SplineCoordinateSpace.WORLD, True)
    for i in range(len(pts)):  # the terrain paths are straight segments between the layout points
        spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, True)
    spline.update_spline()
    return actor


def spawn_destination(d):
    x, y = d["pos"]
    z = surface_z(x, y, d.get("z", 0.0))  # "z" = approximate layout height for raised places (the deck)
    lx, ly = d["look"]
    yaw = math.degrees(math.atan2(ly - y, lx - x))
    actor = actors.spawn_actor_from_class(DESTINATION_CLASS, unreal.Vector(x * M, y * M, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    actor.set_actor_label("PG_Destination_" + d["tag"].split("PathGuide.Place.")[-1].replace(".", "_"))
    actor.set_folder_path(FOLDER)
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="%s")' % d["tag"])  # TagName is read-only from Python
    actor.set_editor_property("place_tag", tag)
    return actor


removed = remove_previous()
created = []

for path in L["paths"]:
    pts = path["pts"]
    if path["name"] == "approach":
        pts = clip_polyline(pts, CLIP_X)
    walkway = spawn_walkway(path["name"], pts)
    if walkway:
        created.append(walkway.get_actor_label())

for conn in PG["connectors"]:
    pts = [[p[0], p[1], p[2] if len(p) > 2 else 0.0] for p in conn["pts"]]
    walkway = spawn_walkway(conn["name"], pts)
    if walkway:
        created.append(walkway.get_actor_label())

for dest in PG["destinations"]:
    created.append(spawn_destination(dest).get_actor_label())

unreal.log("PathGuide placement: removed %d old actor(s), created %d: %s" % (removed, len(created), ", ".join(created)))
