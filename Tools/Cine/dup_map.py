# Cine step 1: duplicate /Game/Zoo/Maps/L_Zoo -> /Game/Zoo/Maps/L_Zoo_Cine (never overwrites an existing copy).
import unreal
SRC, DST = "/Game/Zoo/Maps/L_Zoo", "/Game/Zoo/Maps/L_Zoo_Cine"
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist(DST):
    unreal.log("[CineDup] %s already exists, kept as is" % DST)
else:
    ok = eal.duplicate_asset(SRC, DST)
    unreal.log("[CineDup] duplicate %s -> %s : %s" % (SRC, DST, bool(ok)))
    if ok: eal.save_asset(DST, only_if_is_dirty=False)
unreal.log("[CineDup] exists now: %s" % eal.does_asset_exist(DST))
