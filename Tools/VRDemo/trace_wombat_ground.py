# Traces the ground height in front of the wombat overlook (x 2000..2800, y -4500..-4100) -> [WombatGround]
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Zoo/Maps/L_Zoo")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ignore = [a for a in eas.get_all_level_actors() if a.get_actor_label() in ("AR_Wombat_Overlook", "WOM_Zone_Overlook")]
for y in (-4500, -4400, -4300, -4200, -4100):
    row = []
    for x in range(2000, 2860, 80):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 1500), unreal.Vector(x, y, -800), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
        if hit:
            t = hit.to_tuple()
            loc = t[4]; act = t[9] if len(t) > 9 else None
            row.append("%d:%d(%s)" % (x, round(loc.z), act.get_actor_label()[:14] if act else "-"))
        else:
            row.append("%d:none" % x)
    unreal.log("[WombatGround] y %d  %s" % (y, "  ".join(row)))
unreal.log("[WombatGround] done")
