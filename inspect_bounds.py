import unreal
import json

OUT = r"C:\projects\VR\HandVR\.omc\asset_bounds.json"
result = {}

paths = unreal.EditorAssetLibrary.list_assets("/Game/Imported", recursive=True, include_folder=False)
for p in paths:
    asset = unreal.EditorAssetLibrary.load_asset(p)
    if not asset:
        continue
    if not isinstance(asset, unreal.StaticMesh):
        continue
    try:
        b = asset.get_bounds()
        o = b.origin
        e = b.box_extent
        num_lods = asset.get_num_lods()
        num_sections = asset.get_num_sections(0) if num_lods > 0 else 0
        mats = asset.get_editor_property("static_materials")
        mat_names = []
        for m in mats:
            mi = m.material_interface
            mat_names.append(mi.get_name() if mi else "None")
        result[p] = {
            "origin_cm": [round(o.x, 2), round(o.y, 2), round(o.z, 2)],
            "half_extent_cm": [round(e.x, 2), round(e.y, 2), round(e.z, 2)],
            "full_size_cm": [round(e.x * 2, 2), round(e.y * 2, 2), round(e.z * 2, 2)],
            "sphere_radius_cm": round(b.sphere_radius, 2),
            "num_lods": num_lods,
            "num_sections_lod0": num_sections,
            "materials": mat_names,
        }
        unreal.log("BOUNDS %s full=%s" % (p, result[p]["full_size_cm"]))
    except Exception as e:
        result[p] = {"error": repr(e)}
        unreal.log_warning("BOUNDS_ERR %s : %r" % (p, e))

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, indent=2, ensure_ascii=False)

unreal.log("ASSET_BOUNDS_DONE count=%d -> %s" % (len(result), OUT))
