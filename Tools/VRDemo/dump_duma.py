# Dumps BP_DumaController / BP_Duma defaults relevant to Duma's placement -> Saved/VRDemo/duma.json
import unreal, os, json
OUT = os.path.join(unreal.Paths.project_saved_dir(), "VRDemo")
res = {}
def cdo(path):
    c = unreal.load_object(None, path)
    return unreal.get_default_object(c) if c else None
for name, path in (("BP_DumaController", "/Game/Zoo/Blueprints/Duma/BP_DumaController.BP_DumaController_C"), ("BP_Duma", "/Game/Zoo/Blueprints/Duma/BP_Duma.BP_Duma_C")):
    o = cdo(path)
    d = {}
    if o:
        for n in ("follow_offset", "facing_offset", "follow_stiffness", "turn_speed", "view_half_angle", "min_ahead", "lost_distance"):
            try: d[n] = str(o.get_editor_property(n))
            except Exception as ex: d[n] = "ERR " + str(ex)[:60]
    res[name] = d
with open(os.path.join(OUT, "duma.json"), "w") as f: json.dump(res, f, indent=1)
unreal.log("[DumaDump] " + json.dumps(res))
