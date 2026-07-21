import unreal


SOURCE_MAP = "/Game/Maps/Favela"
MAIN_MAP = "/Game/Maps/L_ShowdownMain"
MAIN_MAP_NAME = "L_ShowdownMain"
HUB_FLOW_BLUEPRINTS = [
    "/Game/BluePrints/Game/BP_ShowDownHubFlowManager",
]


def log(message):
    unreal.log_warning("MAINLEVEL " + message)


def set_level_names(obj):
    for prop_name in ["multiplayer_lobby_level_name", "multiplayer_level_name"]:
        try:
            obj.set_editor_property(prop_name, MAIN_MAP_NAME)
            log("{0}.{1}={2}".format(obj.get_name(), prop_name, MAIN_MAP_NAME))
        except Exception as exc:
            log("{0}.{1} skipped: {2}".format(obj.get_name(), prop_name, exc))


if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
    raise RuntimeError("Missing source map " + SOURCE_MAP)

did_duplicate = False
if unreal.EditorAssetLibrary.does_asset_exist(MAIN_MAP):
    log("Using existing " + MAIN_MAP)
else:
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, MAIN_MAP)
    if not duplicated:
        raise RuntimeError("Failed to duplicate {0} to {1}".format(SOURCE_MAP, MAIN_MAP))
    unreal.EditorAssetLibrary.save_loaded_asset(duplicated)
    did_duplicate = True
    log("Duplicated {0} to {1}".format(SOURCE_MAP, MAIN_MAP))

for bp_path in HUB_FLOW_BLUEPRINTS:
    bp = unreal.EditorAssetLibrary.load_asset(bp_path)
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    if not bp or not bp_class:
        log("Missing hub flow blueprint " + bp_path)
        continue
    cdo = unreal.get_default_object(bp_class)
    set_level_names(cdo)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)

current_world = unreal.EditorLevelLibrary.get_editor_world()
current_map_name = current_world.get_name() if current_world else ""
if current_map_name != MAIN_MAP_NAME:
    if did_duplicate:
        log("Skipping immediate map load after duplicate to avoid editor world package reuse.")
        log("Run this script once more to configure placed actors.")
        raise SystemExit(0)
    unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MAP)

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if "HubFlowManager" in actor.get_class().get_name() or "HubFlowManager" in actor.get_name():
        set_level_names(actor)
        log("Configured hub flow actor " + actor.get_name())

unreal.EditorLoadingAndSavingUtils.save_current_level()
log("complete")
