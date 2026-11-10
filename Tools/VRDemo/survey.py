# VR demo: read-only survey of L_Zoo + assets -> Saved/VRDemo/survey.json + survey.txt
import unreal, os, json, traceback, collections
OUT = os.path.join(unreal.Paths.project_saved_dir(), "VRDemo")
os.makedirs(OUT, exist_ok=True)
lines = []
def L(*a):
    s = " ".join(str(x) for x in a); lines.append(s); unreal.log("[VRSurvey] " + s)
def V(v):
    try: return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]
    except Exception: return str(v)
def R(r):
    try: return [round(r.pitch, 1), round(r.yaw, 1), round(r.roll, 1)]
    except Exception: return str(r)
def prop(o, n, d=None):
    try: return o.get_editor_property(n)
    except Exception: return d
def S(x):
    try: return str(x)
    except Exception: return "?"
res = {}
def conv(v):
    if isinstance(v, unreal.Vector): return V(v)
    if isinstance(v, unreal.Rotator): return R(v)
    if isinstance(v, (unreal.Text, unreal.Name)): return str(v)
    if isinstance(v, unreal.Object): return v.get_path_name()
    return S(v)
def dump_props(o, names):
    d = {}
    for n in names:
        v = prop(o, n, "<none>")
        if isinstance(v, str) and v == "<none>": continue
        d[n] = conv(v)
    return d
try:
    # ---------------- level ----------------
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    L("load_level", les.load_level("/Game/Zoo/Maps/L_Zoo"))
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()
    L("actors", len(actors))
    by_class = collections.Counter(a.get_class().get_name() for a in actors)
    res["classes"] = by_class.most_common()
    notable = []
    keys = ("PlayerStart", "ZooARExhibit", "PathGuide", "Duma", "ZooAnimal", "RecastNavMesh", "NavMesh", "Zone", "Trigger", "Giraffe", "Wombat", "Elephant", "Jerapah", "Gate", "Gerbang", "Sign", "Volume", "Sequence", "AnimalDetection", "BP_Animal", "Spawn", "Walkway", "Destination")
    for a in actors:
        cls = a.get_class().get_name(); lab = a.get_actor_label()
        parents = []
        c = a.get_class()
        for _ in range(6):
            try: c = c.get_super_class()
            except Exception: c = None
            if not c: break
            parents.append(c.get_name())
        chain = cls + ">" + ">".join(parents)
        if not any(k.lower() in (lab + chain).lower() for k in keys): continue
        d = {"label": lab, "class": cls, "chain": chain, "folder": str(a.get_folder_path()), "loc": V(a.get_actor_location()), "rot": R(a.get_actor_rotation()), "tags": [str(t) for t in (prop(a, "tags", []) or [])]}
        try:
            o, e = a.get_actor_bounds(False); d["bounds_o"] = V(o); d["ext"] = V(e)
        except Exception: pass
        if "ZooARExhibit" in chain:
            d["ex"] = dump_props(a, ["animal_name", "scientific_name", "activation_mode", "walk_to_deck_center_before_approach", "deck_center_offset", "habitat_bounds", "habitat_corner_radius", "panel_offset", "panel_world_scale", "information_widget_class", "entrance_start_location", "entrance_target_location", "max_approach_distance", "animal_arrival_timeout"])
            vz = prop(a, "viewing_zone")
            if vz:
                box = vz.get_component_by_class(unreal.BoxComponent)
                d["zone"] = {"label": vz.get_actor_label(), "loc": V(vz.get_actor_location()), "rot": R(vz.get_actor_rotation())}
                if box:
                    d["zone"]["box_loc"] = V(box.get_world_location()); d["zone"]["box_rot"] = R(box.get_world_rotation())
                    d["zone"]["ext_scaled"] = V(box.get_scaled_box_extent())
            hr = prop(a, "hologram_root")
            if hr: d["hologram_root"] = V(hr.get_world_location())
            ip = prop(a, "information_panel")
            if ip: d["info_panel"] = {"loc": V(ip.get_world_location()), "draw": S(prop(ip, "draw_size")), "scale": V(ip.get_world_scale())}
        if "Destination" in chain or "Destination" in lab:
            d["pg"] = dump_props(a, ["place_tag", "arrival_radius"])
        sp = a.get_component_by_class(unreal.SplineComponent)
        if sp:
            n = sp.get_number_of_spline_points()
            d["spline"] = [V(sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)) for i in range(n)]
        if "PlayerStart" in chain:
            d["ps"] = dump_props(a, ["player_start_tag"])
        if "Animal" in chain:
            d["an"] = dump_props(a, ["exhibit_tag", "animal_tag", "species", "exhibit"])
        notable.append(d)
    res["notable"] = notable
    L("notable", len(notable))
    # ---------------- assets: input ----------------
    def imc(path):
        m = unreal.load_asset(path)
        out = []
        if not m: return out
        for mp in prop(m, "mappings", []):
            act = prop(mp, "action"); key = prop(mp, "key")
            kn = str(prop(key, "key_name")) if key else "?"
            trig = [t.get_class().get_name() for t in (prop(mp, "triggers", []) or [])]
            mods = [t.get_class().get_name() for t in (prop(mp, "modifiers", []) or [])]
            out.append({"action": act.get_path_name() if act else None, "key": kn, "triggers": trig, "modifiers": mods})
        return out
    res["IMC_ZooVR"] = imc("/Game/ZooVR/Input/IMC_ZooVR")
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in ar.get_assets_by_path("/Game/Zoo/Input", True):
        if str(a.asset_class_path.asset_name) == "InputMappingContext":
            p = str(a.package_name); res["IMC:" + p] = imc(p)
    res["input_actions"] = [str(a.package_name) for a in ar.get_assets_by_path("/Game/Zoo/Input", True) if str(a.asset_class_path.asset_name) == "InputAction"]
    # ---------------- BP defaults ----------------
    def cdo(path):
        c = unreal.load_object(None, path if path.endswith("_C") else path + "_C")
        return unreal.get_default_object(c) if c else None
    o = cdo("/Game/ZooVR/Blueprints/BP_ZooVRComponent.BP_ZooVRComponent")
    if o: res["BP_ZooVRComponent"] = dump_props(o, ["vr_mapping_context", "move_action", "snap_turn_action", "select_action", "menu_action", "scroll_action", "relay_widget_classes", "hidden_widget_names", "panel_material", "panel_distance", "panel_width", "panel_draw_size", "vr_console_variables", "prompt_replacements", "pointer_distance"])
    o = cdo("/Game/ZooVR/Blueprints/BP_ZooVRGameMode.BP_ZooVRGameMode")
    if o: res["BP_ZooVRGameMode"] = dump_props(o, ["vr_component_class", "default_pawn_class", "player_controller_class", "hud_class"])
    for a in ar.get_assets_by_path("/Game/Zoo/Blueprints", True):
        n = str(a.asset_name)
        if n in ("BP_ZooPlayerController", "BP_Zoo_HUD", "BP_ZooCharacter", "BP_ZooGameMode"):
            o = cdo(str(a.package_name) + "." + n)
            if o: res[n] = dump_props(o, ["default_mapping_context", "move_action", "look_action", "jump_action", "sprint_action", "dialogue_skip_action", "path_guide_mapping_context", "toggle_menu_action", "close_menu_action", "show_cursor_action", "ar_visit_events", "path_guide_widget_class", "narrative_widget_class", "path_guide_widget_controller_class", "walk_speed", "sprint_speed", "eye_height", "default_pawn_class", "player_controller_class", "hud_class"])
    # ---------------- widgets ----------------
    def widget_tree(path):
        wb = unreal.load_asset(path)
        if not wb: return None
        tree = prop(wb, "widget_tree")
        out = []
        for w in unreal.ObjectIterator(unreal.Widget):
            try:
                if w.get_outer() != tree: continue
            except Exception: continue
            e = {"name": w.get_name(), "class": w.get_class().get_name()}
            try:
                par = w.get_parent()
                if par: e["parent"] = par.get_name()
            except Exception: pass
            if isinstance(w, (unreal.TextBlock, unreal.RichTextBlock)):
                e["text"] = str(prop(w, "text", ""))
            vis = prop(w, "visibility")
            if vis is not None: e["vis"] = S(vis)
            out.append(e)
        return out
    for p in ["/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PathGuide", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_DestinationList", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_DestinationCard", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_ArrivalToast", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_NavigateButton", "/Game/Zoo/Assets/Narrative/Widgets/WBP_Zoo_NarrativeOverlay", "/Game/Zoo/Blueprints/Widgets/WBP_ARAnimalInfo", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Media", "/Game/Zoo/Blueprints/Widgets/WBP_AR_MediaExtend", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Characteristic", "/Game/Zoo/Blueprints/Widgets/WBP_AR_GeneralDescription", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Habitat"]:
        try: res["W:" + p.split("/")[-1]] = widget_tree(p)
        except Exception as ex: L("widget fail", p, ex)
    # ---------------- place cards / places ----------------
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Zoo", "ZooPlaceCardsAsset"), True):
        o = unreal.load_asset(str(a.package_name))
        cards = []
        for c in prop(o, "cards", []):
            cards.append({"tag": S(prop(c, "place_tag")), "number": prop(c, "number"), "desc": str(prop(c, "description", ""))[:80]})
        res["PlaceCards:" + str(a.asset_name)] = cards
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/ExplorationSystem", "PathGuidePlacesAsset"), True):
        o = unreal.load_asset(str(a.package_name))
        pl = []
        for c in prop(o, "places", []):
            pl.append({"tag": S(prop(c, "place_tag")), "name": str(prop(c, "display_name", "")), "radius": prop(c, "arrival_radius")})
        res["Places:" + str(a.asset_name)] = pl
    # ---------------- narrative / dialogue assets ----------------
    for path, key in (("/Game/Zoo/Assets/Narrative", "narrative_assets"), ("/Game/Zoo/Assets/Duma/Dialogues", "dialogue_assets"), ("/Game/Zoo/Assets/Dialogue", "dialogue_assets2")):
        for a in ar.get_assets_by_path(path, True):
            res.setdefault(key, []).append(str(a.package_name) + " : " + str(a.asset_class_path.asset_name))
except Exception:
    L("EXC", traceback.format_exc())
with open(os.path.join(OUT, "survey.json"), "w", encoding="utf-8") as f: json.dump(res, f, indent=1, ensure_ascii=False, default=str)
with open(os.path.join(OUT, "survey.txt"), "w", encoding="utf-8") as f: f.write("\n".join(lines))
L("done")
