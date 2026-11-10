# Animal hologram profiles (not Duma): ring on the ground, smaller max ring. Saves the assets. Editor must be CLOSED.
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Zoo", "ZooHologramProfile"), True)
for a in assets:
    o = unreal.load_asset(str(a.package_name))
    name = str(a.asset_name)
    animal = "Duma" not in name
    o.set_editor_property("ring_on_ground", animal)
    o.set_editor_property("max_ring_radius", 140.0 if animal else 90.0)
    o.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(o)
    unreal.log("[Profiles] %s: ring_on_ground=%s max_ring_radius=%s ring_scale=%s ambient_seconds=%s" % (name, animal, o.get_editor_property("max_ring_radius"), o.get_editor_property("ring_radius_scale"), o.get_editor_property("ambient_seconds")))
unreal.log("[Profiles] done (%d)" % len(assets))
