# Cine build (UE commandlet, editor CLOSED): Tools\Cine\run_py.cmd build_cine.py
# Reads Saved/Cine/plan_cinematic.json + plan_fpv.json (python cine_plan.py) and writes:
#   L_Zoo_Cine level edits only: CINE_Visitor and CINE_Panels markers, exhibit timing for the render, BP_Giraffe_2 stands still (FPV orbit target)
#   /Game/Cine/LS_Zoo_Cine_Cinematic, /Game/Cine/LS_Zoo_Cine_FPV (cameras, Duma spawnables, visitor marker, golden-hour sun, fades)
#   /Game/Cine/MRQ/MRQ_Cine_Preview_720p, MRQ_Cine_Final_1080p, MRQ_Cine_Final_1080p_FPV (GameMode override ZooCinematicGameMode)
#   Saved/Cine/Audio/*.wav (CityPark ambience for the ffmpeg mix)
# L_Zoo, L_ZooEka, L_ZooEko and every Blueprint asset stay untouched.
import unreal, os, json, math, traceback

MAP = "/Game/Zoo/Maps/L_Zoo_Cine"
FOLDER = "/Game/Cine"
MRQ_FOLDER = "/Game/Cine/MRQ"
SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Cine")
RENDER_DIR = os.path.join(SAVED, "Render").replace("\\", "/")
DUMA_CLASS = "/Game/Zoo/Blueprints/Duma/BP_Duma.BP_Duma_C"
VISITOR_LABEL = "CINE_Visitor"
PANELS_LABEL = "CINE_Panels"
PANEL_EXHIBITS = ["AR_Giraffe_Deck", "AR_Wombat_Overlook"]
GUIDES = ["PathGuide.Place.Giraffe.Deck", "PathGuide.Place.Wombat.Burrow"]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
LOG = []

def L(*a):
    s = " ".join(str(x) for x in a); LOG.append(s); unreal.log("[CineBuild] " + s)

# ------------------------------------------------------------------ keys
LIN = unreal.MovieSceneKeyInterpolation.LINEAR
CONST = unreal.MovieSceneKeyInterpolation.CONSTANT
AUTO = unreal.MovieSceneKeyInterpolation.AUTO

def key(ch, f, v, interp=LIN):
    ch.add_key(unreal.FrameNumber(int(f)), float(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE, interp)

def transform_channels(binding, f0, f1):
    tr = binding.add_track(unreal.MovieScene3DTransformTrack); sec = tr.add_section(); sec.set_range(f0, f1)
    return list(sec.get_all_channels())   # loc xyz, rot roll pitch yaw, scale xyz

def binding_id(seq, b):
    last = None
    for fn in (lambda: unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, b),
               lambda: seq.get_portable_binding_id(seq, b), lambda: seq.get_binding_id(b), lambda: b.get_binding_id()):
        try: return fn()
        except Exception as e: last = e
    raise last

def spawn_range(b, f0, f1):
    for t in b.get_tracks():
        if isinstance(t, unreal.MovieSceneSpawnTrack):
            for s in t.get_sections(): s.set_range(f0, f1)

def component_float_track(seq, parent, comp, prop, path, f0, f1):
    cb = seq.add_possessable(comp); cb.set_parent(parent)
    ft = cb.add_track(unreal.MovieSceneFloatTrack); ft.set_property_name_and_path(prop, path)
    sec = ft.add_section(); sec.set_range(f0, f1)
    return list(sec.get_all_channels())[0]

# ------------------------------------------------------------------ level (L_Zoo_Cine only)
def prepare_level(actors):
    changed = []
    marker = actors.get(VISITOR_LABEL)
    if marker is None:
        marker = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(-5320, -60, 100), unreal.Rotator(0, 0, 0))
        marker.set_actor_label(VISITOR_LABEL); marker.set_folder_path("Cine"); changed.append("CINE_Visitor created")
    marker.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    marker.set_actor_hidden_in_game(True); marker.set_actor_enable_collision(False)
    marker.set_editor_property("tags", [unreal.Name("CineVisitor")] + [unreal.Name("CineGuide=" + g) for g in GUIDES])
    # AR card demonstration marker: the sequence keys its location (X exhibit, Y card, Z scroll %), ZooCinematicGameMode clicks the cards
    panels = actors.get(PANELS_LABEL)
    if panels is None:
        panels = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        panels.set_actor_label(PANELS_LABEL); panels.set_folder_path("Cine"); changed.append("CINE_Panels created")
    panels.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    panels.set_actor_hidden_in_game(True); panels.set_actor_enable_collision(False)
    panels.set_editor_property("tags", [unreal.Name("CinePanels")] + [unreal.Name("CinePanelExhibit=" + e) for e in PANEL_EXHIBITS])
    for lab in ("AR_Giraffe_Deck", "AR_Wombat_Overlook"):
        ex = actors[lab]
        ex.set_editor_property("walk_to_deck_center_before_approach", False)   # the render visitor is a marker, not a walking character
        ex.set_editor_property("enable_arrival_camera_focus", False)           # the film camera frames the reveal itself
        ex.set_editor_property("animal_arrival_timeout", 0.0)                   # materialize as soon as the visitor is in the zone
        changed.append(lab + " render timing")
    actors["BP_Giraffe_2"].set_editor_property("wander_radius", 0.0); changed.append("BP_Giraffe_2 wander 0")
    g3 = actors["BP_Giraffe_3"]; g3.set_editor_property("wander_wait_min", 0.3); g3.set_editor_property("wander_wait_max", 0.8)   # keeps walking for the tracking shot
    changed.append("BP_Giraffe_3 short waits")
    # keep the wandering giraffes clear of the FPV orbit round BP_Giraffe_2 (and of the drone's hologram pass)
    for lab, radius in (("BP_Giraffe", 250.0), ("BP_Giraffe_3", 300.0), ("BP_Giraffe_4", 200.0)):
        actors[lab].set_editor_property("wander_radius", radius); changed.append("%s wander %d" % (lab, radius))
    les.save_current_level()
    L("level edits:", ", ".join(changed))
    return marker, panels

# ------------------------------------------------------------------ sequence pieces
def new_sequence(name, fps, frames):
    path = FOLDER + "/" + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    seq = at.create_asset(name, FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(fps, 1)); seq.set_tick_resolution(unreal.FrameRate(24000, 1))
    seq.set_playback_start(0); seq.set_playback_end(frames)
    seq.set_view_range_start(-1.0); seq.set_view_range_end(frames / fps + 1.0); seq.set_work_range_start(0); seq.set_work_range_end(frames / fps)
    return seq

def make_camera(seq, name, cam, f0, f1, actors, fpv=False):
    k0 = cam["keys"][0]
    inst = eas.spawn_actor_from_class(unreal.CineCameraActor, unreal.Vector(k0[0], k0[1], k0[2]), unreal.Rotator(roll=k0[3], pitch=k0[4], yaw=k0[5]))
    inst.set_actor_label(name)
    cc = inst.get_cine_camera_component()
    cc.set_editor_property("current_focal_length", float(cam["focal"]))
    cc.set_editor_property("current_aperture", float(cam["fstop"]))
    fs = cc.get_editor_property("focus_settings")
    track = cam.get("track")
    if fpv:
        fs.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)    # FPV: everything sharp
    elif track:
        tfs = fs.get_editor_property("tracking_focus_settings")
        tfs.set_editor_property("actor_to_track", actors[track["actor"]]); tfs.set_editor_property("relative_offset", unreal.Vector(*track["offset"]))
        fs.set_editor_property("tracking_focus_settings", tfs); fs.set_editor_property("focus_method", unreal.CameraFocusMethod.TRACKING)
    else:
        fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
        fs.set_editor_property("manual_focus_distance", float(cam["keys"][len(cam["keys"]) // 2][6]))
    fs.set_editor_property("smooth_focus_changes", True); fs.set_editor_property("focus_smoothing_interp_speed", 4.0)
    cc.set_editor_property("focus_settings", fs)
    pp = cc.get_editor_property("post_process_settings")
    pp.set_editor_property("override_motion_blur_amount", True); pp.set_editor_property("motion_blur_amount", 0.3 if fpv else 0.5)
    pp.set_editor_property("override_motion_blur_max", True); pp.set_editor_property("motion_blur_max", 4.0 if fpv else 5.0)
    pp.set_editor_property("override_scene_color_tint", True); pp.set_editor_property("scene_color_tint", unreal.LinearColor(1.0, 0.97, 0.92, 1.0))   # golden-hour warmth
    pp.set_editor_property("override_auto_exposure_bias", True); pp.set_editor_property("auto_exposure_bias", -0.6)
    pp.set_editor_property("override_vignette_intensity", True); pp.set_editor_property("vignette_intensity", 0.35)
    cc.set_editor_property("post_process_settings", pp); cc.set_editor_property("post_process_blend_weight", 1.0)
    if track:   # live animal: aim follows the actor (AI walks, its path is not known before the render)
        lt = inst.get_editor_property("lookat_tracking_settings")
        lt.set_editor_property("enable_look_at_tracking", True); lt.set_editor_property("actor_to_track", actors[track["actor"]])
        lt.set_editor_property("relative_offset", unreal.Vector(*track["offset"])); lt.set_editor_property("look_at_tracking_interp_speed", 3.0)
        inst.set_editor_property("lookat_tracking_settings", lt)
    b = seq.add_spawnable_from_instance(inst); b.set_display_name(name)
    eas.destroy_actor(inst)
    spawn_range(b, f0, f1)
    ch = transform_channels(b, f0, f1)
    for i, k in enumerate(cam["keys"]):
        f = f0 + i
        for c in range(6): key(ch[c], f, k[c])
    # focus distance follows the planned subject distance (manual focus shots)
    if not fpv and not track:
        try:
            tpl = b.get_object_template(); tcc = tpl.get_cine_camera_component()
            fch = component_float_track(seq, b, tcc, "ManualFocusDistance", "FocusSettings.ManualFocusDistance", f0, f1)
            for i in range(0, len(cam["keys"]), 5): key(fch, f0 + i, cam["keys"][i][6], AUTO)
        except Exception as e: L("  focus track fail", name, e)
    return b

def make_duma(seq, d, fps):
    cls = unreal.load_class(None, DUMA_CLASS)
    k0 = d["keys"][0]
    inst = eas.spawn_actor_from_class(cls, unreal.Vector(k0[0], k0[1], k0[2]), unreal.Rotator(roll=0.0, pitch=0.0, yaw=k0[4]))
    inst.set_actor_label(d["name"])
    inst.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.DISABLED)   # flown by the sequence; ZooCinematicGameMode makes him appear
    b = seq.add_spawnable_from_instance(inst); b.set_display_name(d["name"])
    eas.destroy_actor(inst)
    spawn_range(b, d["f0"], d["f1"])
    ch = transform_channels(b, d["f0"], d["f1"])
    for i, k in enumerate(d["keys"]):
        f = d["f0"] + i
        key(ch[0], f, k[0]); key(ch[1], f, k[1]); key(ch[2], f, k[2]); key(ch[3], f, 0.0); key(ch[4], f, k[3]); key(ch[5], f, k[4])
    L("  Duma %s frames %d-%d" % (d["name"], d["f0"], d["f1"]))

def visitor_track(seq, marker, visitor, fps, frames):
    b = seq.add_possessable(marker)
    ch = transform_channels(b, 0, frames)
    for t, x, y, z in visitor:
        f = int(round(t * fps))
        for c, v in zip(range(6), (x, y, z, 0.0, 0.0, 0.0)): key(ch[c], f, v, CONST)
    L("  visitor marker keys", [(round(t, 2), x, y) for t, x, y, z in visitor])

def panels_track(seq, panels, plan, fps, frames):
    b = seq.add_possessable(panels)
    ch = transform_channels(b, 0, frames)
    for t, ex, card in plan["steps"]:
        f = int(round(t * fps)); key(ch[0], f, ex, CONST); key(ch[1], f, card, CONST)
    for t, z in plan["scroll"]: key(ch[2], int(round(t * fps)), z)
    for c in range(3, 6): key(ch[c], 0, 0.0, CONST)
    L("  card steps", [(round(t, 1), ex, card) for t, ex, card in plan["steps"]])

def golden_hour(seq, actors, frames):
    sun = actors["SUN_Directional"]
    sb = seq.add_possessable(sun)
    ch = transform_channels(sb, 0, frames)
    loc = sun.get_actor_location()
    for c, v in zip(range(6), (loc.x, loc.y, loc.z, 0.0, -30.0, 45.0)): key(ch[c], 0, v, CONST)   # low sun from the south-west, matches SKY_SunDisc
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    for prop, v in (("Temperature", 4100.0), ("Intensity", 7.0)):
        c = component_float_track(seq, sb, lc, prop, prop, 0, frames); key(c, 0, v, CONST)
    L("  golden hour: sun pitch -30 yaw 45, 4100 K, intensity 7 (sequence only)")

def fades(seq, frames, fps, fade_in, fade_out):
    ft = seq.add_track(unreal.MovieSceneFadeTrack); sec = ft.add_section(); sec.set_range(0, frames); c = list(sec.get_all_channels())[0]
    key(c, 0, 1.0); key(c, int(fade_in * fps), 0.0); key(c, frames - int(fade_out * fps), 0.0); key(c, frames, 1.0)

def build(plan_file, name, fpv, actors, marker, panels=None):
    plan = json.load(open(os.path.join(SAVED, plan_file)))
    fps, frames = plan["fps"], plan["frames"]
    seq = new_sequence(name, fps, frames)
    cut = seq.add_track(unreal.MovieSceneCameraCutTrack)
    if fpv:
        cam = {"keys": plan["keys"], "focal": 15.0, "fstop": 8.0}
        b = make_camera(seq, "CAM_FPV", cam, 0, frames, actors, fpv=True)
        cs = cut.add_section(); cs.set_range(0, frames); cs.set_camera_binding_id(binding_id(seq, b))
    else:
        for cam in plan["cameras"]:
            b = make_camera(seq, "CAM_" + cam["name"], cam, cam["f0"], cam["f1"], actors)
            cs = cut.add_section(); cs.set_range(cam["f0"], cam["f1"]); cs.set_camera_binding_id(binding_id(seq, b))
            L("  shot %-24s %4d-%4d %dmm f/%.1f %s" % (cam["name"], cam["f0"], cam["f1"], cam["focal"], cam["fstop"], cam["track"]["actor"] if cam.get("track") else ""))
    for d in plan["duma"]: make_duma(seq, d, fps)
    visitor_track(seq, marker, plan["visitor"], fps, frames)
    if panels and plan.get("panels"): panels_track(seq, panels, plan["panels"], fps, frames)
    golden_hour(seq, actors, frames)
    fades(seq, frames, fps, 1.0 if not fpv else 0.4, 2.5 if not fpv else 1.2)
    eal.save_loaded_asset(seq)
    L("saved %s/%s: %d frames @%d (%.1f s)" % (FOLDER, name, frames, fps, frames / fps))
    return seq

# ------------------------------------------------------------------ Movie Render Queue presets
FINAL_CVARS = {
    # Lumen with hardware ray tracing (RayTracing is already on in Config/DefaultEngine.ini); render-only, VR settings untouched
    "r.DynamicGlobalIlluminationMethod": 1, "r.ReflectionMethod": 1, "r.Lumen.HardwareRayTracing": 1, "r.Lumen.HardwareRayTracing.LightingMode": 2,
    "r.Lumen.Reflections.HardwareRayTracing": 1, "r.Lumen.ScreenProbeGather.DownsampleFactor": 8, "r.Lumen.ScreenProbeGather.TracingOctahedronResolution": 16,
    "r.Lumen.Reflections.MaxRoughnessToTrace": 0.6, "r.Lumen.ScreenProbeGather.MaxRayIntensity": 40,
    "r.Shadow.Virtual.Enable": 1, "r.Shadow.Virtual.ResolutionLodBiasDirectional": 0.0, "r.Shadow.Virtual.SMRT.RayCountDirectional": 8,
    "r.Shadow.Virtual.SMRT.SamplesPerRayDirectional": 4,
    # RTX 3070 8 GB: bounded texture pool (an earlier 1080p run crashed with FullyLoad + pool 4500)
    "r.Streaming.PoolSize": 2500, "r.Streaming.LimitPoolSizeToVRAM": 1, "r.TSR.History.ScreenPercentage": 100,
    "r.VolumetricFog.GridPixelSize": 4, "r.DepthOfFieldQuality": 4, "r.MotionBlurQuality": 4, "r.MotionBlur.Max": 100,
    "r.ViewDistanceScale": 1.5, "foliage.DensityScale": 1.0, "grass.DensityScale": 1.0, "r.SkeletalMeshLODBias": -2, "r.Water.SingleLayer.Reflection": 2,
}
PREVIEW_CVARS = {"r.Streaming.PoolSize": 2000, "r.Streaming.LimitPoolSizeToVRAM": 1, "r.MotionBlurQuality": 0, "r.ViewDistanceScale": 1.5}

def make_preset(name, res, temporal, warmup, step, cvars, tag, frames=None):
    path = MRQ_FOLDER + "/" + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    cfg = at.create_asset(name, MRQ_FOLDER, unreal.MoviePipelinePrimaryConfig, unreal.MoviePipelinePrimaryConfigFactory())
    out = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    out.set_editor_property("output_directory", unreal.DirectoryPath(RENDER_DIR + "/" + tag))
    out.set_editor_property("file_name_format", "{sequence_name}/{sequence_name}.{frame_number}")
    out.set_editor_property("output_resolution", unreal.IntPoint(res[0], res[1]))
    out.set_editor_property("use_custom_frame_rate", False)          # sequence display rate: Cinematic 30, FPV 60
    out.set_editor_property("output_frame_step", step)
    out.set_editor_property("zero_pad_frame_numbers", 4)
    out.set_editor_property("override_existing_output", True)
    if frames:   # re-render of a frame range into the same frame folder
        out.set_editor_property("use_custom_playback_range", True)
        out.set_editor_property("custom_start_frame", frames[0]); out.set_editor_property("custom_end_frame", frames[1])
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
    aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    aa.set_editor_property("spatial_sample_count", 1); aa.set_editor_property("temporal_sample_count", temporal)
    aa.set_editor_property("override_anti_aliasing", True); aa.set_editor_property("anti_aliasing_method", unreal.AntiAliasingMethod.AAM_TSR)
    aa.set_editor_property("render_warm_up_count", warmup); aa.set_editor_property("engine_warm_up_count", warmup)
    aa.set_editor_property("use_camera_cut_for_warm_up", False); aa.set_editor_property("render_warm_up_frames", True)
    cam = cfg.find_or_add_setting_by_class(unreal.MoviePipelineCameraSetting)
    cam.set_editor_property("shutter_timing", unreal.MoviePipelineShutterTiming.FRAME_CENTER)
    go = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
    go.set_editor_property("game_mode_override", unreal.ZooCinematicGameMode)   # render-only player: no HUD, hands, lasers or narrative
    go.set_editor_property("cinematic_quality_settings", True)
    go.set_editor_property("texture_streaming", unreal.MoviePipelineTextureStreamingMethod.NONE)
    go.set_editor_property("use_lod_zero", True); go.set_editor_property("disable_hlods", True)
    go.set_editor_property("use_high_quality_shadows", True); go.set_editor_property("shadow_distance_scale", 10); go.set_editor_property("shadow_radius_threshold", 0.001)
    go.set_editor_property("override_view_distance_scale", True); go.set_editor_property("view_distance_scale", 50)
    go.set_editor_property("flush_grass_streaming", True)
    cv = cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)
    for k, v in cvars.items(): cv.add_or_update_console_variable(k, float(v))
    cv.set_editor_property("start_console_commands", ["r.HZBOcclusion 0"])
    eal.save_loaded_asset(cfg)
    L("preset %s %s temporal %d warmup %d step %d cvars %d" % (path, res, temporal, warmup, step, len(cvars)))

def main():
    les.load_level(MAP)
    world_name = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()
    if world_name != "L_Zoo_Cine": raise RuntimeError("wrong level loaded: " + world_name)
    actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    marker, panels = prepare_level(actors)
    actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    build("plan_cinematic.json", "LS_Zoo_Cine_Cinematic", False, actors, marker, panels)
    build("plan_fpv.json", "LS_Zoo_Cine_FPV", True, actors, marker)
    les.save_current_level()   # spawn/destroy of the camera templates must not leave the level dirty with stray actors
    make_preset("MRQ_Cine_Preview_720p", (1280, 720), 1, 8, 10, PREVIEW_CVARS, "Preview")
    make_preset("MRQ_Cine_Final_1080p", (1920, 1080), 4, 32, 1, FINAL_CVARS, "Final")
    make_preset("MRQ_Cine_Final_1080p_FPV", (1920, 1080), 3, 32, 1, FINAL_CVARS, "Final")
    # single-shot re-renders of the animal shots (the AI differs per run): Cinematic frames into the Final folder
    for c in json.load(open(os.path.join(SAVED, "plan_cinematic.json")))["cameras"]:
        if c["name"][:2] in ("08", "13"): make_preset("MRQ_Cine_Final_1080p_Shot" + c["name"][:2], (1920, 1080), 4, 32, 1, FINAL_CVARS, "Final", (c["f0"], c["f1"]))
    try:
        adir = os.path.join(SAVED, "Audio"); os.makedirs(adir, exist_ok=True)
        at.export_assets(["/Game/Marketplace/CityPark/Sound/S_231537__vkproduktion__forest-birds-loop-02",
                          "/Game/Marketplace/CityPark/Sound/S_345852__hargissssound__spring-birds-loop-with-low-cut-new-jersey",
                          "/Game/Marketplace/CityPark/Sound/S_469009__innorecords__relaxing-mountains-rivers-streams-running-water"], adir)
        L("audio exported:", [f for r, _, fs in os.walk(adir) for f in fs])
    except Exception: L("audio export fail", traceback.format_exc())
    L("=== build DONE")

try: main()
except Exception: L("!! FATAL", traceback.format_exc())
open(os.path.join(SAVED, "build_cine.log"), "w").write("\n".join(LOG))
