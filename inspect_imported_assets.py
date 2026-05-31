import unreal

ASSETS = [
    "/Game/Imported/Environment/SM_StudyRoom.SM_StudyRoom",
    "/Game/Imported/Furniture/SM_DiningTable.SM_DiningTable",
    "/Game/Imported/Props/SM_PaintBrush.SM_PaintBrush",
    "/Game/Imported/Props/SM_ClawHammer.SM_ClawHammer",
    "/Game/Imported/Props/SM_GauntletHand.SM_GauntletHand",
]

for path in ASSETS:
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log_warning(f"ASSET_INSPECT missing {path}")
        continue

    bounds = asset.get_bounds()
    unreal.log(
        "ASSET_INSPECT "
        f"{path} origin={bounds.origin} box_extent={bounds.box_extent} "
        f"sphere_radius={bounds.sphere_radius}"
    )
