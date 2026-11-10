# One-off: moves the VR template hand assets /Game/XRMannequins -> /Game/ZooVR/Hands (references fixed up), drops the Quinn variant.
import unreal
EAL = unreal.EditorAssetLibrary
SRC, DST = "/Game/XRMannequins", "/Game/ZooVR/Hands"
if EAL.does_directory_exist(SRC):
    # Quinn is not used by the demo
    for p in EAL.list_assets(SRC, True, False):
        if "Quinn" in p: unreal.log("[MoveHands] delete " + p + " " + str(EAL.delete_asset(p)))
    ok = EAL.rename_directory(SRC, DST)
    unreal.log("[MoveHands] rename_directory " + str(ok))
    EAL.save_directory(DST, False, True)
    unreal.log("[MoveHands] assets: " + ", ".join(EAL.list_assets(DST, True, False)))
else:
    unreal.log("[MoveHands] source missing")
# ABP variables, for the pose driver
abp = unreal.load_object(None, DST + "/Meshes/ABP_MannequinsXR.ABP_MannequinsXR_C")
if abp:
    cdo = unreal.get_default_object(abp)
    names = [n for n in dir(cdo) if "pose" in n.lower() or "alpha" in n.lower()]
    unreal.log("[MoveHands] ABP pose props: " + ", ".join(names))
unreal.log("[MoveHands] done")
