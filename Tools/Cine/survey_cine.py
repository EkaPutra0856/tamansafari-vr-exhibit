# Cine step 2: survey L_Zoo_Cine -> Saved/Cine/survey_cine.json (actors, exhibits, animals, lights, PathGuide places)
# and Saved/Cine/ground.json (ground height grid, 200 cm). Run with the editor CLOSED: Tools\Cine\run_py.cmd <this file>
import unreal, os, json, traceback

MAP = "/Game/Zoo/Maps/L_Zoo_Cine"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Cine"); os.makedirs(OUT, exist_ok=True)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
res = {"actors": [], "exhibits": {}, "animals": {}, "lights": {}, "places": [], "notes": []}

def v(x): return [round(x.x, 1), round(x.y, 1), round(x.z, 1)]
def r(x): return [round(x.roll, 1), round(x.pitch, 1), round(x.yaw, 1)]
def gp(o, k, d=None):
    try: return o.get_editor_property(k)
    except Exception: return d

try:
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for a in eas.get_all_level_actors():
        lab = a.get_actor_label(); cn = a.get_class().get_name()
        try: org, ext = a.get_actor_bounds(False)
        except Exception: org, ext = a.get_actor_location(), unreal.Vector(0, 0, 0)
        res["actors"].append({"l": lab, "c": cn, "p": v(a.get_actor_location()), "r": r(a.get_actor_rotation()), "bo": v(org), "be": v(ext),
                              "f": str(a.get_folder_path()), "tags": [str(t) for t in a.tags]})
        if isinstance(a, unreal.ZooARExhibit):
            e = {}
            for k in ("activation_mode", "walk_to_deck_center_before_approach", "deck_center_offset", "animal_arrival_timeout", "enable_arrival_camera_focus",
                      "panel_offset", "panel_world_scale", "entrance_start_location", "entrance_target_location", "entrance_move_speed", "max_approach_distance"):
                val = gp(a, k); e[k] = v(val) if isinstance(val, unreal.Vector) else str(val)
            vz = gp(a, "viewing_zone"); e["zone"] = vz.get_actor_label() if vz else None
            if vz:
                box = vz.get_component_by_class(unreal.BoxComponent)
                e["zone_loc"] = v(box.get_world_location()); e["zone_ext"] = v(box.get_scaled_box_extent())
            hb = gp(a, "habitat_bounds"); e["habitat"] = [v(hb.min), v(hb.max)] if hb else None
            m = a.get_editor_property("animal_mesh")
            e["mesh_loc"] = v(m.get_world_location()); e["mesh_rot"] = r(m.get_world_rotation()); e["mesh_scale"] = v(m.get_world_scale())
            bo = m.get_editor_property("bounds") if False else None
            try:
                b = m.bounds; e["mesh_bounds"] = [v(b.origin), v(b.box_extent)]
            except Exception as ex: e["mesh_bounds"] = str(ex)
            p = a.get_editor_property("information_panel"); e["panel_loc"] = v(p.get_world_location())
            res["exhibits"][lab] = e
        if isinstance(a, unreal.ZooAnimal):
            res["animals"][lab] = {"c": cn, "p": v(a.get_actor_location()), "yaw": round(a.get_actor_rotation().yaw, 1),
                                   "walk": gp(a, "walk_speed"), "wander": gp(a, "wander_radius"), "wmin": gp(a, "wander_wait_min"), "wmax": gp(a, "wander_wait_max"),
                                   "path": a.get_path_name()}
        if cn in ("DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog", "PostProcessVolume", "VolumetricCloud") or "Sun" in lab or "Sky" in lab:
            d = {"c": cn, "r": r(a.get_actor_rotation()), "path": a.get_path_name()}
            lc = a.get_component_by_class(unreal.DirectionalLightComponent)
            if lc:
                for k in ("intensity", "temperature", "use_temperature", "light_color", "atmosphere_sun_light"): d[k] = str(gp(lc, k))
            res["lights"][lab] = d
        if cn == "LevelSequenceActor":
            ps = a.get_editor_property("playback_settings"); res["notes"].append("LSA %s auto_play %s seq %s" % (lab, ps.get_editor_property("auto_play"), a.get_editor_property("level_sequence_asset")))
    # PathGuide places (tags)
    try:
        pl = unreal.load_asset("/Game/Zoo/PathGuide/Data/DA_PG_Places")
        for k in pl.get_editor_property("places") if gp(pl, "places") is not None else []:
            res["places"].append(str(k))
    except Exception: res["notes"].append("places " + traceback.format_exc()[-300:])
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith("PG_Destination"):
            d = {}
            for k in ("place_tag", "place", "tag", "arrival_radius"):
                val = gp(a, k)
                if val is not None: d[k] = str(val)
            res["places"].append({a.get_actor_label(): d, "p": v(a.get_actor_location())})
    res["duma_class"] = str(unreal.load_asset("/Game/Zoo/Blueprints/Duma/BP_Duma").generated_class())
    json.dump(res, open(os.path.join(OUT, "survey_cine.json"), "w"), indent=0, default=str)
    unreal.log("[CineSurvey] actors %d" % len(res["actors"]))

    # ground grid
    def ground(x, y):
        ignore = []
        for _ in range(10):
            hr = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 8000), unreal.Vector(x, y, -4000),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, unreal.DrawDebugTrace.NONE, True)
            if hr is None: return None, None
            t = hr.to_tuple()
            if not t[0]: return None, None
            act = t[9]
            if act is None: return t[5].z, "?"
            cn = act.get_class().get_name()
            if cn in ("Landscape", "LandscapeStreamingProxy") or "Water" in act.get_actor_label(): return t[5].z, act.get_actor_label()
            return t[5].z, act.get_actor_label()   # first hit, whatever it is (top surface)
        return None, None
    X0, X1, Y0, Y1, S = -12000, 8000, -9000, 8000, 200
    grid = {"x0": X0, "y0": Y0, "s": S, "nx": (X1 - X0) // S + 1, "ny": (Y1 - Y0) // S + 1, "z": [], "hit": {}}
    for j in range(grid["ny"]):
        row = []
        for i in range(grid["nx"]):
            z, lab = ground(X0 + i * S, Y0 + j * S)
            row.append(None if z is None else round(z, 1))
            if lab: grid["hit"][lab] = grid["hit"].get(lab, 0) + 1
        grid["z"].append(row)
    json.dump(grid, open(os.path.join(OUT, "ground.json"), "w"))
    unreal.log("[CineSurvey] DONE")
except Exception:
    unreal.log("[CineSurvey] FATAL " + traceback.format_exc())
    open(os.path.join(OUT, "survey_err.txt"), "w").write(traceback.format_exc())
