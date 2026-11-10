# VR demo survey part 2: exhibits, IMC key mappings, widget trees, narrative director, dialogues, XR pawn hands -> Saved/VRDemo/survey2.json
import unreal, os, json, traceback
OUT = os.path.join(unreal.Paths.project_saved_dir(), "VRDemo")
os.makedirs(OUT, exist_ok=True)
res = {}
def prop(o, n, d=None):
    try: return o.get_editor_property(n)
    except Exception: return d
def V(v): return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]
def R(r): return [round(r.pitch, 1), round(r.yaw, 1), round(r.roll, 1)]
def deep(v, depth=3):
    try:
        if v is None or isinstance(v, (bool, int, float, str)): return v
        if isinstance(v, unreal.Vector): return V(v)
        if isinstance(v, unreal.Rotator): return R(v)
        if isinstance(v, unreal.GameplayTag): return str(v.tag_name)
        if isinstance(v, (unreal.Text, unreal.Name)): return str(v)
        if isinstance(v, unreal.Class): return v.get_path_name()
        if isinstance(v, unreal.Object):
            if depth <= 0: return v.get_path_name()
            return {"__obj": v.get_path_name(), **{n: deep(getattr_prop(v, n), depth - 1) for n in props_of(v)}}
        if isinstance(v, (unreal.Array, unreal.Set, list, tuple)):
            return [deep(x, depth - 1) for x in list(v)[:40]]
        if isinstance(v, (unreal.Map, dict)):
            return {str(deep(k, 0)): deep(x, depth - 1) for k, x in dict(v).items()}
        if isinstance(v, unreal.StructBase):
            if depth <= 0: return str(v)[:120]
            return {n: deep(getattr_prop(v, n), depth - 1) for n in props_of(v)}
        return str(v)[:200]
    except Exception as ex:
        return "ERR:" + str(ex)[:80]
def props_of(o):
    out = []
    for n in dir(o):
        if n.startswith("_") or n in ("cast", "static_class", "get_default_object", "get_editor_property", "set_editor_property"): continue
        try:
            a = getattr(type(o), n, None)
            if callable(a): continue
        except Exception: continue
        out.append(n)
    return out[:80]
def getattr_prop(o, n):
    try: return o.get_editor_property(n)
    except Exception:
        try: return getattr(o, n)
        except Exception: return None
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level("/Game/Zoo/Maps/L_Zoo")
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()
    ex = []
    for a in actors:
        if not isinstance(a, unreal.ZooARExhibit): continue
        d = {"label": a.get_actor_label(), "loc": V(a.get_actor_location()), "rot": R(a.get_actor_rotation()), "tags": [str(t) for t in prop(a, "tags", [])]}
        for n in ["animal_name", "scientific_name", "activation_mode", "walk_to_deck_center_before_approach", "deck_center_offset", "habitat_bounds", "habitat_corner_radius", "panel_offset", "panel_world_scale", "information_widget_class", "entrance_start_location", "entrance_target_location", "max_approach_distance", "animal_arrival_timeout", "enable_arrival_camera_focus", "reveal_duration"]:
            d[n] = deep(prop(a, n), 1)
        vz = prop(a, "viewing_zone")
        if vz:
            d["zone"] = {"label": vz.get_actor_label(), "loc": V(vz.get_actor_location())}
            box = vz.get_component_by_class(unreal.BoxComponent)
            if box: d["zone"].update({"box_loc": V(box.get_world_location()), "box_rot": R(box.get_world_rotation()), "ext_scaled": V(box.get_scaled_box_extent())})
        for cn in ("hologram_root", "information_panel", "animal_mesh", "animal_click_bounds"):
            c = prop(a, cn)
            if c: d[cn] = {"loc": V(c.get_world_location()), "rot": R(c.get_world_rotation()), "scale": V(c.get_world_scale())}
        ip = prop(a, "information_panel")
        if ip: d["panel_draw"] = str(prop(ip, "draw_size")); d["panel_widget_class"] = deep(prop(ip, "widget_class"), 0)
        ex.append(d)
    res["exhibits"] = ex
    # animals
    an = []
    for a in actors:
        if isinstance(a, unreal.ZooAnimal) or "Giraffe" in a.get_class().get_name() or "Wombat" in a.get_class().get_name():
            an.append({"label": a.get_actor_label(), "class": a.get_class().get_name(), "loc": V(a.get_actor_location()), "is_zoo_animal": isinstance(a, unreal.ZooAnimal)})
    res["animals"] = an
    # zones
    res["trigger_boxes"] = []
    for a in actors:
        if isinstance(a, unreal.TriggerBox):
            box = a.get_component_by_class(unreal.BoxComponent)
            res["trigger_boxes"].append({"label": a.get_actor_label(), "loc": V(a.get_actor_location()), "ext": V(box.get_scaled_box_extent()) if box else None, "rot": R(a.get_actor_rotation())})
    # destinations with tags
    res["destinations"] = [{"label": a.get_actor_label(), "loc": V(a.get_actor_location()), "tag": deep(prop(a, "place_tag"), 0), "radius": prop(a, "arrival_radius")} for a in actors if a.get_class().get_name() == "WzPathGuideDestination"]
    # ---------------- IMC key mappings ----------------
    def imc(path):
        m = unreal.load_asset(path)
        out = {"raw": None}
        if not m: return out
        dk = prop(m, "default_key_mappings")
        out["dk_type"] = type(dk).__name__ if dk is not None else None
        maps = None
        if dk is not None:
            for n in ("mappings", "key_mappings", "action_key_mappings"):
                maps = prop(dk, n)
                if maps is not None: break
            if maps is None:
                out["dk_deep"] = deep(dk, 3)
        if maps is None:
            maps = prop(m, "mappings")
        rows = []
        for mp in (maps or []):
            act = prop(mp, "action"); key = prop(mp, "key")
            rows.append({"action": act.get_path_name() if act else None, "key": str(prop(key, "key_name")) if key else "?", "triggers": [t.get_class().get_name() for t in (prop(mp, "triggers", []) or [])], "modifiers": [t.get_class().get_name() for t in (prop(mp, "modifiers", []) or [])]})
        out["rows"] = rows
        return out
    for p in ("/Game/ZooVR/Input/IMC_ZooVR", "/Game/Zoo/Input/IMC_Zoo", "/Game/Zoo/Input/IMC_PG"):
        res["IMC:" + p.split("/")[-1]] = imc(p)
    # ---------------- widget trees ----------------
    def widget_tree(path):
        wb = unreal.load_asset(path)
        if not wb: return None
        pkg = wb.get_outermost().get_path_name()
        out = []
        for w in unreal.ObjectIterator(unreal.Widget):
            try:
                if w.get_outermost().get_path_name() != pkg: continue
                if "WidgetTree" not in w.get_outer().get_path_name(): continue
            except Exception: continue
            e = {"name": w.get_name(), "class": w.get_class().get_name()}
            try:
                par = w.get_parent()
                if par: e["parent"] = par.get_name()
            except Exception: pass
            if isinstance(w, (unreal.TextBlock, unreal.RichTextBlock)): e["text"] = str(prop(w, "text", ""))
            vis = prop(w, "visibility")
            if vis is not None: e["vis"] = str(vis).split(".")[-1]
            out.append(e)
        return out
    for p in ["/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PathGuide", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_DestinationList", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_DestinationCard", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_ArrivalToast", "/Game/Zoo/Blueprints/Widgets/PathGuide/WBP_PG_NavigateButton", "/Game/Zoo/Assets/Narrative/Widgets/WBP_Zoo_NarrativeOverlay", "/Game/Zoo/Blueprints/Widgets/WBP_ARAnimalInfo", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Media", "/Game/Zoo/Blueprints/Widgets/WBP_AR_MediaExtend", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Characteristic", "/Game/Zoo/Blueprints/Widgets/WBP_AR_GeneralDescription", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Habitat"]:
        try: res["W:" + p.split("/")[-1]] = widget_tree(p)
        except Exception as ex: res["W:" + p.split("/")[-1]] = "ERR " + str(ex)
    # ---------------- narrative director + dialogues ----------------
    dr = unreal.load_asset("/Game/Zoo/Assets/Narrative/DA_Zoo_Director")
    res["director_props"] = props_of(dr) if dr else None
    res["director"] = deep(dr, 5) if dr else None
    for p in ["DA_Duma_Greeting", "DA_Duma_Navigation", "DA_Duma_GiraffeDeck", "DA_Duma_WombatOverlook"]:
        o = unreal.load_asset("/Game/Zoo/Assets/Duma/Dialogues/" + p)
        res["DLG:" + p] = deep(o, 4) if o else None
    # ---------------- place cards tags ----------------
    pc = unreal.load_asset("/Game/Zoo/PathGuide/DA_ZooPlaceCards") or None
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Zoo", "ZooPlaceCardsAsset"), True):
        res["PlaceCards"] = deep(unreal.load_asset(str(a.package_name)), 3)
    for a in ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/ExplorationSystem", "PathGuidePlacesAsset"), True):
        res["Places:" + str(a.asset_name)] = deep(unreal.load_asset(str(a.package_name)), 3)
    # ---------------- XR pawn hands ----------------
    bp = unreal.load_asset("/Game/XRFramework/Blueprints/BP_XRPawn")
    comps = []
    if bp:
        sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        for h in sds.k2_gather_subobject_data_for_blueprint(bp):
            d = sds.k2_find_subobject_data_from_handle(h)
            obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
            if not obj: continue
            e = {"name": str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(d)), "class": obj.get_class().get_name()}
            if isinstance(obj, unreal.SceneComponent):
                e["rel_loc"] = V(prop(obj, "relative_location")); e["rel_rot"] = R(prop(obj, "relative_rotation")); e["rel_scale"] = V(prop(obj, "relative_scale3d"))
                try:
                    pd = sds.k2_find_subobject_data_from_handle(unreal.SubobjectDataBlueprintFunctionLibrary.get_parent_handle(d)) if hasattr(unreal.SubobjectDataBlueprintFunctionLibrary, "get_parent_handle") else None
                except Exception: pd = None
            if isinstance(obj, unreal.SkeletalMeshComponent):
                sm = prop(obj, "skeletal_mesh_asset"); e["mesh"] = sm.get_path_name() if sm else None
                ac = prop(obj, "anim_class"); e["anim_class"] = ac.get_path_name() if ac else None
            if isinstance(obj, unreal.MotionControllerComponent): e["motion_source"] = str(prop(obj, "motion_source"))
            comps.append(e)
    res["BP_XRPawn_components"] = comps
    abp = unreal.load_asset("/Game/XRMannequins/Meshes/ABP_MannequinsXR")
    if abp:
        res["ABP_vars"] = [{"name": str(prop(nv, "var_name")), "cat": str(prop(prop(nv, "var_type"), "pin_category"))} for nv in (prop(abp, "new_variables", []) or [])]
    skm = unreal.load_asset("/Game/XRMannequins/Meshes/SKM_MannyXR_right")
    if skm:
        sk = prop(skm, "skeleton")
        res["SKM_right"] = {"skeleton": sk.get_path_name() if sk else None, "bounds": str(skm.get_bounds().box_extent), "sockets": [str(prop(s, "socket_name")) for s in (prop(skm, "sockets", []) or [])]}
        try:
            res["SKM_right"]["bones"] = [str(skm.get_bone_name(i)) for i in range(skm.get_num_bones())] if hasattr(skm, "get_num_bones") else "n/a"
        except Exception as ex: res["SKM_right"]["bones"] = "ERR " + str(ex)
except Exception:
    res["EXC"] = traceback.format_exc()
with open(os.path.join(OUT, "survey2.json"), "w", encoding="utf-8") as f: json.dump(res, f, indent=1, ensure_ascii=False, default=str)
unreal.log("[VRSurvey2] done")
