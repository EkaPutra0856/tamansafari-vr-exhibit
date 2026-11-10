# AR_Wombat_Overlook in L_Zoo: hologram scale set to an absolute value and the exhibit pushed a little further from the rail
# (x3 = 6.6 fills the view from 4 m and breaks the info panel). Editor must be CLOSED.
import unreal
SCALE = 4.4
PUSH_X = 120.0
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() != "AR_Wombat_Overlook": continue
    mesh = a.get_editor_property("animal_mesh")
    old = mesh.get_editor_property("relative_scale3d")
    mesh.set_editor_property("relative_scale3d", unreal.Vector(SCALE, SCALE, SCALE))
    loc = a.get_actor_location()
    if loc.x < 2400.0:
        a.set_actor_location(unreal.Vector(loc.x + PUSH_X, loc.y, loc.z), False, False)
    po = a.get_editor_property("panel_offset")
    a.set_editor_property("panel_offset", unreal.Vector(po.x, po.y, 330.0 + 120.0))
    a.modify()
    unreal.log("[WombatAdjust] scale %s -> %s, loc %s -> %s, panel z %s" % (old, mesh.get_editor_property("relative_scale3d"), loc, a.get_actor_location(), a.get_editor_property("panel_offset").z))
    break
unreal.log("[WombatAdjust] saved %s" % les.save_current_level())
