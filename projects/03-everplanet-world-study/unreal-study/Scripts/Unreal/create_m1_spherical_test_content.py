"""Create or verify the isolated M1 spherical-gravity test content.

Run from Unreal Editor 5.8 with ``-ExecutePythonScript``.  The script is
deliberately conservative:

* Missing target assets are created in dependency order.
* Existing targets are verified before they are reused.
* The exact M1 v1 spherical parent can be migrated once to the v2 manual-probe parent.
* A blank map left by an interrupted first run can be completed safely.
* Unexpected existing target assets are never overwritten or deleted.

The existing Third Person map and project default-map settings are never changed.
"""

from __future__ import annotations

import math

import unreal


SOURCE_CHARACTER = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
SOURCE_GAME_MODE = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode"

TARGET_ROOT = "/Game/EverPlanet/Tests/M1"
TARGET_CHARACTER = f"{TARGET_ROOT}/Blueprints/BP_M1SphericalCharacter"
TARGET_GAME_MODE = f"{TARGET_ROOT}/Blueprints/BP_M1SphericalGameMode"
TARGET_MAP = f"{TARGET_ROOT}/Maps/Lvl_M1SphericalGravity"

SPHERICAL_CHARACTER_CLASS = "/Script/EverPlanet.EverPlanetSphericalCharacter"
MANUAL_TEST_CHARACTER_CLASS = "/Script/EverPlanet.M1SphericalManualTestCharacter"
PLANET_BODY_CLASS = "/Script/EverPlanet.PlanetBody"
PLANET_GRAVITY_COMPONENT_CLASS = "/Script/EverPlanet.PlanetGravityComponent"
PLAYER_START_CLASS = "/Script/Engine.PlayerStart"
DIRECTIONAL_LIGHT_CLASS = "/Script/Engine.DirectionalLight"
SKY_LIGHT_CLASS = "/Script/Engine.SkyLight"
SKY_ATMOSPHERE_CLASS = "/Script/Engine.SkyAtmosphere"
DEFAULT_MAPPING_CONTEXT = "/Game/Input/IMC_Default.IMC_Default"
MOUSE_LOOK_MAPPING_CONTEXT = "/Game/Input/IMC_MouseLook.IMC_MouseLook"

PLANET_LABEL = "PlanetBody_0"
PLAYER_START_LABEL = "PlayerStart_0"
PLANET_RADIUS_CM = 3000.0
SPAWN_CLEARANCE_CM = 25.0
KILL_Z_CM = -1000000.0


def info(message: str) -> None:
    unreal.log(f"[EverPlanet M1 Content] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet M1 Content] {message}")
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
    status = blueprint.get_editor_property("status")
    info(f"Compiled {label}; status={status}")


def create_character_blueprint(asset_subsystem: unreal.EditorAssetSubsystem) -> unreal.Blueprint:
    info(f"Duplicating character Blueprint: {SOURCE_CHARACTER} -> {TARGET_CHARACTER}")
    duplicated = asset_subsystem.duplicate_asset(SOURCE_CHARACTER, TARGET_CHARACTER)
    require(isinstance(duplicated, unreal.Blueprint), "Character Blueprint duplication failed")

    manual_probe_parent = load_class_checked(MANUAL_TEST_CHARACTER_CLASS)
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicated, manual_probe_parent)
    compile_blueprint_checked(duplicated, TARGET_CHARACTER)
    require(asset_subsystem.save_asset(TARGET_CHARACTER, False), "Character Blueprint save failed")

    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(duplicated)
    require(
        actual_parent == manual_probe_parent,
        f"Character parent mismatch after reparent: {object_path(actual_parent)}",
    )
    return duplicated


def migrate_character_parent_if_needed(
    asset_subsystem: unreal.EditorAssetSubsystem,
) -> unreal.Blueprint:
    """Perform the single supported M1 v1 -> v2 parent migration.

    Any parent other than the exact legacy spherical class or the expected manual
    probe class fails closed so this script cannot replace an intentional user
    reparent.
    """

    blueprint = asset_subsystem.load_asset(TARGET_CHARACTER)
    require(isinstance(blueprint, unreal.Blueprint), "M1 character asset is not a Blueprint")

    expected_parent = load_class_checked(MANUAL_TEST_CHARACTER_CLASS)
    legacy_parent = load_class_checked(SPHERICAL_CHARACTER_CLASS)
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    if actual_parent == expected_parent:
        return blueprint

    require(
        actual_parent == legacy_parent,
        "Unexpected M1 character parent; refusing automatic reparent: "
        + object_path(actual_parent),
    )
    info(
        "Migrating M1 character parent: "
        f"{object_path(legacy_parent)} -> {object_path(expected_parent)}"
    )
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, expected_parent)
    compile_blueprint_checked(blueprint, TARGET_CHARACTER)
    require(asset_subsystem.save_asset(TARGET_CHARACTER, False), "Migrated character save failed")
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(actual_parent == expected_parent, "M1 character parent migration did not persist")
    info("M1_CONTENT_MIGRATION=V1_SPHERICAL_TO_V2_MANUAL_PROBE")
    return blueprint


def create_game_mode_blueprint(
    asset_subsystem: unreal.EditorAssetSubsystem,
    character_class: unreal.Class,
) -> unreal.Blueprint:
    info(f"Duplicating GameMode Blueprint: {SOURCE_GAME_MODE} -> {TARGET_GAME_MODE}")
    duplicated = asset_subsystem.duplicate_asset(SOURCE_GAME_MODE, TARGET_GAME_MODE)
    require(isinstance(duplicated, unreal.Blueprint), "GameMode Blueprint duplication failed")
    compile_blueprint_checked(duplicated, TARGET_GAME_MODE)

    game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
    game_mode_defaults = unreal.get_default_object(game_mode_class)
    require(game_mode_defaults is not None, "GameMode class default object is unavailable")
    game_mode_defaults.modify()
    game_mode_defaults.set_editor_property("default_pawn_class", character_class)
    require(asset_subsystem.save_asset(TARGET_GAME_MODE, False), "GameMode Blueprint save failed")

    actual_pawn_class = game_mode_defaults.get_editor_property("default_pawn_class")
    require(
        actual_pawn_class == character_class,
        f"DefaultPawnClass mismatch: {object_path(actual_pawn_class)}",
    )
    return duplicated


def set_actor_label(actor: unreal.Actor, label: str) -> None:
    actor.set_actor_label(label, mark_dirty=True)
    require(actor.get_actor_label() == label, f"Actor label could not be set: {label}")


def set_component_movable(actor: unreal.Actor, property_name: str) -> unreal.ActorComponent:
    component = actor.get_editor_property(property_name)
    require(component is not None, f"Missing component property {property_name} on {actor.get_name()}")
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    return component


def spawn_map_actor(
    actor_subsystem: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    location: unreal.Vector,
    rotation: unreal.Rotator,
    label: str,
) -> unreal.Actor:
    actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation, False)
    require(actor is not None, f"Failed to spawn actor: {label}")
    set_actor_label(actor, label)
    return actor


def create_test_map(character_class: unreal.Class, game_mode_class: unreal.Class) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal_editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(level_subsystem is not None, "LevelEditorSubsystem is unavailable")
    require(actor_subsystem is not None, "EditorActorSubsystem is unavailable")
    require(unreal_editor_subsystem is not None, "UnrealEditorSubsystem is unavailable")

    map_already_exists = unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP)
    if map_already_exists:
        info(f"Loading existing test map for verification or safe completion: {TARGET_MAP}")
        require(level_subsystem.load_level(TARGET_MAP), "Existing test map could not be loaded")
    else:
        info(f"Creating non-partitioned test map: {TARGET_MAP}")
        require(level_subsystem.new_level(TARGET_MAP, False), "Test map creation failed")

    world = unreal_editor_subsystem.get_editor_world()
    require(world is not None, "New test map did not produce an editor world")
    world_settings = world.get_world_settings()
    require(world_settings is not None, "WorldSettings is unavailable")

    expected_labels = {
        PLANET_LABEL,
        PLAYER_START_LABEL,
        "DirectionalLight_0",
        "SkyLight_0",
        "SkyAtmosphere_0",
    }
    existing_actors = actor_subsystem.get_all_level_actors()
    existing_labels = {
        actor.get_actor_label()
        for actor in existing_actors
    }
    matching_labels = expected_labels.intersection(existing_labels)
    if matching_labels == expected_labels:
        info("Existing test map already contains the complete expected actor set")
        return
    require(
        not matching_labels,
        "Partially populated test map detected; refusing to duplicate actors: "
        + ", ".join(sorted(matching_labels)),
    )
    require(
        not map_already_exists or not existing_actors,
        "Existing test map contains unrelated actors; refusing to mutate or save it: "
        + ", ".join(sorted(existing_labels)),
    )

    world_settings.set_editor_property("default_game_mode", game_mode_class)
    world_settings.set_editor_property("kill_z", KILL_Z_CM)

    planet_class = load_class_checked(PLANET_BODY_CLASS)
    player_start_class = load_class_checked(PLAYER_START_CLASS)

    planet = spawn_map_actor(
        actor_subsystem,
        planet_class,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        PLANET_LABEL,
    )
    planet.set_editor_property("surface_radius", PLANET_RADIUS_CM)

    character_defaults = unreal.get_default_object(character_class)
    require(character_defaults is not None, "Character class default object is unavailable")
    capsule = character_defaults.get_editor_property("capsule_component")
    require(capsule is not None, "Character capsule component is unavailable")
    capsule_half_height = float(capsule.get_unscaled_capsule_half_height())
    spawn_distance = PLANET_RADIUS_CM + capsule_half_height + SPAWN_CLEARANCE_CM

    spawn_map_actor(
        actor_subsystem,
        player_start_class,
        unreal.Vector(0.0, 0.0, spawn_distance),
        unreal.Rotator(0.0, 0.0, 0.0),
        PLAYER_START_LABEL,
    )

    directional_light = spawn_map_actor(
        actor_subsystem,
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, spawn_distance + 1000.0),
        unreal.Rotator(-35.0, -135.0, 0.0),
        "DirectionalLight_0",
    )
    directional_component = set_component_movable(
        directional_light,
        "directional_light_component",
    )
    directional_component.set_editor_property("intensity", 5.0)

    sky_light = spawn_map_actor(
        actor_subsystem,
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, spawn_distance + 500.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "SkyLight_0",
    )
    sky_component = set_component_movable(sky_light, "light_component")
    sky_component.set_editor_property("real_time_capture", True)

    spawn_map_actor(
        actor_subsystem,
        unreal.SkyAtmosphere,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "SkyAtmosphere_0",
    )

    require(level_subsystem.save_current_level(), "Test map save failed")
    info(
        "Created map with "
        f"SurfaceRadius={PLANET_RADIUS_CM:.1f} cm, "
        f"CapsuleHalfHeight={capsule_half_height:.1f} cm, "
        f"SpawnClearance={SPAWN_CLEARANCE_CM:.1f} cm, "
        f"PlayerStartDistance={spawn_distance:.1f} cm"
    )


def audit_character(asset_subsystem: unreal.EditorAssetSubsystem) -> unreal.Class:
    blueprint = asset_subsystem.load_asset(TARGET_CHARACTER)
    require(isinstance(blueprint, unreal.Blueprint), "M1 character asset is not a Blueprint")
    compile_blueprint_checked(blueprint, TARGET_CHARACTER)

    expected_parent = load_class_checked(MANUAL_TEST_CHARACTER_CLASS)
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(actual_parent == expected_parent, f"Unexpected character parent: {object_path(actual_parent)}")

    character_class = load_blueprint_class_checked(TARGET_CHARACTER)
    defaults = unreal.get_default_object(character_class)
    capsule = defaults.get_editor_property("capsule_component")
    movement = defaults.get_editor_property("character_movement")
    mesh = defaults.get_editor_property("mesh")
    camera_boom = defaults.get_editor_property("camera_boom")
    follow_camera = defaults.get_editor_property("follow_camera")
    gravity_component = defaults.get_editor_property("planet_gravity_component")

    require(all((capsule, movement, mesh, camera_boom, follow_camera, gravity_component)), "Character components are incomplete")
    require(
        class_path(gravity_component) == PLANET_GRAVITY_COMPONENT_CLASS,
        f"Unexpected gravity component class: {class_path(gravity_component)}",
    )

    source_class = load_blueprint_class_checked(SOURCE_CHARACTER)
    source_defaults = unreal.get_default_object(source_class)
    require(source_defaults is not None, "Source character class default object is unavailable")
    source_capsule = source_defaults.get_editor_property("capsule_component")
    source_movement = source_defaults.get_editor_property("character_movement")
    source_mesh = source_defaults.get_editor_property("mesh")
    source_camera_boom = source_defaults.get_editor_property("camera_boom")
    require(
        all((source_capsule, source_movement, source_mesh, source_camera_boom)),
        "Source character components are incomplete",
    )

    numeric_contracts = (
        (
            "capsule radius",
            float(capsule.get_unscaled_capsule_radius()),
            float(source_capsule.get_unscaled_capsule_radius()),
        ),
        (
            "capsule half height",
            float(capsule.get_unscaled_capsule_half_height()),
            float(source_capsule.get_unscaled_capsule_half_height()),
        ),
        (
            "maximum walk speed",
            float(movement.get_editor_property("max_walk_speed")),
            float(source_movement.get_editor_property("max_walk_speed")),
        ),
        (
            "jump Z velocity",
            float(movement.get_editor_property("jump_z_velocity")),
            float(source_movement.get_editor_property("jump_z_velocity")),
        ),
        (
            "gravity scale",
            float(movement.get_editor_property("gravity_scale")),
            float(source_movement.get_editor_property("gravity_scale")),
        ),
        (
            "camera arm length",
            float(camera_boom.get_editor_property("target_arm_length")),
            float(source_camera_boom.get_editor_property("target_arm_length")),
        ),
    )
    for label, actual, expected in numeric_contracts:
        require(
            math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-4),
            f"Character {label} mismatch: actual={actual}, expected={expected}",
        )

    require(
        mesh.get_editor_property("skeletal_mesh_asset")
        == source_mesh.get_editor_property("skeletal_mesh_asset"),
        "Character skeletal mesh no longer matches the Third Person source",
    )
    require(
        mesh.get_editor_property("anim_class")
        == source_mesh.get_editor_property("anim_class"),
        "Character animation class no longer matches the Third Person source",
    )
    for action_property in (
        "jump_action",
        "move_action",
        "look_action",
        "mouse_look_action",
    ):
        actual_action = defaults.get_editor_property(action_property)
        expected_action = source_defaults.get_editor_property(action_property)
        require(expected_action is not None, f"Source {action_property} is missing")
        require(
            actual_action == expected_action,
            f"Character {action_property} mismatch: "
            f"actual={object_path(actual_action)}, expected={object_path(expected_action)}",
        )
    info(
        "Character audit: "
        f"class={object_path(character_class)}, "
        f"parent={object_path(actual_parent)}, "
        f"capsule=(r={capsule.get_unscaled_capsule_radius():.1f}, h={capsule.get_unscaled_capsule_half_height():.1f}), "
        f"mesh={object_path(mesh.get_editor_property('skeletal_mesh_asset'))}, "
        f"anim={object_path(mesh.get_editor_property('anim_class'))}, "
        f"walk_speed={movement.get_editor_property('max_walk_speed'):.1f}, "
        f"jump_z={movement.get_editor_property('jump_z_velocity'):.1f}, "
        f"gravity_scale={movement.get_editor_property('gravity_scale'):.2f}, "
        f"camera_arm={camera_boom.get_editor_property('target_arm_length'):.1f}, "
        f"gravity_component={object_path(gravity_component)}"
    )
    return character_class


def audit_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    character_class: unreal.Class,
) -> unreal.Class:
    blueprint = asset_subsystem.load_asset(TARGET_GAME_MODE)
    require(isinstance(blueprint, unreal.Blueprint), "M1 GameMode asset is not a Blueprint")
    compile_blueprint_checked(blueprint, TARGET_GAME_MODE)

    game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
    defaults = unreal.get_default_object(game_mode_class)
    actual_pawn_class = defaults.get_editor_property("default_pawn_class")
    require(actual_pawn_class == character_class, f"Unexpected DefaultPawnClass: {object_path(actual_pawn_class)}")
    source_game_mode_class = load_blueprint_class_checked(SOURCE_GAME_MODE)
    source_defaults = unreal.get_default_object(source_game_mode_class)
    require(source_defaults is not None, "Source GameMode class default object is unavailable")
    expected_player_controller = source_defaults.get_editor_property("player_controller_class")
    actual_player_controller = defaults.get_editor_property("player_controller_class")
    require(expected_player_controller is not None, "Source PlayerControllerClass is missing")
    require(
        actual_player_controller == expected_player_controller,
        "Unexpected PlayerControllerClass: "
        f"actual={object_path(actual_player_controller)}, "
        f"expected={object_path(expected_player_controller)}",
    )
    player_controller_defaults = unreal.get_default_object(actual_player_controller)
    require(player_controller_defaults is not None, "PlayerController class default object is unavailable")
    default_mapping_contexts = {
        object_path(context)
        for context in player_controller_defaults.get_editor_property("default_mapping_contexts")
    }
    mobile_excluded_mapping_contexts = {
        object_path(context)
        for context in player_controller_defaults.get_editor_property("mobile_excluded_mapping_contexts")
    }
    require(
        DEFAULT_MAPPING_CONTEXT in default_mapping_contexts,
        f"PlayerController is missing {DEFAULT_MAPPING_CONTEXT}: {sorted(default_mapping_contexts)}",
    )
    require(
        MOUSE_LOOK_MAPPING_CONTEXT in mobile_excluded_mapping_contexts,
        "PlayerController is missing the desktop mouse-look mapping context "
        f"{MOUSE_LOOK_MAPPING_CONTEXT}: {sorted(mobile_excluded_mapping_contexts)}",
    )
    info(
        "GameMode audit: "
        f"class={object_path(game_mode_class)}, "
        f"default_pawn={object_path(actual_pawn_class)}, "
        f"player_controller={object_path(actual_player_controller)}, "
        f"default_mapping_contexts={sorted(default_mapping_contexts)}, "
        f"mobile_excluded_mapping_contexts={sorted(mobile_excluded_mapping_contexts)}"
    )
    return game_mode_class


def audit_map(game_mode_class: unreal.Class, character_class: unreal.Class) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal_editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(level_subsystem.load_level(TARGET_MAP), f"Could not load test map: {TARGET_MAP}")

    world = unreal_editor_subsystem.get_editor_world()
    require(world is not None, "Loaded map has no editor world")
    world_settings = world.get_world_settings()
    require(world_settings.get_editor_property("default_game_mode") == game_mode_class, "Map GameMode override mismatch")
    require(math.isclose(float(world_settings.get_editor_property("kill_z")), KILL_Z_CM), "Map KillZ mismatch")

    actors = actor_subsystem.get_all_level_actors()
    actors_by_label: dict[str, list[unreal.Actor]] = {}
    for actor in actors:
        actors_by_label.setdefault(actor.get_actor_label(), []).append(actor)
    required_labels = {
        PLANET_LABEL,
        PLAYER_START_LABEL,
        "DirectionalLight_0",
        "SkyLight_0",
        "SkyAtmosphere_0",
    }
    require(required_labels.issubset(actors_by_label), f"Map actor labels missing: {sorted(required_labels - set(actors_by_label))}")
    require(
        len(actors) == len(required_labels),
        f"Isolated M1 map actor count mismatch: actual={len(actors)}, expected={len(required_labels)}",
    )
    for label in required_labels:
        require(
            len(actors_by_label[label]) == 1,
            f"Map actor label must be unique: {label} count={len(actors_by_label[label])}",
        )

    planet_actors = [actor for actor in actors if class_path(actor) == PLANET_BODY_CLASS]
    player_start_actors = [actor for actor in actors if class_path(actor) == PLAYER_START_CLASS]
    require(len(planet_actors) == 1, f"Expected exactly one PlanetBody, found {len(planet_actors)}")
    require(len(player_start_actors) == 1, f"Expected exactly one PlayerStart, found {len(player_start_actors)}")

    planet = actors_by_label[PLANET_LABEL][0]
    player_start = actors_by_label[PLAYER_START_LABEL][0]
    directional_light = actors_by_label["DirectionalLight_0"][0]
    sky_light = actors_by_label["SkyLight_0"][0]
    sky_atmosphere = actors_by_label["SkyAtmosphere_0"][0]
    require(planet is planet_actors[0], "PlanetBody_0 label does not identify the only PlanetBody")
    require(player_start is player_start_actors[0], "PlayerStart_0 label does not identify the only PlayerStart")
    require(class_path(planet) == PLANET_BODY_CLASS, f"Unexpected PlanetBody class: {class_path(planet)}")
    require(class_path(player_start) == PLAYER_START_CLASS, f"Unexpected PlayerStart class: {class_path(player_start)}")
    require(
        class_path(directional_light) == DIRECTIONAL_LIGHT_CLASS,
        f"Unexpected DirectionalLight class: {class_path(directional_light)}",
    )
    require(class_path(sky_light) == SKY_LIGHT_CLASS, f"Unexpected SkyLight class: {class_path(sky_light)}")
    require(
        class_path(sky_atmosphere) == SKY_ATMOSPHERE_CLASS,
        f"Unexpected SkyAtmosphere class: {class_path(sky_atmosphere)}",
    )
    require(math.isclose(float(planet.get_surface_radius()), PLANET_RADIUS_CM), "Planet radius mismatch")

    directional_component = directional_light.get_editor_property("directional_light_component")
    sky_component = sky_light.get_editor_property("light_component")
    require(directional_component is not None, "DirectionalLight component is missing")
    require(sky_component is not None, "SkyLight component is missing")
    require(
        directional_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        "DirectionalLight mobility is not Movable",
    )
    require(
        math.isclose(
            float(directional_component.get_editor_property("intensity")),
            5.0,
            rel_tol=0.0,
            abs_tol=1.0e-4,
        ),
        "DirectionalLight intensity mismatch",
    )
    require(
        sky_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        "SkyLight mobility is not Movable",
    )
    require(bool(sky_component.get_editor_property("real_time_capture")), "SkyLight real-time capture is disabled")

    center = planet.get_actor_location()
    spawn = player_start.get_actor_location()
    delta_x = spawn.x - center.x
    delta_y = spawn.y - center.y
    delta_z = spawn.z - center.z
    distance = math.sqrt(
        delta_x**2
        + delta_y**2
        + delta_z**2
    )
    character_defaults = unreal.get_default_object(character_class)
    require(character_defaults is not None, "Character class default object is unavailable for map audit")
    capsule = character_defaults.get_editor_property("capsule_component")
    require(capsule is not None, "Character capsule is unavailable for map audit")
    expected_spawn_distance = (
        PLANET_RADIUS_CM
        + float(capsule.get_scaled_capsule_half_height())
        + SPAWN_CLEARANCE_CM
    )
    require(
        math.isclose(distance, expected_spawn_distance, rel_tol=0.0, abs_tol=0.1),
        "PlayerStart center distance mismatch: "
        f"actual={distance:.3f}, expected={expected_spawn_distance:.3f}",
    )
    require(
        abs(delta_x) <= 0.1 and abs(delta_y) <= 0.1 and delta_z > 0.0,
        "PlayerStart is not on the authored north radial direction",
    )
    info(
        "Map audit: "
        f"world={object_path(world)}, "
        f"game_mode={object_path(world_settings.get_editor_property('default_game_mode'))}, "
        f"kill_z={world_settings.get_editor_property('kill_z')}, "
        f"actors={len(actors)}, "
        f"planet_center=({center.x:.1f},{center.y:.1f},{center.z:.1f}), "
        f"player_start_distance={distance:.1f} cm"
    )


def audit_all(asset_subsystem: unreal.EditorAssetSubsystem) -> None:
    character_class = audit_character(asset_subsystem)
    game_mode_class = audit_game_mode(asset_subsystem, character_class)
    audit_map(game_mode_class, character_class)
    info("M1_CONTENT_RESULT=VERIFIED")


def main() -> None:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem is unavailable")

    require(asset_subsystem.does_asset_exist(SOURCE_CHARACTER), f"Missing source asset: {SOURCE_CHARACTER}")
    require(asset_subsystem.does_asset_exist(SOURCE_GAME_MODE), f"Missing source asset: {SOURCE_GAME_MODE}")

    target_paths = (TARGET_CHARACTER, TARGET_GAME_MODE, TARGET_MAP)
    existed_before = {
        path: bool(asset_subsystem.does_asset_exist(path))
        for path in target_paths
    }

    if existed_before[TARGET_CHARACTER]:
        info("Existing M1 character found; checking the supported parent migration")
        character_blueprint = migrate_character_parent_if_needed(asset_subsystem)
        character_class = audit_character(asset_subsystem)
    else:
        character_blueprint = create_character_blueprint(asset_subsystem)
        character_class = load_blueprint_class_checked(TARGET_CHARACTER)

    if existed_before[TARGET_GAME_MODE]:
        info("Existing M1 GameMode found; verifying instead of overwriting")
        game_mode_class = audit_game_mode(asset_subsystem, character_class)
    else:
        create_game_mode_blueprint(asset_subsystem, character_class)
        game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)

    create_test_map(character_class, game_mode_class)

    # Keep a strong reference until every package has been saved and audited.
    require(character_blueprint is not None, "Character Blueprint unexpectedly became unavailable")
    audit_all(asset_subsystem)
    if all(existed_before.values()):
        info("M1_CONTENT_RESULT=VERIFIED_OR_SAFELY_COMPLETED")
    else:
        info("M1_CONTENT_RESULT=CREATED_AND_VERIFIED")


if __name__ == "__main__":
    main()
