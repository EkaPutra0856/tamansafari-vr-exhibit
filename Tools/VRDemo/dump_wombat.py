# Logs the AR wombat exhibit placement (actor, hologram root, mesh, entrance points, viewing zone, real wombats) -> [WombatDump]
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def L(s): unreal.log("[WombatDump] " + s)
for a in eas.get_all_level_actors():
    lab = a.get_actor_label()
    if "ombat" not in lab and "Overlook" not in lab: continue
    L("%s | %s | loc %s rot %s scale %s" % (lab, a.get_class().get_name(), a.get_actor_location(), a.get_actor_rotation(), a.get_actor_scale3d()))
    if lab == "AR_Wombat_Overlook":
        for p in ("entrance_start_location", "entrance_target_location", "panel_offset", "activation_mode", "b_animate_entrance_movement", "habitat_bounds", "viewing_zone", "max_approach_distance"):
            try: L("  %s = %s" % (p, a.get_editor_property(p)))
            except Exception as e: L("  %s ? %s" % (p, e))
        for c in ("hologram_root", "animal_mesh"):
            comp = a.get_editor_property(c)
            L("  %s rel %s world %s scale %s" % (c, comp.get_editor_property("relative_location"), comp.get_world_location(), comp.get_editor_property("relative_scale3d")))
        vz = a.get_editor_property("viewing_zone")
        if vz: L("  zone %s loc %s" % (vz.get_actor_label(), vz.get_actor_location()))
L("done")
