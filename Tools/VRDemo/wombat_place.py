# AR_Wombat_Overlook in L_Zoo: puts the wombat hologram's BODY in the middle of the view from the overlook, under the info cards.
# SK_Wombat's geometry sits ~486 units off its pivot along the body axis, so scaling the mesh flings the body away from the
# exhibit (x4.4 put it ~21 m back, behind the fence). Here the pivot is placed so that the body lands on BODY_XY, on the pit
# floor, facing FACING_YAW, and the hologram walks in along its facing. Info card back at its original height; arrival camera focus off
# (the real wombat it would turn to now heads for the far pivot). Level instance only, BP untouched. Editor must be CLOSED.
import math
import unreal

SCALE = 6.6                 # x3 of the original 2.2
BODY_XY = (2950.0, -4320.0)  # centre of the view from the overlook deck (1930,-4350), ~10 deg below the horizon
FACING_YAW = -145.0          # towards the visitor, turned to their left (3/4 view)
WALK_IN = 600.0              # entrance walk length along the facing, cm
WALK_SPEED = 450.0           # cm/s, matches the larger stride
PANEL_Z = 330.0              # the original card height: the body is no longer under the cards, so they need not be lifted

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()

def L(s): unreal.log("[WombatPlace] " + s)

def ground(x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 1500), unreal.Vector(x, y, -800), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else -180.0

for a in actors:
    if a.get_actor_label() != "AR_Wombat_Overlook": continue
    mesh = a.get_editor_property("animal_mesh")
    root = a.get_editor_property("hologram_root")
    c = mesh.get_skeletal_mesh_asset().get_bounds().origin
    floor = ground(BODY_XY[0], BODY_XY[1], [a])

    # mesh world yaw so that the walk (local +Y at yaw 0 in mesh space) points along FACING_YAW
    mesh_yaw = FACING_YAW - 90.0
    r = math.radians(mesh_yaw)
    off_x = (c.x * math.cos(r) - c.y * math.sin(r)) * SCALE
    off_y = (c.x * math.sin(r) + c.y * math.cos(r)) * SCALE
    pivot = unreal.Vector(BODY_XY[0] - off_x, BODY_XY[1] - off_y, floor)
    f = math.radians(FACING_YAW)
    start = unreal.Vector(pivot.x - math.cos(f) * WALK_IN, pivot.y - math.sin(f) * WALK_IN, floor)

    root_xf = root.get_world_transform()
    rel_target = root_xf.inverse_transform_location(pivot)
    rel_start = root_xf.inverse_transform_location(start)
    rel_yaw = mesh_yaw - root_xf.rotation.rotator().yaw
    rel_yaw = (rel_yaw + 180.0) % 360.0 - 180.0

    mesh.set_editor_property("relative_scale3d", unreal.Vector(SCALE, SCALE, SCALE))
    mesh.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=rel_yaw))
    mesh.set_editor_property("relative_location", rel_target)
    a.set_editor_property("entrance_target_location", rel_target)
    a.set_editor_property("entrance_start_location", rel_start)
    a.set_editor_property("entrance_move_speed", WALK_SPEED)
    po = a.get_editor_property("panel_offset")
    a.set_editor_property("panel_offset", unreal.Vector(po.x, po.y, PANEL_Z))
    # the arrival focus turns the view to the reserved real wombat, which now heads for the far pivot: the visitor looks at the hologram instead
    a.set_editor_property("enable_arrival_camera_focus", False)
    a.modify()

    body = mesh.get_world_transform().transform_location(c)
    L("floor %.0f pivot %s rel yaw %.1f target %s start %s" % (floor, pivot, rel_yaw, rel_target, rel_start))
    L("body centre now %s (wanted %s)" % (body, BODY_XY))
    for w in actors:
        if w.get_class().get_name() == "BP_ARWombat_C":
            L("real %s %.0f cm from the new animal destination" % (w.get_actor_label(), (w.get_actor_location() - pivot).length()))
    break

L("saved %s" % les.save_current_level())
