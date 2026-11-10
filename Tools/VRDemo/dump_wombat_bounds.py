# Logs AR_Wombat_Overlook AnimalMesh world bounds (at its entrance target) and the panel location -> [WombatBounds]
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() != "AR_Wombat_Overlook": continue
    mesh = a.get_editor_property("animal_mesh")
    for rel in (unreal.Vector(0, 0, 0), unreal.Vector(-300, 0, 0)):
        mesh.set_editor_property("relative_location", rel)
        o, e, r = unreal.SystemLibrary.get_component_bounds(mesh)
        unreal.log("[WombatBounds] rel %s -> world origin %s extent %s radius %.0f comp %s" % (rel, o, e, r, mesh.get_world_location()))
    mesh.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
    unreal.log("[WombatBounds] mesh asset %s rel rot %s" % (mesh.get_skeletal_mesh_asset().get_path_name(), mesh.get_editor_property("relative_rotation")))
    ab = mesh.get_skeletal_mesh_asset().get_bounds()
    unreal.log("[WombatBounds] asset bounds origin %s extent %s" % (ab.origin, ab.box_extent))
    p = a.get_editor_property("information_panel")
    unreal.log("[WombatBounds] panel %s" % p.get_world_location())
