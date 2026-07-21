import unreal


MAPS = [
    "/Game/Maps/L_ShowdownMain",
    "/Game/Maps/Favela",
    "/Game/Maps/L_Hub",
    "/Game/Maps/L_Hub_FavelaGameplay",
    "/Game/Maps/L_MultiplayerLobby",
    "/Game/Maps/L_MultiplayerGame",
]


def label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def prop(obj, name):
    try:
        value = obj.get_editor_property(name)
        if hasattr(value, "get_name"):
            return value.get_name()
        if hasattr(value, "name"):
            return value.name
        return str(value)
    except Exception:
        return "-"


def is_interesting(actor):
    class_name = actor.get_class().get_name()
    actor_label = label(actor)
    tags = ",".join(str(tag) for tag in actor.tags)
    interesting_terms = [
        "ShowDown",
        "HubFlow",
        "Flow",
        "Lobby",
        "Table",
        "Card",
        "Anchor",
        "PlayerStart",
        "Pawn",
        "Camera",
        "BP_",
        "Multiplayer",
        "SelfShot",
    ]
    haystack = " ".join([class_name, actor_label, tags])
    return any(term in haystack for term in interesting_terms)


for map_path in MAPS:
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.log_warning("MAPMERGE missing " + map_path)
        continue

    unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings() if world else None
    game_mode = prop(world_settings, "default_game_mode") if world_settings else "-"
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    unreal.log_warning("MAPMERGE map={0} actors={1} game_mode={2}".format(map_path, len(actors), game_mode))
    for actor in actors:
        if not is_interesting(actor):
            continue
        loc = actor.get_actor_location()
        rot = actor.get_actor_rotation()
        unreal.log_warning(
            "MAPMERGE actor map={0} name={1} label={2} class={3} loc=({4:.1f},{5:.1f},{6:.1f}) rot=({7:.1f},{8:.1f},{9:.1f}) tags={10}".format(
                map_path,
                actor.get_name(),
                label(actor),
                actor.get_class().get_name(),
                loc.x,
                loc.y,
                loc.z,
                rot.pitch,
                rot.yaw,
                rot.roll,
                ",".join(str(tag) for tag in actor.tags),
            )
        )

unreal.log_warning("MAPMERGE complete")
