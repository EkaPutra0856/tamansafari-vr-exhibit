# Dumps canvas slot layouts of the narrative overlay and path guide children -> log
import unreal, json
def prop(o, n, d=None):
    try: return o.get_editor_property(n)
    except Exception: return d
for path in ("/Game/Zoo/Assets/Narrative/Widgets/WBP_Zoo_NarrativeOverlay", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PathGuide"):
    wb = unreal.load_asset(path)
    pkg = wb.get_outermost().get_path_name()
    for w in unreal.ObjectIterator(unreal.Widget):
        try:
            if w.get_outermost().get_path_name() != pkg or "WidgetTree" not in w.get_outer().get_path_name(): continue
        except Exception: continue
        slot = prop(w, "slot")
        if isinstance(slot, unreal.CanvasPanelSlot):
            ld = slot.get_layout()
            a = ld.anchors
            unreal.log("[Slots] %s %s anchors=(%s,%s)-(%s,%s) offsets=(%s,%s,%s,%s) align=(%s,%s) autosize=%s" % (path.split("/")[-1], w.get_name(), a.minimum.x, a.minimum.y, a.maximum.x, a.maximum.y, ld.offsets.left, ld.offsets.top, ld.offsets.right, ld.offsets.bottom, ld.alignment.x, ld.alignment.y, slot.get_auto_size()))
unreal.log("[Slots] done")
