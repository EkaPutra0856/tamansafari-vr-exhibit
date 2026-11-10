# Triples the AR wombat hologram (AR_Wombat_Overlook in L_Zoo): AnimalMesh scale 2.2 -> 6.6, panel a little higher. Saves L_Zoo. Editor must be CLOSED.
import unreal
FACTOR = 3.0
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() != "AR_Wombat_Overlook": continue
    mesh = a.get_editor_property("animal_mesh")
    old = mesh.get_editor_property("relative_scale3d")
    new = unreal.Vector(old.x * FACTOR, old.y * FACTOR, old.z * FACTOR)
    mesh.set_editor_property("relative_scale3d", new)
    # the information panel sits above the hologram: lift it with the animal
    po = a.get_editor_property("panel_offset")
    a.set_editor_property("panel_offset", unreal.Vector(po.x, po.y, po.z + 220.0))
    a.modify()
    unreal.log("[Wombat] AnimalMesh scale %s -> %s, panel offset z %s -> %s" % (old, new, po.z, po.z + 220.0))
    break
ok = les.save_current_level()
unreal.log("[Wombat] saved %s" % ok)
