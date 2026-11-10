# Creates / refreshes the Path Guide data assets from zoo_layout.json.
#   /Game/PathGuide/Data/DA_PG_Places  - the destination list (names, numbers, radii, map)
#   /Game/PathGuide/Data/DA_PG_Config  - all tunables (class defaults) + input assets
# Run inside the editor's Python:  exec(open(r"D:/UEProjects/Zoo/Tools/PathGuide/build_data_assets.py").read())
# Thumbnails, materials and sounds are assigned by later steps (assign_assets.py) once those assets exist.
import json

import unreal

LAYOUT = r"D:/UEProjects/Zoo/Tools/ZooTerrain/zoo_layout.json"
FOLDER = "/Game/PathGuide/Data"
MAP = "/Game/Zoo/Maps/L_Zoo.L_Zoo"
M = 100.0

lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def get_or_create(name, cls):
    lib.make_directory(FOLDER)
    path = FOLDER + "/" + name
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, FOLDER, cls, factory)


def text(ns, key, value):
    return 'NSLOCTEXT("%s","%s","%s")' % (ns, key, value.replace('"', '\\"'))


# ---------------------------------------------------------------- places
dests = json.load(open(LAYOUT))["path_guide"]["destinations"]
places_asset = get_or_create("DA_PG_Places", unreal.PathGuidePlacesAsset)

places = []
for d in sorted(dests, key=lambda d: d["number"]):
    key = d["tag"].split("PathGuide.Place.")[-1]
    place = unreal.PathGuidePlace()
    place.import_text(
        '(PlaceTag=(TagName="%s"),Number=%d,DisplayName=%s,Description=%s,Map="%s",ArrivalRadius=%f)'
        % (d["tag"], d["number"], text("PathGuidePlaces", key + ".Name", d["name"]),
           text("PathGuidePlaces", key + ".Description", d["description"]), MAP, d["radius_m"] * M))
    places.append(place)

places_asset.set_editor_property("places", places)
lib.save_loaded_asset(places_asset)

# ---------------------------------------------------------------- config
config_asset = get_or_create("DA_PG_Config", unreal.PathGuideConfig)
for prop, path in (
    ("input_mapping_context", "/Game/PathGuide/Input/IMC_PG"),
    ("toggle_menu_action", "/Game/PathGuide/Input/IA_PG_ToggleMenu"),
    ("close_menu_action", "/Game/PathGuide/Input/IA_PG_CloseMenu"),
    ("show_cursor_action", "/Game/PathGuide/Input/IA_PG_ShowCursor"),
):
    config_asset.set_editor_property(prop, lib.load_asset(path))
lib.save_loaded_asset(config_asset)

for p in places_asset.get_editor_property("places"):
    print(p.get_editor_property("number"), p.get_editor_property("place_tag").export_text(),
          p.get_editor_property("display_name"), p.get_editor_property("arrival_radius"))
print("config IMC:", config_asset.get_editor_property("input_mapping_context"))
