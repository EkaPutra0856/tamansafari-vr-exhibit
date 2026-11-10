# In-editor applier for Saved/ZooBuild/plan_<stage>_index.json (+ its parts).
# Paste into unreal-mcp ProgrammaticToolset.execute_tool_script after setting STAGE (and optionally ONLY).
# Each ISM group becomes one Actor with one static InstancedStaticMeshComponent per mesh; existing actors
# with the same label are replaced, so re-running a stage is safe. Parts flagged "append" add components to
# the actor created by an earlier part. PerInstanceSMData is set twice on purpose: the first (size-changing)
# set resets the newest element to identity.
import json, math
S='editor_toolset.toolsets.scene.SceneTools.'
A='editor_toolset.toolsets.actor.ActorTools.'
O='editor_toolset.toolsets.object.ObjectTools.'
STAGE='veg'
ONLY=None  # e.g. [0, 1] to apply only some part indices
ONLY_LABELS=None  # e.g. ['GIR_DeckBoards'] to apply only these groups/heroes
BASE='D:/UEProjects/Zoo/Saved/ZooBuild/'
# NOTE: after applying, save and reload L_Zoo - the editor render state of re-set ISM data can be stale.

def T(n, a):
    return execute_tool(n, json.dumps(a))

def mtx(v):
    x, y, z, p, yw, r, sx, sy, sz = v
    p, yw, r = math.radians(p), math.radians(yw), math.radians(r)
    SP, CP, SY, CY, SR, CR = math.sin(p), math.cos(p), math.sin(yw), math.cos(yw), math.sin(r), math.cos(r)
    return {'transform': {'xPlane': {'x': CP*CY*sx, 'y': CP*SY*sx, 'z': SP*sx, 'w': 0}, 'yPlane': {'x': (SR*SP*CY-CR*SY)*sy, 'y': (SR*SP*SY+CR*CY)*sy, 'z': -SR*CP*sy, 'w': 0}, 'zPlane': {'x': -(CR*SP*CY+SR*SY)*sz, 'y': (CY*SR-CR*SP*SY)*sz, 'z': CR*CP*sz, 'w': 0}, 'wPlane': {'x': x, 'y': y, 'z': z, 'w': 1}}}

def by_label(label):
    res = []
    for a in T(S+'find_actors', {'name': label, 'tag': '', 'collision_channels': []})['returnValue']:
        if T(A+'get_label', {'actor': a})['returnValue'] == label:
            res.append(a)
    return res

def setp(ref, vals):
    return T(O+'set_properties', {'instance': ref, 'values': json.dumps(vals)})

def read(name):
    return json.loads(T('editor_toolset.toolsets.asset.AssetTools.read_file', {'file_path': BASE + name})['returnValue'])

def apply_group(g, log):
    if g.get('append'):
        acts = by_label(g['label'])
        act = acts[0]
    else:
        for a in by_label(g['label']):
            T(S+'remove_from_scene', {'actor': a})
        act = T(S+'add_to_scene_from_class', {'actor_type': {'refPath': '/Script/Engine.Actor'}, 'name': g['label'], 'xform': {'location': {'x': 0, 'y': 0, 'z': 0}}})['returnValue']
        T(A+'set_label', {'actor': act, 'label': g['label']})
    n = 0
    for i, m in enumerate(g['meshes']):
        idx = i + g.get('mesh_offset', 0)
        short = m['mesh'].split('.')[-1][:40]
        comp = T(A+'add_component', {'owner': act, 'component_type': {'refPath': '/Script/Engine.InstancedStaticMeshComponent'}, 'name': 'ISM_%02d_%s' % (idx, short)})['returnValue']
        props = {'StaticMesh': {'refPath': m['mesh']}, 'Mobility': 'Static'}
        props.update(g.get('comp_props', {}))
        try:
            setp(comp, props)
        except Exception as e:
            log['errors'].append('%s props: %s' % (short, str(e)[:200]))
            setp(comp, {'StaticMesh': {'refPath': m['mesh']}})
        data = [mtx(v) for v in m['inst']]
        setp(comp, {'PerInstanceSMData': data})
        setp(comp, {'PerInstanceSMData': data})
        n += len(data)
    T(S+'set_actor_folder', {'actor': act, 'folder_path': g['folder']})
    log['groups'][g['label']] = log['groups'].get(g['label'], 0) + n

def apply_hero(h, log):
    for a in by_label(h['label']):
        T(S+'remove_from_scene', {'actor': a})
    xf = {'location': {'x': h['loc'][0], 'y': h['loc'][1], 'z': h['loc'][2]}, 'rotation': {'pitch': h['rot'][0], 'yaw': h['rot'][1], 'roll': h['rot'][2]}, 'scale': {'x': h['scale'][0], 'y': h['scale'][1], 'z': h['scale'][2]}}
    if h.get('asset'):
        act = T(S+'add_to_scene_from_asset', {'asset_path': h['asset'].split('.')[0], 'name': h['label'], 'xform': xf})['returnValue']
    else:
        act = T(S+'add_to_scene_from_class', {'actor_type': {'refPath': h['class']}, 'name': h['label'], 'xform': xf})['returnValue']
    T(A+'set_label', {'actor': act, 'label': h['label']})
    if h.get('folder'):
        T(S+'set_actor_folder', {'actor': act, 'folder_path': h['folder']})
    for tg in h.get('tags', []):
        T(A+'add_tag', {'actor': act, 'tag': tg})
    if h.get('mat'):
        setp({'refPath': act['refPath'] + '.StaticMeshComponent0'}, {'OverrideMaterials': [{'refPath': h['mat']}]})
    for cname, vals in h.get('comp_props', {}).items():
        try:
            setp({'refPath': act['refPath'] + '.' + cname}, vals)
        except Exception as e:
            log['errors'].append('%s.%s: %s' % (h['label'], cname, str(e)[:200]))
    log['hero'] += 1

def run():
    idx = read('plan_%s_index.json' % STAGE)
    log = {'groups': {}, 'hero': 0, 'errors': [], 'parts': 0}
    for k, name in enumerate(idx['parts']):
        if ONLY is not None and k not in ONLY:
            continue
        part = read(name)
        for g in part['groups']:
            if ONLY_LABELS is None or g['label'] in ONLY_LABELS:
                apply_group(g, log)
        for h in part['hero']:
            if ONLY_LABELS is None or h['label'] in ONLY_LABELS:
                apply_hero(h, log)
        log['parts'] += 1
    return log
