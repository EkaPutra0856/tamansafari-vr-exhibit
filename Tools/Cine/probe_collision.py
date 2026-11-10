# Cine: which meshes of L_Zoo_Cine have collision (for camera clearance), and what the group actors contain.
import unreal, os, json, traceback
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Cine"); os.makedirs(OUT, exist_ok=True)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lines = []
def L(*a): lines.append(" ".join(str(x) for x in a))
try:
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Zoo/Maps/L_Zoo_Cine")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for (x, y) in ((-3750, 0), (961, 1376), (-4600, -1000), (-300, -3100), (2850, -1228)):
        for cplx in (False, True):
            hr = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -3000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, cplx, [], unreal.DrawDebugTrace.NONE, True)
            t = hr.to_tuple() if hr else None
            L("trace", x, y, "complex", cplx, "->", (t[9].get_actor_label() if t and t[9] else None), (round(t[5].z) if t else None), (t[10].get_name() if t and t[10] else None))
        hr = unreal.SystemLibrary.line_trace_single_for_objects(world, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -3000), [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1], True, [], unreal.DrawDebugTrace.NONE, True)
        t = hr.to_tuple() if hr else None
        L("  objects WorldStatic ->", (t[9].get_actor_label() if t and t[9] else None), (round(t[5].z) if t else None))
    for a in eas.get_all_level_actors():
        lab = a.get_actor_label()
        comps = a.get_components_by_class(unreal.PrimitiveComponent)
        if not comps: continue
        summ = {}
        for c in comps:
            cn = c.get_class().get_name()
            mesh = None
            try: mesh = c.get_editor_property("static_mesh")
            except Exception: pass
            n = c.get_instance_count() if isinstance(c, unreal.InstancedStaticMeshComponent) else 1
            col = str(c.get_collision_enabled()).split(".")[-1]
            prof = str(c.get_collision_profile_name())
            key = "%s|%s|%s|%s" % (cn, mesh.get_name() if mesh else "-", col, prof)
            summ[key] = summ.get(key, 0) + n
        L(lab, a.get_class().get_name(), "::", "; ".join("%s x%d" % (k, v) for k, v in summ.items())[:900])
except Exception: L("FATAL", traceback.format_exc())
open(os.path.join(OUT, "probe_collision.txt"), "w").write("\n".join(lines))
unreal.log("[CineProbe] DONE")
