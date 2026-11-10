# Cine: why do world traces miss static meshes in the commandlet? body setup / nanite / per-component trace
import unreal, os, traceback
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Cine")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lines = []
def L(*a): lines.append(" ".join(str(x) for x in a))
try:
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Zoo/Maps/L_Zoo_Cine")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    acts = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    for lab in ("ENT_Pendopo", "ENT_Tusk_W", "GIR_Deck_Floor", "REST_Gazebo", "VEG_Specimens", "GIR_Trees"):
        a = acts[lab]
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property("static_mesh")
            bs = sm.get_editor_property("body_setup") if sm else None
            info = []
            if bs:
                try: info.append("simple %s complexity %s" % (unreal.EditorStaticMeshLibrary.get_simple_collision_count(sm), unreal.EditorStaticMeshLibrary.get_collision_complexity(sm)))
                except Exception as ex: info.append("simple ? %s" % ex)
                info.append("flag %s" % bs.get_editor_property("collision_trace_flag"))
            try: info.append("nanite %s" % sm.get_editor_property("nanite_settings").get_editor_property("enabled"))
            except Exception as e: info.append("nanite ? %s" % e)
            org, ext = c.get_local_bounds() if hasattr(c, "get_local_bounds") else (None, None)
            b = c.get_editor_property("bounds") if False else None
            o, e, r = unreal.SystemLibrary.get_component_bounds(c)
            s = o + unreal.Vector(0, 0, e.z + 500); t = o - unreal.Vector(0, 0, e.z + 500)
            try:
                res = c.k2_line_trace_component(s, t, True, False, False)
                info.append("k2trace %s" % (res[0] if isinstance(res, tuple) else res))
            except Exception as ex: info.append("k2trace ERR %s" % ex)
            hr = unreal.SystemLibrary.sphere_trace_single(world, s, t, 30.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            tt = hr.to_tuple() if hr else None
            info.append("sphere %s" % ((tt[9].get_actor_label() if tt and tt[9] else None)))
            info.append("physstate %s" % c.is_physics_state_created() if hasattr(c, "is_physics_state_created") else "")
            L(lab, c.get_name(), sm.get_name() if sm else None, "bounds", [round(o.x), round(o.y), round(o.z)], [round(e.x), round(e.y), round(e.z)], " | ".join(info))
except Exception: L("FATAL", traceback.format_exc())
open(os.path.join(OUT, "probe_collision2.txt"), "w").write("\n".join(lines))
