import unreal
import traceback

target = "/Game/Maps/Main"
lines = []

def rec(msg):
    lines.append(str(msg))
    unreal.log("MAPGEN: " + str(msg))

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    rec("LevelEditorSubsystem=" + repr(les))
    ok = les.new_level(target)
    rec("new_level_returned=" + repr(ok))

    eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    saved = eas.save_asset(target, only_if_is_dirty=False)
    rec("save_asset=" + repr(saved))

    exists = unreal.EditorAssetLibrary.does_asset_exist(target)
    rec("asset_exists=" + repr(exists))
except Exception as e:
    rec("EXCEPTION: " + repr(e))
    rec(traceback.format_exc())

with open(r"C:\projects\VR\HandVR\mapgen_result.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
