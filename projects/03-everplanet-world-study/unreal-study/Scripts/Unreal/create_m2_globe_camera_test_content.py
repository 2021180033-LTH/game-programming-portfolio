"""Create or strictly verify the isolated M2 globe-camera comparison content.

Run from Unreal Editor 5.8 with ``-ExecutePythonScript``. The script never
modifies M1 assets, Third Person source assets, project default maps, or an
unexpected existing M2 target. Missing targets are created in dependency order;
existing targets are audited and reused only when they match the M2 contract.
"""

from __future__ import annotations

import math

import unreal


SOURCE_CHARACTER = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
SOURCE_GAME_MODE = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode"
DEFAULT_MAPPING_CONTEXT = "/Game/Input/IMC_Default.IMC_Default"
MOUSE_LOOK_MAPPING_CONTEXT = "/Game/Input/IMC_MouseLook.IMC_MouseLook"

TARGET_ROOT = "/Game/EverPlanet/Tests/M2"
TARGET_CHARACTER = f"{TARGET_ROOT}/Blueprints/BP_M2GlobeCameraCharacter"
TARGET_GAME_MODE = f"{TARGET_ROOT}/Blueprints/BP_M2GlobeCameraGameMode"
TARGET_MAP = f"{TARGET_ROOT}/Maps/Lvl_M2GlobeCamera"

M2_MANUAL_CHARACTER_CLASS = "/Script/EverPlanet.M2GlobeCameraManualTestCharacter"
GLOBE_CAMERA_COMPONENT_CLASS = "/Script/EverPlanet.PlanetGlobeCameraComponent"
PLANET_GRAVITY_COMPONENT_CLASS = "/Script/EverPlanet.PlanetGravityComponent"
PLANET_BODY_CLASS = "/Script/EverPlanet.PlanetBody"
PLAYER_START_CLASS = "/Script/Engine.PlayerStart"
DIRECTIONAL_LIGHT_CLASS = "/Script/Engine.DirectionalLight"
SKY_LIGHT_CLASS = "/Script/Engine.SkyLight"
SKY_ATMOSPHERE_CLASS = "/Script/Engine.SkyAtmosphere"

PLANET_LABEL = "PlanetBody_0"
PLAYER_START_LABEL = "PlayerStart_0"
DIRECTIONAL_LIGHT_LABEL = "DirectionalLight_0"
SKY_LIGHT_LABEL = "SkyLight_0"
SKY_ATMOSPHERE_LABEL = "SkyAtmosphere_0"
EXPECTED_LABELS = {
    PLANET_LABEL,
    PLAYER_START_LABEL,
    DIRECTIONAL_LIGHT_LABEL,
    SKY_LIGHT_LABEL,
    SKY_ATMOSPHERE_LABEL,
}

PLANET_RADIUS_CM = 10000.0
SPAWN_CLEARANCE_CM = 25.0
KILL_Z_CM = -1000000.0
CAMERA_ARM_CM = 1600.0
CAMERA_DOWN_PITCH_DEG = 38.0
CAMERA_HFOV_DEG = 60.0
CAMERA_LOCAL_UP_OFFSET_CM = 100.0
CAMERA_LOOK_AHEAD_CM = 0.0
CAMERA_PROBE_SIZE_CM = 18.0
ATMOSPHERE_BOTTOM_RADIUS_KM = PLANET_RADIUS_CM / 100000.0
ATMOSPHERE_HEIGHT_KM = 10.0


def info(message: str) -> None:
    unreal.log(f"[EverPlanet M2 Content] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet M2 Content] {message}")
        raise RuntimeError(message)


def object_path(obj: unreal.Object | None) -> str:
    return obj.get_path_name() if obj else "None"


def class_path(obj: unreal.Object | None) -> str:
    return obj.get_class().get_path_name() if obj else "None"


def load_class_checked(path: str) -> unreal.Class:
    loaded = unreal.load_class(None, path)
    require(loaded is not None, f"Required class could not be loaded: {path}")
    return loaded


def load_blueprint_class_checked(asset_path: str) -> unreal.Class:
    loaded = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    require(loaded is not None, f"Generated Blueprint class could not be loaded: {asset_path}")
    return loaded


def compile_blueprint_checked(blueprint: unreal.Blueprint, label: str) -> None:
    compiled = unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(bool(compiled), f"Blueprint compilation failed: {label}")
    info(f"Compiled {label}; status={blueprint.get_editor_property('status')}")


def set_actor_label(actor: unreal.Actor, label: str) -> None:
    actor.set_actor_label(label, mark_dirty=True)
    require(actor.get_actor_label() == label, f"Actor label could not be set: {label}")


def spawn_actor(
    actor_subsystem: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    location: unreal.Vector,
    rotation: unreal.Rotator,
    label: str,
) -> unreal.Actor:
    actor = actor_subsystem.spawn_actor_from_class(
        actor_class,
        location,
        rotation,
        False,
    )
    require(actor is not None, f"Failed to spawn actor: {label}")
    set_actor_label(actor, label)
    return actor


def set_component_movable(actor: unreal.Actor, property_name: str) -> unreal.ActorComponent:
    component = actor.get_editor_property(property_name)
    require(component is not None, f"Missing component {property_name} on {actor.get_name()}")
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    return component


def preflight_existing_targets(asset_subsystem: unreal.EditorAssetSubsystem) -> None:
    """Reject inconsistent or unexpected existing M2 targets before mutation."""

    character_exists = asset_subsystem.does_asset_exist(TARGET_CHARACTER)
    game_mode_exists = asset_subsystem.does_asset_exist(TARGET_GAME_MODE)
    map_exists = asset_subsystem.does_asset_exist(TARGET_MAP)
    require(
        not game_mode_exists or character_exists,
        "Existing M2 GameMode has no matching M2 character; refusing partial repair",
    )
    require(
        not map_exists or (character_exists and game_mode_exists),
        "Existing M2 map is missing one or both dependent Blueprints; refusing partial repair",
    )

    if character_exists:
        character_blueprint = asset_subsystem.load_asset(TARGET_CHARACTER)
        require(
            isinstance(character_blueprint, unreal.Blueprint),
            "Existing M2 character target is not a Blueprint",
        )
        actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            character_blueprint
        )
        expected_parent = load_class_checked(M2_MANUAL_CHARACTER_CLASS)
        require(
            actual_parent == expected_parent,
            "Unexpected existing M2 character parent before mutation: "
            f"{object_path(actual_parent)}",
        )

    if game_mode_exists:
        game_mode_blueprint = asset_subsystem.load_asset(TARGET_GAME_MODE)
        source_blueprint = asset_subsystem.load_asset(SOURCE_GAME_MODE)
        require(
            isinstance(game_mode_blueprint, unreal.Blueprint)
            and isinstance(source_blueprint, unreal.Blueprint),
            "Existing or source M2 GameMode target is not a Blueprint",
        )
        actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            game_mode_blueprint
        )
        expected_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            source_blueprint
        )
        require(
            actual_parent == expected_parent,
            "Unexpected existing M2 GameMode parent before mutation: "
            f"actual={object_path(actual_parent)}, expected={object_path(expected_parent)}",
        )

    if map_exists:
        level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(
            level_subsystem is not None
            and actor_subsystem is not None
            and editor_subsystem is not None,
            "Editor subsystems unavailable during existing-map preflight",
        )
        require(
            level_subsystem.load_level(TARGET_MAP),
            "Existing M2 map could not be loaded during preflight",
        )
        actors = actor_subsystem.get_all_level_actors()
        by_label = {actor.get_actor_label(): actor for actor in actors}
        require(
            len(actors) == len(EXPECTED_LABELS)
            and len(by_label) == len(actors)
            and set(by_label) == EXPECTED_LABELS,
            "Unexpected existing M2 map actor set before mutation: "
            f"count={len(actors)}, labels={sorted(by_label)}",
        )
        expected_classes = {
            PLANET_LABEL: PLANET_BODY_CLASS,
            PLAYER_START_LABEL: PLAYER_START_CLASS,
            DIRECTIONAL_LIGHT_LABEL: DIRECTIONAL_LIGHT_CLASS,
            SKY_LIGHT_LABEL: SKY_LIGHT_CLASS,
            SKY_ATMOSPHERE_LABEL: SKY_ATMOSPHERE_CLASS,
        }
        for label, expected_class in expected_classes.items():
            require(
                class_path(by_label[label]) == expected_class,
                "Unexpected existing M2 map actor class before mutation: "
                f"{label}={class_path(by_label[label])}",
            )

        world = editor_subsystem.get_editor_world()
        require(world is not None, "Existing M2 map has no editor world during preflight")
        game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
        require(
            world.get_world_settings().get_editor_property("default_game_mode")
            == game_mode_class,
            "Unexpected existing M2 map GameMode before mutation",
        )

    info(
        "M2_PREFLIGHT="
        f"character={'existing' if character_exists else 'missing'}, "
        f"gameMode={'existing' if game_mode_exists else 'missing'}, "
        f"map={'existing' if map_exists else 'missing'}"
    )


def create_or_load_character(asset_subsystem: unreal.EditorAssetSubsystem) -> unreal.Blueprint:
    if asset_subsystem.does_asset_exist(TARGET_CHARACTER):
        loaded = asset_subsystem.load_asset(TARGET_CHARACTER)
        require(isinstance(loaded, unreal.Blueprint), "M2 character target is not a Blueprint")
        return loaded

    info(f"Duplicating character Blueprint: {SOURCE_CHARACTER} -> {TARGET_CHARACTER}")
    duplicated = asset_subsystem.duplicate_asset(SOURCE_CHARACTER, TARGET_CHARACTER)
    require(isinstance(duplicated, unreal.Blueprint), "M2 character duplication failed")
    expected_parent = load_class_checked(M2_MANUAL_CHARACTER_CLASS)
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicated, expected_parent)
    compile_blueprint_checked(duplicated, TARGET_CHARACTER)
    require(asset_subsystem.save_asset(TARGET_CHARACTER, False), "M2 character save failed")
    return duplicated


def audit_character(
    asset_subsystem: unreal.EditorAssetSubsystem,
    blueprint: unreal.Blueprint,
) -> unreal.Class:
    expected_parent = load_class_checked(M2_MANUAL_CHARACTER_CLASS)
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(
        actual_parent == expected_parent,
        f"Unexpected M2 character parent: {object_path(actual_parent)}",
    )
    compile_blueprint_checked(blueprint, TARGET_CHARACTER)

    character_class = load_blueprint_class_checked(TARGET_CHARACTER)
    defaults = unreal.get_default_object(character_class)
    source_class = load_blueprint_class_checked(SOURCE_CHARACTER)
    source_defaults = unreal.get_default_object(source_class)
    require(defaults is not None and source_defaults is not None, "Character defaults unavailable")

    component_names = (
        "capsule_component",
        "character_movement",
        "mesh",
        "camera_boom",
        "follow_camera",
        "planet_gravity_component",
        "planet_globe_camera_component",
    )
    components = {name: defaults.get_editor_property(name) for name in component_names}
    require(all(components.values()), "M2 character components are incomplete")
    require(
        class_path(components["planet_gravity_component"]) == PLANET_GRAVITY_COMPONENT_CLASS,
        "M2 gravity component class mismatch",
    )
    require(
        class_path(components["planet_globe_camera_component"]) == GLOBE_CAMERA_COMPONENT_CLASS,
        "M2 camera rig component class mismatch",
    )

    source_mesh = source_defaults.get_editor_property("mesh")
    require(source_mesh is not None, "Source mesh component unavailable")
    require(
        components["mesh"].get_editor_property("skeletal_mesh_asset")
        == source_mesh.get_editor_property("skeletal_mesh_asset"),
        "M2 skeletal mesh no longer matches the Third Person source",
    )
    require(
        components["mesh"].get_editor_property("anim_class")
        == source_mesh.get_editor_property("anim_class"),
        "M2 animation class no longer matches the Third Person source",
    )
    for action_name in ("jump_action", "move_action", "look_action", "mouse_look_action"):
        require(
            defaults.get_editor_property(action_name)
            == source_defaults.get_editor_property(action_name),
            f"M2 Input Action mismatch: {action_name}",
        )

    camera_rig = components["planet_globe_camera_component"]
    numeric_contracts = (
        ("camera_arm_length", CAMERA_ARM_CM),
        ("camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
        ("camera_horizontal_fov_degrees", CAMERA_HFOV_DEG),
        ("camera_local_up_offset", CAMERA_LOCAL_UP_OFFSET_CM),
        ("camera_look_ahead_distance", CAMERA_LOOK_AHEAD_CM),
        ("camera_collision_probe_size", CAMERA_PROBE_SIZE_CM),
    )
    for property_name, expected in numeric_contracts:
        actual = float(camera_rig.get_editor_property(property_name))
        require(
            math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-4),
            f"M2 camera property mismatch {property_name}: actual={actual}, expected={expected}",
        )

    info(
        "M2_CHARACTER_AUDIT="
        f"parent={object_path(actual_parent)}, "
        f"rig={class_path(camera_rig)}, arm={CAMERA_ARM_CM:.1f}, "
        f"pitch={CAMERA_DOWN_PITCH_DEG:.1f}, hfov={CAMERA_HFOV_DEG:.1f}, "
        f"upOffset={CAMERA_LOCAL_UP_OFFSET_CM:.1f}, lookAhead={CAMERA_LOOK_AHEAD_CM:.1f}"
    )
    return character_class


def create_or_load_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    character_class: unreal.Class,
) -> unreal.Blueprint:
    if asset_subsystem.does_asset_exist(TARGET_GAME_MODE):
        loaded = asset_subsystem.load_asset(TARGET_GAME_MODE)
        require(isinstance(loaded, unreal.Blueprint), "M2 GameMode target is not a Blueprint")
        return loaded

    info(f"Duplicating GameMode Blueprint: {SOURCE_GAME_MODE} -> {TARGET_GAME_MODE}")
    duplicated = asset_subsystem.duplicate_asset(SOURCE_GAME_MODE, TARGET_GAME_MODE)
    require(isinstance(duplicated, unreal.Blueprint), "M2 GameMode duplication failed")
    compile_blueprint_checked(duplicated, TARGET_GAME_MODE)
    game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
    defaults = unreal.get_default_object(game_mode_class)
    require(defaults is not None, "M2 GameMode defaults unavailable")
    defaults.modify()
    defaults.set_editor_property("default_pawn_class", character_class)
    require(asset_subsystem.save_asset(TARGET_GAME_MODE, False), "M2 GameMode save failed")
    return duplicated


def audit_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    blueprint: unreal.Blueprint,
    character_class: unreal.Class,
) -> unreal.Class:
    source_blueprint = asset_subsystem.load_asset(SOURCE_GAME_MODE)
    require(isinstance(source_blueprint, unreal.Blueprint), "Source GameMode is not a Blueprint")
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    expected_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(source_blueprint)
    require(
        actual_parent == expected_parent,
        "M2 GameMode parent mismatch: "
        f"actual={object_path(actual_parent)}, expected={object_path(expected_parent)}",
    )
    compile_blueprint_checked(blueprint, TARGET_GAME_MODE)
    game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
    defaults = unreal.get_default_object(game_mode_class)
    source_class = load_blueprint_class_checked(SOURCE_GAME_MODE)
    source_defaults = unreal.get_default_object(source_class)
    require(defaults is not None and source_defaults is not None, "GameMode defaults unavailable")
    require(
        defaults.get_editor_property("default_pawn_class") == character_class,
        "M2 GameMode DefaultPawnClass mismatch",
    )
    require(
        defaults.get_editor_property("player_controller_class")
        == source_defaults.get_editor_property("player_controller_class"),
        "M2 GameMode PlayerController no longer matches the Third Person source",
    )
    player_controller_class = defaults.get_editor_property("player_controller_class")
    player_controller_defaults = unreal.get_default_object(player_controller_class)
    require(player_controller_defaults is not None, "M2 PlayerController defaults unavailable")
    default_mapping_contexts = {
        object_path(context)
        for context in player_controller_defaults.get_editor_property("default_mapping_contexts")
    }
    mobile_excluded_mapping_contexts = {
        object_path(context)
        for context in player_controller_defaults.get_editor_property(
            "mobile_excluded_mapping_contexts"
        )
    }
    require(
        DEFAULT_MAPPING_CONTEXT in default_mapping_contexts,
        f"M2 PlayerController is missing {DEFAULT_MAPPING_CONTEXT}: {sorted(default_mapping_contexts)}",
    )
    require(
        MOUSE_LOOK_MAPPING_CONTEXT in mobile_excluded_mapping_contexts,
        "M2 PlayerController is missing the desktop mouse-look context "
        f"{MOUSE_LOOK_MAPPING_CONTEXT}: {sorted(mobile_excluded_mapping_contexts)}",
    )
    info(
        "M2_GAMEMODE_AUDIT="
        f"parent={object_path(actual_parent)}, "
        f"pawn={object_path(defaults.get_editor_property('default_pawn_class'))}, "
        f"controller={object_path(player_controller_class)}, "
        f"contexts={sorted(default_mapping_contexts)}, "
        f"desktopContexts={sorted(mobile_excluded_mapping_contexts)}"
    )
    return game_mode_class


def create_map(character_class: unreal.Class, game_mode_class: unreal.Class) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(level_subsystem is not None, "LevelEditorSubsystem unavailable")
    require(actor_subsystem is not None, "EditorActorSubsystem unavailable")
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    require(not unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP), "Refusing to overwrite existing M2 map")
    require(level_subsystem.new_level(TARGET_MAP, False), "M2 non-partitioned map creation failed")

    world = editor_subsystem.get_editor_world()
    require(world is not None, "M2 editor world unavailable")
    world_settings = world.get_world_settings()
    require(world_settings is not None, "M2 WorldSettings unavailable")
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    world_settings.set_editor_property("kill_z", KILL_Z_CM)

    planet = spawn_actor(
        actor_subsystem,
        load_class_checked(PLANET_BODY_CLASS),
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        PLANET_LABEL,
    )
    planet.set_editor_property("surface_radius", PLANET_RADIUS_CM)

    character_defaults = unreal.get_default_object(character_class)
    capsule = character_defaults.get_editor_property("capsule_component")
    require(capsule is not None, "M2 character capsule unavailable")
    capsule_half_height = float(capsule.get_scaled_capsule_half_height())
    spawn_distance = PLANET_RADIUS_CM + capsule_half_height + SPAWN_CLEARANCE_CM
    spawn_actor(
        actor_subsystem,
        load_class_checked(PLAYER_START_CLASS),
        unreal.Vector(0.0, 0.0, spawn_distance),
        unreal.Rotator(0.0, 0.0, 0.0),
        PLAYER_START_LABEL,
    )

    directional_light = spawn_actor(
        actor_subsystem,
        load_class_checked(DIRECTIONAL_LIGHT_CLASS),
        unreal.Vector(0.0, 0.0, spawn_distance + 1000.0),
        unreal.Rotator(-35.0, -135.0, 0.0),
        DIRECTIONAL_LIGHT_LABEL,
    )
    directional_component = set_component_movable(
        directional_light,
        "directional_light_component",
    )
    directional_component.set_editor_property("intensity", 5.0)
    directional_component.set_editor_property("atmosphere_sun_light", True)

    sky_light = spawn_actor(
        actor_subsystem,
        load_class_checked(SKY_LIGHT_CLASS),
        unreal.Vector(0.0, 0.0, spawn_distance + 500.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        SKY_LIGHT_LABEL,
    )
    sky_component = set_component_movable(sky_light, "light_component")
    sky_component.set_editor_property("real_time_capture", True)

    sky_atmosphere = spawn_actor(
        actor_subsystem,
        load_class_checked(SKY_ATMOSPHERE_CLASS),
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        SKY_ATMOSPHERE_LABEL,
    )
    atmosphere_component = sky_atmosphere.get_editor_property("sky_atmosphere_component")
    require(atmosphere_component is not None, "SkyAtmosphere component unavailable")
    atmosphere_component.set_editor_property(
        "transform_mode",
        unreal.SkyAtmosphereTransformMode.PLANET_CENTER_AT_COMPONENT_TRANSFORM,
    )
    atmosphere_component.set_editor_property("bottom_radius", ATMOSPHERE_BOTTOM_RADIUS_KM)
    atmosphere_component.set_editor_property("atmosphere_height", ATMOSPHERE_HEIGHT_KM)

    require(level_subsystem.save_current_level(), "M2 map save failed")
    info(
        "M2_MAP_CREATED="
        f"radius={PLANET_RADIUS_CM:.1f}cm, capsuleHalf={capsule_half_height:.1f}cm, "
        f"spawnDistance={spawn_distance:.1f}cm, atmosphereBottom={ATMOSPHERE_BOTTOM_RADIUS_KM:.3f}km"
    )


def audit_map(character_class: unreal.Class, game_mode_class: unreal.Class) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(level_subsystem.load_level(TARGET_MAP), "Existing M2 map could not be loaded")
    world = editor_subsystem.get_editor_world()
    require(world is not None, "M2 editor world unavailable during audit")
    world_settings = world.get_world_settings()
    require(
        world_settings.get_editor_property("default_game_mode") == game_mode_class,
        "M2 map GameMode override mismatch",
    )
    require(
        math.isclose(
            float(world_settings.get_editor_property("kill_z")),
            KILL_Z_CM,
            rel_tol=0.0,
            abs_tol=0.1,
        ),
        "M2 map KillZ mismatch",
    )

    actors = actor_subsystem.get_all_level_actors()
    require(len(actors) == len(EXPECTED_LABELS), f"M2 map actor count mismatch: {len(actors)}")
    by_label = {actor.get_actor_label(): actor for actor in actors}
    require(len(by_label) == len(actors), "M2 map actor labels are not unique")
    require(set(by_label) == EXPECTED_LABELS, f"M2 map labels mismatch: {sorted(by_label)}")
    expected_classes = {
        PLANET_LABEL: PLANET_BODY_CLASS,
        PLAYER_START_LABEL: PLAYER_START_CLASS,
        DIRECTIONAL_LIGHT_LABEL: DIRECTIONAL_LIGHT_CLASS,
        SKY_LIGHT_LABEL: SKY_LIGHT_CLASS,
        SKY_ATMOSPHERE_LABEL: SKY_ATMOSPHERE_CLASS,
    }
    for label, expected_class in expected_classes.items():
        require(
            class_path(by_label[label]) == expected_class,
            f"M2 actor class mismatch {label}: {class_path(by_label[label])}",
        )

    planet = by_label[PLANET_LABEL]
    planet_location = planet.get_actor_location()
    require(
        planet_location.length() <= 0.1,
        f"M2 PlanetBody is not centered at world origin: {planet_location}",
    )
    require(
        math.isclose(float(planet.get_surface_radius()), PLANET_RADIUS_CM, abs_tol=1.0e-4),
        "M2 PlanetBody radius mismatch",
    )
    defaults = unreal.get_default_object(character_class)
    capsule = defaults.get_editor_property("capsule_component")
    expected_spawn_distance = (
        PLANET_RADIUS_CM
        + float(capsule.get_scaled_capsule_half_height())
        + SPAWN_CLEARANCE_CM
    )
    player_start_location = by_label[PLAYER_START_LABEL].get_actor_location()
    player_start_distance = player_start_location.length()
    require(
        math.isclose(player_start_distance, expected_spawn_distance, abs_tol=0.1),
        f"M2 PlayerStart distance mismatch: {player_start_distance}",
    )
    require(
        abs(player_start_location.x) <= 0.1
        and abs(player_start_location.y) <= 0.1
        and math.isclose(
            player_start_location.z,
            expected_spawn_distance,
            rel_tol=0.0,
            abs_tol=0.1,
        ),
        f"M2 PlayerStart is not on the authored north radial: {player_start_location}",
    )

    directional_component = by_label[DIRECTIONAL_LIGHT_LABEL].get_editor_property(
        "directional_light_component"
    )
    require(directional_component is not None, "M2 DirectionalLight component unavailable")
    require(
        directional_component.get_editor_property("mobility")
        == unreal.ComponentMobility.MOVABLE,
        "M2 DirectionalLight is not Movable",
    )
    require(
        math.isclose(
            float(directional_component.get_editor_property("intensity")),
            5.0,
            rel_tol=0.0,
            abs_tol=1.0e-4,
        ),
        "M2 DirectionalLight intensity mismatch",
    )
    require(
        bool(directional_component.get_editor_property("atmosphere_sun_light")),
        "M2 sun is not linked to atmosphere",
    )
    sky_component = by_label[SKY_LIGHT_LABEL].get_editor_property("light_component")
    require(sky_component is not None, "M2 SkyLight component unavailable")
    require(
        sky_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        "M2 SkyLight is not Movable",
    )
    require(
        bool(sky_component.get_editor_property("real_time_capture")),
        "M2 SkyLight real-time capture disabled",
    )
    atmosphere_component = by_label[SKY_ATMOSPHERE_LABEL].get_editor_property(
        "sky_atmosphere_component"
    )
    require(atmosphere_component is not None, "M2 SkyAtmosphere component unavailable")
    require(
        by_label[SKY_ATMOSPHERE_LABEL].get_actor_location().length() <= 0.1,
        "M2 SkyAtmosphere center no longer matches the PlanetBody center",
    )
    require(
        atmosphere_component.get_editor_property("transform_mode")
        == unreal.SkyAtmosphereTransformMode.PLANET_CENTER_AT_COMPONENT_TRANSFORM,
        "M2 SkyAtmosphere transform mode mismatch",
    )
    require(
        math.isclose(
            float(atmosphere_component.get_editor_property("bottom_radius")),
            ATMOSPHERE_BOTTOM_RADIUS_KM,
            abs_tol=1.0e-4,
        ),
        "M2 SkyAtmosphere ground radius mismatch",
    )
    require(
        math.isclose(
            float(atmosphere_component.get_editor_property("atmosphere_height")),
            ATMOSPHERE_HEIGHT_KM,
            rel_tol=0.0,
            abs_tol=1.0e-4,
        ),
        "M2 SkyAtmosphere height mismatch",
    )
    info(
        "M2_MAP_AUDIT="
        f"actors={len(actors)}, radius={PLANET_RADIUS_CM:.1f}cm, "
        f"playerStart={player_start_distance:.1f}cm, atmosphere=planet-center"
    )


def main() -> None:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    require(asset_subsystem.does_asset_exist(SOURCE_CHARACTER), "Source character missing")
    require(asset_subsystem.does_asset_exist(SOURCE_GAME_MODE), "Source GameMode missing")
    preflight_existing_targets(asset_subsystem)
    asset_subsystem.make_directory(f"{TARGET_ROOT}/Blueprints")
    asset_subsystem.make_directory(f"{TARGET_ROOT}/Maps")

    character_blueprint = create_or_load_character(asset_subsystem)
    character_class = audit_character(asset_subsystem, character_blueprint)
    game_mode_blueprint = create_or_load_game_mode(asset_subsystem, character_class)
    game_mode_class = audit_game_mode(
        asset_subsystem,
        game_mode_blueprint,
        character_class,
    )

    if asset_subsystem.does_asset_exist(TARGET_MAP):
        audit_map(character_class, game_mode_class)
    else:
        create_map(character_class, game_mode_class)
        audit_map(character_class, game_mode_class)

    info("M2_CONTENT_RESULT=VERIFIED")


try:
    main()
except Exception as exc:
    unreal.log_error(f"[EverPlanet M2 Content] M2_CONTENT_RESULT=FAILED error={exc}")
    raise
