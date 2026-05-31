import os
import unreal


PROJECT_ROOT = r"C:\projects\VR\HandVR"

ASSETS = [
    (r"assets\vr-new-study-room\source\Brand new Study Room.fbx", "/Game/Imported/Environment", "SM_StudyRoom"),
    (r"assets\elepheant_dining_table.fbx", "/Game/Imported/Furniture", "SM_DiningTable"),
    (r"assets\cc0-paint-brush-3\source\PaintBrush3.fbx", "/Game/Imported/Props", "SM_PaintBrush"),
    (r"assets\claw-hammer-low-poly\source\Claw Hammer\Claw hammer.fbx", "/Game/Imported/Props", "SM_ClawHammer"),
    (r"assets\generic-leather-glove-hand\source\GenericGauntlet_Low.obj", "/Game/Imported/Props", "SM_GauntletHand"),
]


def fbx_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = True
    options.automated_import_should_detect_type = False

    mesh_data = unreal.FbxStaticMeshImportData()
    mesh_data.combine_meshes = True
    mesh_data.generate_lightmap_u_vs = True
    mesh_data.auto_generate_collision = False
    options.static_mesh_import_data = mesh_data
    return options


def import_asset(relative_path, destination, name):
    if unreal.EditorAssetLibrary.does_asset_exist(f"{destination}/{name}"):
        unreal.log(f"[HandVR Import] Already exists, skipping: {destination}/{name}")
        return False

    source = os.path.join(PROJECT_ROOT, relative_path)
    if not os.path.exists(source):
        unreal.log_warning(f"[HandVR Import] Missing source: {source}")
        return False

    unreal.EditorAssetLibrary.make_directory(destination)

    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = destination
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True

    if source.lower().endswith(".fbx"):
        task.options = fbx_options()

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.imported_object_paths)
    if imported:
        unreal.log(f"[HandVR Import] Imported {source} -> {imported}")
        return True

    unreal.log_warning(f"[HandVR Import] Import produced no assets: {source}")
    return False


count = 0
for item in ASSETS:
    if import_asset(*item):
        count += 1

unreal.EditorAssetLibrary.save_directory("/Game/Imported", only_if_is_dirty=False, recursive=True)
unreal.log(f"[HandVR Import] Completed this pass: {count} source asset imported")
