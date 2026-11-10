# Makes the AR MEDIA card tall enough that nothing is cut off (its body is not meant to scroll): WBP_AR_Media "Body" SizeBox.
# Saves the widget Blueprint. Editor must be CLOSED.
import unreal
EXTRA = 110.0
path = "/Game/Zoo/Blueprints/Widgets/WBP_AR_Media"
wb = unreal.load_asset(path)
pkg = wb.get_outermost().get_path_name()
done = False
def prop(o, n):
    try: return o.get_editor_property(n)
    except Exception as ex: return "ERR " + str(ex)[:50]
for w in unreal.ObjectIterator(unreal.SizeBox):
    try:
        if w.get_outermost().get_path_name() != pkg or "WidgetTree" not in w.get_outer().get_path_name(): continue
    except Exception: continue
    unreal.log("[MediaCard] %s height_override=%s max_desired_height=%s min_desired_height=%s" % (w.get_name(), prop(w, "height_override"), prop(w, "max_desired_height"), prop(w, "min_desired_height")))
    if w.get_name() != "Body": continue
    h = w.get_editor_property("height_override")
    if h and h > 0:
        w.set_height_override(h + EXTRA)
        unreal.log("[MediaCard] Body height %s -> %s" % (h, h + EXTRA))
    else:
        w.clear_max_desired_height()
        unreal.log("[MediaCard] Body: cleared max desired height")
    done = True
if done:
    wb.modify()
    unreal.log("[MediaCard] saved %s" % unreal.EditorAssetLibrary.save_loaded_asset(wb))
else:
    unreal.log("[MediaCard] Body not found")
