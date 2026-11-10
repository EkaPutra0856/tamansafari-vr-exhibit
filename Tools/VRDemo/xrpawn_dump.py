# Dumps BP_XRPawn's hand components (relative transforms) and ABP_MannequinsXR variables -> <Saved>/VRDemo/xrpawn.json
import unreal, os, json, traceback
OUT = os.path.join(unreal.Paths.project_saved_dir(), "VRDemo")
os.makedirs(OUT, exist_ok=True)
res = {}
def prop(o, n, d=None):
    try: return o.get_editor_property(n)
    except Exception: return d
def V(v): return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]
def R(r): return [round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2)]
try:
    bp = unreal.load_asset("/Game/XRFramework/Blueprints/BP_XRPawn")
    comps = []
    if bp:
        scs = prop(bp, "simple_construction_script")
        # SubobjectDataSubsystem path
        sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = sds.k2_gather_subobject_data_for_blueprint(bp)
        for h in handles:
            d = sds.k2_find_subobject_data_from_handle(h)
            obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
            if not obj: continue
            e = {"name": unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(d).__str__(), "class": obj.get_class().get_name()}
            if isinstance(obj, unreal.SceneComponent):
                e["rel_loc"] = V(obj.get_editor_property("relative_location")); e["rel_rot"] = R(obj.get_editor_property("relative_rotation")); e["rel_scale"] = V(obj.get_editor_property("relative_scale3d"))
            if isinstance(obj, unreal.SkeletalMeshComponent):
                sm = prop(obj, "skeletal_mesh_asset") or prop(obj, "skeletal_mesh")
                e["mesh"] = sm.get_path_name() if sm else None
                ac = prop(obj, "anim_class"); e["anim_class"] = ac.get_path_name() if ac else None
            if isinstance(obj, unreal.MotionControllerComponent):
                e["motion_source"] = str(prop(obj, "motion_source"))
            try:
                ph = unreal.SubobjectDataBlueprintFunctionLibrary.get_parent_handle(d) if hasattr(unreal.SubobjectDataBlueprintFunctionLibrary, "get_parent_handle") else None
            except Exception: ph = None
            comps.append(e)
    res["BP_XRPawn_components"] = comps
    abp = unreal.load_asset("/Game/XRMannequins/Meshes/ABP_MannequinsXR")
    if abp:
        c = unreal.load_object(None, "/Game/XRMannequins/Meshes/ABP_MannequinsXR.ABP_MannequinsXR_C")
        cdo = unreal.get_default_object(c) if c else None
        vars_ = []
        for nv in prop(abp, "new_variables", []) or []:
            vars_.append({"name": str(prop(nv, "var_name")), "type": str(prop(prop(nv, "var_type"), "pin_category"))})
        res["ABP_vars"] = vars_
        if cdo:
            names = ["PoseAlphaGrasp", "PoseAlphaIndexCurl", "PoseAlphaPoint", "PoseAlphaThumbUp", "Pose_Alpha_Grasp"]
            res["ABP_cdo_props"] = {n: str(prop(cdo, n, "<none>")) for n in names}
    skm = unreal.load_asset("/Game/XRMannequins/Meshes/SKM_MannyXR_right")
    if skm:
        sk = prop(skm, "skeleton")
        bones = []
        try:
            n = skm.get_editor_property("ref_skeleton")  # may fail
        except Exception: pass
        try:
            # bone names via skeleton helpers
            names = unreal.SkeletalMeshEditorSubsystem and None
        except Exception: pass
        res["SKM_right"] = {"path": skm.get_path_name(), "skeleton": sk.get_path_name() if sk else None, "bounds": str(skm.get_bounds().box_extent) if hasattr(skm, "get_bounds") else "?"}
        try:
            res["SKM_right"]["sockets"] = [str(s.socket_name) for s in skm.get_editor_property("sockets")]
        except Exception as ex: res["SKM_right"]["sockets_err"] = str(ex)
except Exception:
    res["EXC"] = traceback.format_exc()
with open(os.path.join(OUT, "xrpawn.json"), "w", encoding="utf-8") as f: json.dump(res, f, indent=1, default=str)
unreal.log("[XRPawnDump] done")
