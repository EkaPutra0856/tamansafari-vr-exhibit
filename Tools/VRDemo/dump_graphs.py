# Dumps Blueprint graph nodes (events, function calls) of the media extend flow -> log
import unreal
def prop(o, n, d=None):
    try: return o.get_editor_property(n)
    except Exception: return d
def describe(node):
    cls = node.get_class().get_name()
    extra = ""
    fr = prop(node, "function_reference")
    if fr is not None:
        extra = str(prop(fr, "member_name"))
        parent = prop(fr, "member_parent")
        if parent: extra += " @" + parent.get_name()
    ev = prop(node, "event_reference")
    if ev is not None: extra = "event " + str(prop(ev, "member_name"))
    cen = prop(node, "custom_function_name")
    if cen is not None and str(cen) not in ("None", ""): extra += " custom " + str(cen)
    dn = prop(node, "delegate_reference")
    if dn is not None: extra += " delegate " + str(prop(dn, "member_name"))
    vr = prop(node, "variable_reference")
    if vr is not None: extra += " var " + str(prop(vr, "member_name"))
    tl = prop(node, "node_comment")
    return "%s %s" % (cls, extra)
for path in ("/Game/Zoo/Blueprints/BP_Zoo_HUD", "/Game/Zoo/Blueprints/Widgets/WBP_AR_MediaExtend", "/Game/Zoo/Blueprints/Widgets/WBP_AR_Media"):
    bp = unreal.load_asset(path)
    if not bp:
        unreal.log("[Graph] missing " + path); continue
    for gname in ("ubergraph_pages", "function_graphs", "delegate_signature_graphs"):
        graphs = prop(bp, gname, []) or []
        for g in graphs:
            nodes = prop(g, "nodes", []) or []
            unreal.log("[Graph] %s :: %s (%s) %d nodes" % (path.split("/")[-1], g.get_name(), gname, len(nodes)))
            for n in nodes:
                d = describe(n)
                if "Knot" in d or "Comment" in d: continue
                unreal.log("[Graph]    " + d)
unreal.log("[Graph] done")
