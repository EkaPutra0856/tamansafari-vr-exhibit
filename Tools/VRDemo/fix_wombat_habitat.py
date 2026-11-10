# The x3 wombat hologram crosses its HabitatBounds (the exhibit disables itself): widen the bounds of AR_Wombat_Overlook in L_Zoo. Editor must be CLOSED.
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() != "AR_Wombat_Overlook": continue
    hb = a.get_editor_property("habitat_bounds")
    unreal.log("[WombatHabitat] old min %s max %s scale %s panel %s" % (hb.min, hb.max, a.get_editor_property("animal_mesh").get_editor_property("relative_scale3d"), a.get_editor_property("panel_offset")))
    nb = unreal.Box(unreal.Vector(min(hb.min.x, 400.0), min(hb.min.y, -7200.0), min(hb.min.z, -1800.0)), unreal.Vector(max(hb.max.x, 5400.0), max(hb.max.y, -1800.0), max(hb.max.z, 2600.0)))
    nb.is_valid = True
    a.set_editor_property("habitat_bounds", nb)
    a.set_editor_property("habitat_corner_radius", 300.0)
    a.modify()
    hb2 = a.get_editor_property("habitat_bounds")
    unreal.log("[WombatHabitat] new min %s max %s" % (hb2.min, hb2.max))
    break
unreal.log("[WombatHabitat] saved %s" % les.save_current_level())
