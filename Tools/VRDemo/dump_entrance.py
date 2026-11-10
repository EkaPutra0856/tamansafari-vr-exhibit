# Lists actors around the entrance / plaza (x < -2500) with bounds -> log, to place the opening scene in front of the gate
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def V(v): return "(%d,%d,%d)" % (round(v.x), round(v.y), round(v.z))
def R(r): return "(%d,%d,%d)" % (round(r.pitch), round(r.yaw), round(r.roll))
for a in eas.get_all_level_actors():
    loc = a.get_actor_location()
    if loc.x > -2500 or a.get_class().get_name() in ("InstancedFoliageActor", "Landscape", "LandscapeStreamingProxy"): continue
    try: o, e = a.get_actor_bounds(False)
    except Exception: o = e = unreal.Vector()
    mesh = ""
    smc = a.get_component_by_class(unreal.StaticMeshComponent)
    if smc and smc.static_mesh: mesh = smc.static_mesh.get_name()
    unreal.log("[Entrance] %s | %s | loc %s rot %s | ext %s | %s" % (a.get_actor_label(), a.get_class().get_name(), V(loc), R(a.get_actor_rotation()), V(e), mesh))
unreal.log("[Entrance] done")
