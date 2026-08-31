"""
Create and verify the first product-facing EverPlanet starter planet graybox.

Run this script only through UnrealEditor.exe with -ExecutePythonScript.
It intentionally never deletes or overwrites an unexpected existing asset.
Existing targets are reused only after their exact product contract is audited.

Created content:
  /Game/EverPlanet/Product/Characters/BP_EverPlanetPlayerCharacter
  /Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode
  /Game/EverPlanet/Product/Maps/Lvl_FirstPlanet_Graybox
"""

from __future__ import annotations

import math

import unreal


SOURCE_CHARACTER = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
SOURCE_GAME_MODE = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode"
DEFAULT_MAPPING_CONTEXT = "/Game/Input/IMC_Default.IMC_Default"
MOUSE_LOOK_MAPPING_CONTEXT = "/Game/Input/IMC_MouseLook.IMC_MouseLook"

TARGET_ROOT = "/Game/EverPlanet/Product"
TARGET_CHARACTER = f"{TARGET_ROOT}/Characters/BP_EverPlanetPlayerCharacter"
TARGET_GAME_MODE = f"{TARGET_ROOT}/GameModes/BP_EverPlanetGameMode"
TARGET_MAP = f"{TARGET_ROOT}/Maps/Lvl_FirstPlanet_Graybox"

GLOBE_CHARACTER_CLASS = "/Script/EverPlanet.EverPlanetGlobeCameraCharacter"
GLOBE_CAMERA_COMPONENT_CLASS = "/Script/EverPlanet.PlanetGlobeCameraComponent"
PLANET_GRAVITY_COMPONENT_CLASS = "/Script/EverPlanet.PlanetGravityComponent"
PLANET_BODY_CLASS = "/Script/EverPlanet.PlanetBody"
PLAYER_START_CLASS = "/Script/Engine.PlayerStart"
STATIC_MESH_ACTOR_CLASS = "/Script/Engine.StaticMeshActor"
DIRECTIONAL_LIGHT_CLASS = "/Script/Engine.DirectionalLight"
SKY_LIGHT_CLASS = "/Script/Engine.SkyLight"
SKY_ATMOSPHERE_CLASS = "/Script/Engine.SkyAtmosphere"

CUBE_MESH = "/Engine/BasicShapes/Cube"
SPHERE_MESH = "/Engine/BasicShapes/Sphere"
PLANET_MATERIAL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02"
ROUTE_MATERIAL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark"
PLAZA_MATERIAL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray"
LANDMARK_MATERIAL_A = "/Game/LevelPrototyping/Materials/MI_DefaultColorway"
LANDMARK_MATERIAL_B = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_Round"

PLANET_LABEL = "PlanetBody_0"
PLAYER_START_LABEL = "PlayerStart_0"
MAIN_LIGHT_LABEL = "DirectionalLight_Main"
FILL_LIGHT_LABEL = "DirectionalLight_Fill"
SKY_LIGHT_LABEL = "SkyLight_0"
SKY_ATMOSPHERE_LABEL = "SkyAtmosphere_0"

PLANET_RADIUS_CM = 10000.0
ENGINE_SPHERE_RADIUS_CM = 50.0
SPAWN_CLEARANCE_CM = 25.0
KILL_Z_CM = -1000000.0

# Rotation tuples in this script always use (pitch, yaw, roll).  Unreal Python's
# Rotator constructor uses (roll, pitch, yaw), so spawn_actor maps them by name.
PLAYER_START_ROTATION = (0.0, 0.0, 0.0)
MAIN_LIGHT_ROTATION = (-35.0, -135.0, 0.0)
FILL_LIGHT_ROTATION = (35.0, 45.0, 0.0)
MAIN_FORWARD_SHADING_PRIORITY = 1
FILL_FORWARD_SHADING_PRIORITY = 0

CAMERA_ARM_CM = 1600.0
CAMERA_DOWN_PITCH_DEG = 38.0
CAMERA_HFOV_DEG = 60.0
CAMERA_LOCAL_UP_OFFSET_CM = 100.0
CAMERA_LOOK_AHEAD_CM = 0.0
CAMERA_PROBE_SIZE_CM = 18.0
CAMERA_YAW_PER_INPUT = 0.0
CAMERA_PITCH_PER_INPUT = 0.0

ROUTE_TILE_COUNT = 72
ROUTE_STEP_DEGREES = 360.0 / ROUTE_TILE_COUNT
ROUTE_FIRST_ANGLE_DEGREES = ROUTE_STEP_DEGREES * 0.5
ATMOSPHERE_BOTTOM_RADIUS_KM = PLANET_RADIUS_CM / 100000.0
ATMOSPHERE_HEIGHT_KM = 10.0

CREATED_THIS_RUN: list[str] = []


def info(message: str) -> None:
    unreal.log(f"[EverPlanet Starter Content] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet Starter Content] {message}")
        raise RuntimeError(message)


def object_path(obj: unreal.Object | None) -> str:
    return obj.get_path_name() if obj else "None"


def class_path(obj: unreal.Object | None) -> str:
    return obj.get_class().get_path_name() if obj else "None"


def load_class_checked(path: str) -> unreal.Class:
    loaded = unreal.load_class(None, path)
    require(loaded is not None, f"Required class could not be loaded: {path}")
    return loaded


def load_asset_checked(path: str) -> unreal.Object:
    loaded = unreal.load_asset(path)
    require(loaded is not None, f"Required asset could not be loaded: {path}")
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
    location: tuple[float, float, float],
    rotation: tuple[float, float, float],
    label: str,
) -> unreal.Actor:
    actor = actor_subsystem.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(
            roll=rotation[2],
            pitch=rotation[0],
            yaw=rotation[1],
        ),
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


def vector_length(vector: unreal.Vector) -> float:
    return math.sqrt(float(vector.x) ** 2 + float(vector.y) ** 2 + float(vector.z) ** 2)


def vector_distance(left: unreal.Vector, right: tuple[float, float, float]) -> float:
    return math.sqrt(
        (float(left.x) - right[0]) ** 2
        + (float(left.y) - right[1]) ** 2
        + (float(left.z) - right[2]) ** 2
    )


def vector_dot(left: unreal.Vector, right: tuple[float, float, float]) -> float:
    return (
        float(left.x) * right[0]
        + float(left.y) * right[1]
        + float(left.z) * right[2]
    )


def tuple_nearly_equal(
    actual: unreal.Vector,
    expected: tuple[float, float, float],
    tolerance: float,
) -> bool:
    return (
        abs(float(actual.x) - expected[0]) <= tolerance
        and abs(float(actual.y) - expected[1]) <= tolerance
        and abs(float(actual.z) - expected[2]) <= tolerance
    )


def angle_distance_degrees(actual: float, expected: float) -> float:
    return abs((actual - expected + 180.0) % 360.0 - 180.0)


def rotator_nearly_equal(
    actual: unreal.Rotator,
    expected_pitch_yaw_roll: tuple[float, float, float],
    tolerance: float,
) -> bool:
    expected_pitch, expected_yaw, expected_roll = expected_pitch_yaw_roll
    return (
        angle_distance_degrees(float(actual.pitch), expected_pitch) <= tolerance
        and angle_distance_degrees(float(actual.yaw), expected_yaw) <= tolerance
        and angle_distance_degrees(float(actual.roll), expected_roll) <= tolerance
    )


def surface_frame(angle_degrees: float) -> dict[str, tuple[float, float, float]]:
    radians = math.radians(angle_degrees)
    normal = (math.sin(radians), 0.0, math.cos(radians))
    forward = (math.cos(radians), 0.0, -math.sin(radians))
    return {
        "normal": normal,
        "forward": forward,
        "right": (0.0, 1.0, 0.0),
        "rotation": (-angle_degrees, 0.0, 0.0),
    }


def local_surface_location(
    angle_degrees: float,
    forward_offset: float,
    right_offset: float,
    up_offset: float,
) -> tuple[float, float, float]:
    frame = surface_frame(angle_degrees)
    normal = frame["normal"]
    forward = frame["forward"]
    right = frame["right"]
    radial_distance = PLANET_RADIUS_CM + up_offset
    return (
        normal[0] * radial_distance + forward[0] * forward_offset + right[0] * right_offset,
        normal[1] * radial_distance + forward[1] * forward_offset + right[1] * right_offset,
        normal[2] * radial_distance + forward[2] * forward_offset + right[2] * right_offset,
    )


def static_spec(
    label: str,
    angle_degrees: float,
    forward_offset: float,
    right_offset: float,
    up_offset: float,
    scale: tuple[float, float, float],
    material_path: str,
    collision_profile: str,
) -> dict[str, object]:
    frame = surface_frame(angle_degrees)
    return {
        "label": label,
        "mesh_path": CUBE_MESH,
        "material_path": material_path,
        "location": local_surface_location(
            angle_degrees,
            forward_offset,
            right_offset,
            up_offset,
        ),
        "rotation": frame["rotation"],
        "scale": scale,
        "collision_profile": collision_profile,
        "forward": frame["forward"],
        "up": frame["normal"],
    }


def build_static_specs() -> list[dict[str, object]]:
    specs: list[dict[str, object]] = []

    for index in range(ROUTE_TILE_COUNT):
        angle = ROUTE_FIRST_ANGLE_DEGREES + ROUTE_STEP_DEGREES * index
        specs.append(
            static_spec(
                f"RouteTile_{index:03d}",
                angle,
                0.0,
                0.0,
                8.0,
                (8.0, 2.4, 0.08),
                ROUTE_MATERIAL,
                "NoCollision",
            )
        )

    for row, forward_offset in enumerate((-500.0, 0.0, 500.0)):
        for column, right_offset in enumerate((-500.0, 0.0, 500.0)):
            surface_z = math.sqrt(
                max(
                    0.0,
                    PLANET_RADIUS_CM**2 - forward_offset**2 - right_offset**2,
                )
            )
            specs.append(
                {
                    "label": f"NorthPlaza_R{row}_C{column}",
                    "mesh_path": CUBE_MESH,
                    "material_path": PLAZA_MATERIAL,
                    "location": (forward_offset, right_offset, surface_z + 8.0),
                    "rotation": (0.0, 0.0, 0.0),
                    "scale": (4.8, 4.8, 0.08),
                    "collision_profile": "NoCollision",
                    "forward": (1.0, 0.0, 0.0),
                    "up": (0.0, 0.0, 1.0),
                }
            )

    gate_contracts = (
        ("NorthGate", 0.0, 900.0, 900.0, LANDMARK_MATERIAL_A),
        ("EastGate", 90.0, 700.0, 1200.0, LANDMARK_MATERIAL_B),
        ("SouthGate", 180.0, 1100.0, 800.0, LANDMARK_MATERIAL_A),
        ("WestGate", 270.0, 600.0, 1400.0, LANDMARK_MATERIAL_B),
    )
    for prefix, angle, width, height, material_path in gate_contracts:
        for side_name, side_offset in (("Left", -width * 0.5), ("Right", width * 0.5)):
            specs.append(
                static_spec(
                    f"{prefix}_{side_name}",
                    angle,
                    0.0,
                    side_offset,
                    height * 0.5,
                    (1.2, 1.2, height / 100.0),
                    material_path,
                    "BlockAll",
                )
            )
        specs.append(
            static_spec(
                f"{prefix}_Top",
                angle,
                0.0,
                0.0,
                height,
                (1.2, (width + 120.0) / 100.0, 1.2),
                material_path,
                "BlockAll",
            )
        )

    specs.append(
        static_spec(
            "JumpBarrier_0",
            45.0,
            0.0,
            0.0,
            45.0,
            (1.0, 6.0, 0.9),
            LANDMARK_MATERIAL_A,
            "BlockAll",
        )
    )
    return specs


STATIC_SPECS = build_static_specs()
STATIC_LABELS = {str(spec["label"]) for spec in STATIC_SPECS}
BASE_LABELS = {
    PLANET_LABEL,
    PLAYER_START_LABEL,
    MAIN_LIGHT_LABEL,
    FILL_LIGHT_LABEL,
    SKY_LIGHT_LABEL,
    SKY_ATMOSPHERE_LABEL,
}
EXPECTED_LABELS = BASE_LABELS | STATIC_LABELS


def audit_existing_directional_light_contract(
    by_label: dict[str, unreal.Actor],
) -> None:
    """Reject a pre-existing map unless both product lights are exact."""

    directional_labels = {
        label
        for label, actor in by_label.items()
        if class_path(actor) == DIRECTIONAL_LIGHT_CLASS
    }
    require(
        directional_labels == {MAIN_LIGHT_LABEL, FILL_LIGHT_LABEL},
        "Unexpected DirectionalLight actor set before mutation: "
        f"{sorted(directional_labels)}",
    )

    main_light = by_label[MAIN_LIGHT_LABEL]
    fill_light = by_label[FILL_LIGHT_LABEL]
    main_component = main_light.get_editor_property("directional_light_component")
    fill_component = fill_light.get_editor_property("directional_light_component")
    require(
        main_component is not None and fill_component is not None,
        "Existing product Map DirectionalLight component missing",
    )
    require(
        rotator_nearly_equal(main_light.get_actor_rotation(), MAIN_LIGHT_ROTATION, 1.0e-3),
        "Unexpected Main DirectionalLight rotation before mutation",
    )
    require(
        rotator_nearly_equal(fill_light.get_actor_rotation(), FILL_LIGHT_ROTATION, 1.0e-3),
        "Unexpected Fill DirectionalLight rotation before mutation",
    )
    require(
        main_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE
        and fill_component.get_editor_property("mobility")
        == unreal.ComponentMobility.MOVABLE,
        "Existing product Map DirectionalLight mobility mismatch before mutation",
    )
    require(
        math.isclose(
            float(main_component.get_editor_property("intensity")),
            5.0,
            abs_tol=1.0e-4,
        )
        and math.isclose(
            float(fill_component.get_editor_property("intensity")),
            1.5,
            abs_tol=1.0e-4,
        ),
        "Existing product Map DirectionalLight intensity mismatch before mutation",
    )
    require(
        bool(main_component.get_editor_property("atmosphere_sun_light"))
        and not bool(fill_component.get_editor_property("atmosphere_sun_light")),
        "Existing product Map DirectionalLight atmosphere role mismatch before mutation",
    )
    main_priority = int(
        main_component.get_editor_property("forward_shading_priority")
    )
    fill_priority = int(
        fill_component.get_editor_property("forward_shading_priority")
    )
    require(
        main_priority == MAIN_FORWARD_SHADING_PRIORITY
        and fill_priority == FILL_FORWARD_SHADING_PRIORITY,
        "Existing product Map DirectionalLight priority mismatch before mutation: "
        f"main={main_priority}, fill={fill_priority}. Run the dedicated one-shot patch first.",
    )


def preflight_sources(asset_subsystem: unreal.EditorAssetSubsystem) -> None:
    required_assets = (
        SOURCE_CHARACTER,
        SOURCE_GAME_MODE,
        CUBE_MESH,
        SPHERE_MESH,
        PLANET_MATERIAL,
        ROUTE_MATERIAL,
        PLAZA_MATERIAL,
        LANDMARK_MATERIAL_A,
        LANDMARK_MATERIAL_B,
    )
    for asset_path in required_assets:
        require(
            asset_subsystem.does_asset_exist(asset_path),
            f"Required source asset is missing: {asset_path}",
        )
        load_asset_checked(asset_path)

    for class_name in (
        GLOBE_CHARACTER_CLASS,
        GLOBE_CAMERA_COMPONENT_CLASS,
        PLANET_GRAVITY_COMPONENT_CLASS,
        PLANET_BODY_CLASS,
        PLAYER_START_CLASS,
        STATIC_MESH_ACTOR_CLASS,
        DIRECTIONAL_LIGHT_CLASS,
        SKY_LIGHT_CLASS,
        SKY_ATMOSPHERE_CLASS,
    ):
        load_class_checked(class_name)

    require(
        "/Tests/" not in TARGET_CHARACTER
        and "/Tests/" not in TARGET_GAME_MODE
        and "/Tests/" not in TARGET_MAP,
        "Product target paths must never live below /Tests/",
    )


def target_state(asset_subsystem: unreal.EditorAssetSubsystem) -> tuple[bool, bool, bool]:
    return (
        asset_subsystem.does_asset_exist(TARGET_CHARACTER),
        asset_subsystem.does_asset_exist(TARGET_GAME_MODE),
        asset_subsystem.does_asset_exist(TARGET_MAP),
    )


def preflight_existing_targets(
    asset_subsystem: unreal.EditorAssetSubsystem,
) -> tuple[bool, bool, bool]:
    """Reject inconsistent existing product targets before any mutation."""

    character_exists, game_mode_exists, map_exists = target_state(asset_subsystem)
    require(
        not game_mode_exists or character_exists,
        "Existing product GameMode has no matching product Character; refusing partial repair",
    )
    require(
        not map_exists or (character_exists and game_mode_exists),
        "Existing product Map is missing one or both Blueprint dependencies; refusing partial repair",
    )

    if character_exists:
        character_blueprint = asset_subsystem.load_asset(TARGET_CHARACTER)
        require(
            isinstance(character_blueprint, unreal.Blueprint),
            "Existing product Character target is not a Blueprint",
        )
        actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            character_blueprint
        )
        expected_parent = load_class_checked(GLOBE_CHARACTER_CLASS)
        require(
            actual_parent == expected_parent,
            "Unexpected existing product Character parent before mutation: "
            f"{object_path(actual_parent)}",
        )

    if game_mode_exists:
        game_mode_blueprint = asset_subsystem.load_asset(TARGET_GAME_MODE)
        source_blueprint = asset_subsystem.load_asset(SOURCE_GAME_MODE)
        require(
            isinstance(game_mode_blueprint, unreal.Blueprint)
            and isinstance(source_blueprint, unreal.Blueprint),
            "Existing product or source GameMode is not a Blueprint",
        )
        actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            game_mode_blueprint
        )
        expected_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            source_blueprint
        )
        require(
            actual_parent == expected_parent,
            "Unexpected existing product GameMode parent before mutation: "
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
            "Editor subsystems unavailable during existing product Map preflight",
        )
        require(
            level_subsystem.load_level(TARGET_MAP),
            "Existing product Map could not be loaded during preflight",
        )
        actors = actor_subsystem.get_all_level_actors()
        by_label = {actor.get_actor_label(): actor for actor in actors}
        require(
            len(actors) == len(EXPECTED_LABELS)
            and len(by_label) == len(actors)
            and set(by_label) == EXPECTED_LABELS,
            "Unexpected existing product Map actor set before mutation: "
            f"count={len(actors)}, labels={sorted(by_label)}",
        )
        audit_existing_directional_light_contract(by_label)
        world = editor_subsystem.get_editor_world()
        require(world is not None, "Existing product Map has no Editor World")
        game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
        require(
            world.get_world_settings().get_editor_property("default_game_mode")
            == game_mode_class,
            "Unexpected existing product Map GameMode before mutation",
        )

    state_name = {
        (False, False, False): "fresh",
        (True, False, False): "resume-after-character",
        (True, True, False): "resume-before-map",
        (True, True, True): "verify-only",
    }.get((character_exists, game_mode_exists, map_exists))
    require(state_name is not None, "Unsupported product target dependency state")
    info(
        "STARTER_PREFLIGHT="
        f"character={'existing' if character_exists else 'missing'}, "
        f"gameMode={'existing' if game_mode_exists else 'missing'}, "
        f"map={'existing' if map_exists else 'missing'}, state={state_name}"
    )
    return character_exists, game_mode_exists, map_exists


def create_or_load_character(
    asset_subsystem: unreal.EditorAssetSubsystem,
) -> tuple[unreal.Blueprint, bool]:
    if asset_subsystem.does_asset_exist(TARGET_CHARACTER):
        loaded = asset_subsystem.load_asset(TARGET_CHARACTER)
        require(isinstance(loaded, unreal.Blueprint), "Product Character is not a Blueprint")
        return loaded, False

    info(f"Duplicating Character Blueprint: {SOURCE_CHARACTER} -> {TARGET_CHARACTER}")
    duplicated = asset_subsystem.duplicate_asset(SOURCE_CHARACTER, TARGET_CHARACTER)
    require(isinstance(duplicated, unreal.Blueprint), "Product Character duplication failed")
    CREATED_THIS_RUN.append(TARGET_CHARACTER)

    expected_parent = load_class_checked(GLOBE_CHARACTER_CLASS)
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicated, expected_parent)
    compile_blueprint_checked(duplicated, TARGET_CHARACTER)

    character_class = load_blueprint_class_checked(TARGET_CHARACTER)
    defaults = unreal.get_default_object(character_class)
    require(defaults is not None, "Product Character defaults unavailable")
    camera_rig = defaults.get_editor_property("planet_globe_camera_component")
    require(camera_rig is not None, "Product Character globe camera component unavailable")
    defaults.modify()
    camera_rig.modify()
    camera_policy = (
        ("camera_arm_length", CAMERA_ARM_CM),
        ("camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
        ("camera_horizontal_fov_degrees", CAMERA_HFOV_DEG),
        ("camera_local_up_offset", CAMERA_LOCAL_UP_OFFSET_CM),
        ("camera_look_ahead_distance", CAMERA_LOOK_AHEAD_CM),
        ("camera_collision_probe_size", CAMERA_PROBE_SIZE_CM),
        ("camera_yaw_degrees_per_input_unit", CAMERA_YAW_PER_INPUT),
        ("camera_pitch_degrees_per_input_unit", CAMERA_PITCH_PER_INPUT),
        ("minimum_camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
        ("maximum_camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
    )
    for property_name, value in camera_policy:
        camera_rig.set_editor_property(property_name, value)

    require(
        asset_subsystem.save_asset(TARGET_CHARACTER, False),
        "Product Character save failed",
    )
    return duplicated, True


def audit_character(
    asset_subsystem: unreal.EditorAssetSubsystem,
    blueprint: unreal.Blueprint,
    compile_created: bool,
) -> unreal.Class:
    expected_parent = load_class_checked(GLOBE_CHARACTER_CLASS)
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(
        actual_parent == expected_parent,
        f"Unexpected product Character parent: {object_path(actual_parent)}",
    )
    require(
        "/Tests/" not in object_path(actual_parent),
        "Product Character uses a test-only parent",
    )
    if compile_created:
        compile_blueprint_checked(blueprint, TARGET_CHARACTER)
        require(
            asset_subsystem.save_asset(TARGET_CHARACTER, False),
            "Product Character post-compile save failed",
        )

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
    require(all(components.values()), "Product Character components are incomplete")
    require(
        class_path(components["planet_gravity_component"]) == PLANET_GRAVITY_COMPONENT_CLASS,
        "Product Character gravity component class mismatch",
    )
    require(
        class_path(components["planet_globe_camera_component"]) == GLOBE_CAMERA_COMPONENT_CLASS,
        "Product Character globe camera component class mismatch",
    )

    source_mesh = source_defaults.get_editor_property("mesh")
    require(source_mesh is not None, "Source mesh component unavailable")
    require(
        components["mesh"].get_editor_property("skeletal_mesh_asset")
        == source_mesh.get_editor_property("skeletal_mesh_asset"),
        "Product Character skeletal mesh no longer matches the Third Person source",
    )
    require(
        components["mesh"].get_editor_property("anim_class")
        == source_mesh.get_editor_property("anim_class"),
        "Product Character animation class no longer matches the Third Person source",
    )
    for action_name in ("jump_action", "move_action", "look_action", "mouse_look_action"):
        require(
            defaults.get_editor_property(action_name)
            == source_defaults.get_editor_property(action_name),
            f"Product Character Input Action mismatch: {action_name}",
        )

    camera_rig = components["planet_globe_camera_component"]
    camera_contracts = (
        ("camera_arm_length", CAMERA_ARM_CM),
        ("camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
        ("camera_horizontal_fov_degrees", CAMERA_HFOV_DEG),
        ("camera_local_up_offset", CAMERA_LOCAL_UP_OFFSET_CM),
        ("camera_look_ahead_distance", CAMERA_LOOK_AHEAD_CM),
        ("camera_collision_probe_size", CAMERA_PROBE_SIZE_CM),
        ("camera_yaw_degrees_per_input_unit", CAMERA_YAW_PER_INPUT),
        ("camera_pitch_degrees_per_input_unit", CAMERA_PITCH_PER_INPUT),
        ("minimum_camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
        ("maximum_camera_down_pitch_degrees", CAMERA_DOWN_PITCH_DEG),
    )
    for property_name, expected in camera_contracts:
        actual = float(camera_rig.get_editor_property(property_name))
        if property_name in {
            "camera_yaw_degrees_per_input_unit",
            "camera_pitch_degrees_per_input_unit",
        }:
            require(
                actual == expected,
                "Product Character camera input scale mismatch "
                f"{property_name}: actual={actual}, expected exact={expected}",
            )
            continue
        require(
            math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-4),
            "Product Character camera mismatch "
            f"{property_name}: actual={actual}, expected={expected}",
        )

    info(
        "STARTER_PLAYER_AUDIT="
        f"parent={object_path(actual_parent)}, rig={class_path(camera_rig)}, "
        f"arm={CAMERA_ARM_CM:.1f}, pitch={CAMERA_DOWN_PITCH_DEG:.1f}, "
        f"hfov={CAMERA_HFOV_DEG:.1f}, yawInput={CAMERA_YAW_PER_INPUT:.1f}, "
        f"pitchInput={CAMERA_PITCH_PER_INPUT:.1f}, pitchRange=38..38"
    )
    return character_class


def create_or_load_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    character_class: unreal.Class,
) -> tuple[unreal.Blueprint, bool]:
    if asset_subsystem.does_asset_exist(TARGET_GAME_MODE):
        loaded = asset_subsystem.load_asset(TARGET_GAME_MODE)
        require(isinstance(loaded, unreal.Blueprint), "Product GameMode is not a Blueprint")
        return loaded, False

    info(f"Duplicating GameMode Blueprint: {SOURCE_GAME_MODE} -> {TARGET_GAME_MODE}")
    duplicated = asset_subsystem.duplicate_asset(SOURCE_GAME_MODE, TARGET_GAME_MODE)
    require(isinstance(duplicated, unreal.Blueprint), "Product GameMode duplication failed")
    CREATED_THIS_RUN.append(TARGET_GAME_MODE)
    compile_blueprint_checked(duplicated, TARGET_GAME_MODE)

    game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
    defaults = unreal.get_default_object(game_mode_class)
    require(defaults is not None, "Product GameMode defaults unavailable")
    defaults.modify()
    defaults.set_editor_property("default_pawn_class", character_class)
    require(
        asset_subsystem.save_asset(TARGET_GAME_MODE, False),
        "Product GameMode save failed",
    )
    return duplicated, True


def audit_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    blueprint: unreal.Blueprint,
    character_class: unreal.Class,
    compile_created: bool,
) -> unreal.Class:
    source_blueprint = asset_subsystem.load_asset(SOURCE_GAME_MODE)
    require(isinstance(source_blueprint, unreal.Blueprint), "Source GameMode is not a Blueprint")
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    expected_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(source_blueprint)
    require(
        actual_parent == expected_parent,
        "Product GameMode parent mismatch: "
        f"actual={object_path(actual_parent)}, expected={object_path(expected_parent)}",
    )
    if compile_created:
        compile_blueprint_checked(blueprint, TARGET_GAME_MODE)
        require(
            asset_subsystem.save_asset(TARGET_GAME_MODE, False),
            "Product GameMode post-compile save failed",
        )

    game_mode_class = load_blueprint_class_checked(TARGET_GAME_MODE)
    defaults = unreal.get_default_object(game_mode_class)
    source_class = load_blueprint_class_checked(SOURCE_GAME_MODE)
    source_defaults = unreal.get_default_object(source_class)
    require(defaults is not None and source_defaults is not None, "GameMode defaults unavailable")
    require(
        defaults.get_editor_property("default_pawn_class") == character_class,
        "Product GameMode DefaultPawnClass mismatch",
    )
    require(
        defaults.get_editor_property("player_controller_class")
        == source_defaults.get_editor_property("player_controller_class"),
        "Product GameMode PlayerController no longer matches the Third Person source",
    )

    player_controller_class = defaults.get_editor_property("player_controller_class")
    player_controller_defaults = unreal.get_default_object(player_controller_class)
    require(player_controller_defaults is not None, "Product PlayerController defaults unavailable")
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
        f"Product PlayerController is missing {DEFAULT_MAPPING_CONTEXT}",
    )
    require(
        MOUSE_LOOK_MAPPING_CONTEXT in mobile_excluded_mapping_contexts,
        f"Product PlayerController is missing {MOUSE_LOOK_MAPPING_CONTEXT}",
    )

    info(
        "STARTER_GAMEMODE_AUDIT="
        f"parent={object_path(actual_parent)}, "
        f"pawn={object_path(defaults.get_editor_property('default_pawn_class'))}, "
        f"controller={object_path(player_controller_class)}"
    )
    return game_mode_class


def spawn_static_mesh_actor(
    actor_subsystem: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    spec: dict[str, object],
    mesh_assets: dict[str, unreal.Object],
    material_assets: dict[str, unreal.Object],
) -> unreal.Actor:
    label = str(spec["label"])
    actor = spawn_actor(
        actor_subsystem,
        actor_class,
        spec["location"],
        spec["rotation"],
        label,
    )
    component = actor.get_editor_property("static_mesh_component")
    require(component is not None, f"Static mesh component unavailable: {label}")
    require(
        bool(component.set_static_mesh(mesh_assets[str(spec["mesh_path"])])),
        f"Static mesh assignment failed: {label}",
    )
    component.set_material(0, material_assets[str(spec["material_path"])])
    component.set_collision_profile_name(str(spec["collision_profile"]))
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(unreal.Vector(*spec["scale"]))
    return actor


def create_map(character_class: unreal.Class, game_mode_class: unreal.Class) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(level_subsystem is not None, "LevelEditorSubsystem unavailable")
    require(actor_subsystem is not None, "EditorActorSubsystem unavailable")
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    require(
        not unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP),
        "Refusing to overwrite existing product Map",
    )
    require(
        level_subsystem.new_level(TARGET_MAP, False),
        "Product non-partitioned Map creation failed",
    )
    CREATED_THIS_RUN.append(TARGET_MAP)

    world = editor_subsystem.get_editor_world()
    require(world is not None, "Product Editor World unavailable")
    world_settings = world.get_world_settings()
    require(world_settings is not None, "Product WorldSettings unavailable")
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    world_settings.set_editor_property("kill_z", KILL_Z_CM)

    planet = spawn_actor(
        actor_subsystem,
        load_class_checked(PLANET_BODY_CLASS),
        (0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0),
        PLANET_LABEL,
    )
    planet.set_editor_property("surface_radius", PLANET_RADIUS_CM)
    planet_mesh = planet.get_editor_property("planet_mesh")
    require(planet_mesh is not None, "Planet mesh component unavailable")
    planet_mesh.set_material(0, load_asset_checked(PLANET_MATERIAL))

    character_defaults = unreal.get_default_object(character_class)
    capsule = character_defaults.get_editor_property("capsule_component")
    require(capsule is not None, "Product Character capsule unavailable")
    capsule_half_height = float(capsule.get_scaled_capsule_half_height())
    spawn_distance = PLANET_RADIUS_CM + capsule_half_height + SPAWN_CLEARANCE_CM
    spawn_actor(
        actor_subsystem,
        load_class_checked(PLAYER_START_CLASS),
        (0.0, 0.0, spawn_distance),
        PLAYER_START_ROTATION,
        PLAYER_START_LABEL,
    )

    main_light = spawn_actor(
        actor_subsystem,
        load_class_checked(DIRECTIONAL_LIGHT_CLASS),
        (0.0, 0.0, spawn_distance + 1000.0),
        MAIN_LIGHT_ROTATION,
        MAIN_LIGHT_LABEL,
    )
    main_component = set_component_movable(main_light, "directional_light_component")
    main_component.set_editor_property("intensity", 5.0)
    main_component.set_editor_property("atmosphere_sun_light", True)
    main_component.set_editor_property(
        "forward_shading_priority",
        MAIN_FORWARD_SHADING_PRIORITY,
    )

    fill_light = spawn_actor(
        actor_subsystem,
        load_class_checked(DIRECTIONAL_LIGHT_CLASS),
        (0.0, 0.0, -spawn_distance - 1000.0),
        FILL_LIGHT_ROTATION,
        FILL_LIGHT_LABEL,
    )
    fill_component = set_component_movable(fill_light, "directional_light_component")
    fill_component.set_editor_property("intensity", 1.5)
    fill_component.set_editor_property("atmosphere_sun_light", False)
    fill_component.set_editor_property(
        "forward_shading_priority",
        FILL_FORWARD_SHADING_PRIORITY,
    )

    sky_light = spawn_actor(
        actor_subsystem,
        load_class_checked(SKY_LIGHT_CLASS),
        (0.0, 0.0, spawn_distance + 500.0),
        (0.0, 0.0, 0.0),
        SKY_LIGHT_LABEL,
    )
    sky_component = set_component_movable(sky_light, "light_component")
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_editor_property("intensity", 1.5)

    sky_atmosphere = spawn_actor(
        actor_subsystem,
        load_class_checked(SKY_ATMOSPHERE_CLASS),
        (0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0),
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

    mesh_assets = {CUBE_MESH: load_asset_checked(CUBE_MESH)}
    material_assets = {
        path: load_asset_checked(path)
        for path in {
            ROUTE_MATERIAL,
            PLAZA_MATERIAL,
            LANDMARK_MATERIAL_A,
            LANDMARK_MATERIAL_B,
        }
    }
    static_mesh_actor_class = load_class_checked(STATIC_MESH_ACTOR_CLASS)
    for spec in STATIC_SPECS:
        spawn_static_mesh_actor(
            actor_subsystem,
            static_mesh_actor_class,
            spec,
            mesh_assets,
            material_assets,
        )

    require(level_subsystem.save_current_level(), "Product Map save failed")
    info(
        "STARTER_MAP_CREATED="
        f"actors={len(EXPECTED_LABELS)}, radius={PLANET_RADIUS_CM:.1f}cm, "
        f"routeTiles={ROUTE_TILE_COUNT}, landmarks=4, obstacle=1, "
        f"spawnDistance={spawn_distance:.1f}cm"
    )


def audit_static_actor(
    actor: unreal.Actor,
    spec: dict[str, object],
    mesh_assets: dict[str, unreal.Object],
    material_assets: dict[str, unreal.Object],
) -> None:
    label = str(spec["label"])
    require(
        class_path(actor) == STATIC_MESH_ACTOR_CLASS,
        f"Static actor class mismatch {label}: {class_path(actor)}",
    )
    require(
        vector_distance(actor.get_actor_location(), spec["location"]) <= 0.1,
        f"Static actor location mismatch: {label}",
    )
    require(
        tuple_nearly_equal(actor.get_actor_scale3d(), spec["scale"], 1.0e-4),
        f"Static actor scale mismatch: {label}",
    )
    require(
        vector_dot(actor.get_actor_forward_vector(), spec["forward"]) >= 0.99999,
        f"Static actor forward basis mismatch: {label}",
    )
    require(
        vector_dot(actor.get_actor_up_vector(), spec["up"]) >= 0.99999,
        f"Static actor up basis mismatch: {label}",
    )

    component = actor.get_editor_property("static_mesh_component")
    require(component is not None, f"Static mesh component unavailable: {label}")
    require(
        component.get_editor_property("static_mesh") == mesh_assets[str(spec["mesh_path"])],
        f"Static mesh mismatch: {label}",
    )
    require(
        component.get_material(0) == material_assets[str(spec["material_path"])],
        f"Static material mismatch: {label}",
    )
    require(
        str(component.get_collision_profile_name()) == str(spec["collision_profile"]),
        f"Static collision profile mismatch: {label}",
    )
    require(
        component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC,
        f"Static mobility mismatch: {label}",
    )


def audit_map(character_class: unreal.Class, game_mode_class: unreal.Class) -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(level_subsystem is not None, "LevelEditorSubsystem unavailable during audit")
    require(actor_subsystem is not None, "EditorActorSubsystem unavailable during audit")
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable during audit")
    require(level_subsystem.load_level(TARGET_MAP), "Product Map could not be loaded")
    world = editor_subsystem.get_editor_world()
    require(world is not None, "Product Editor World unavailable during audit")
    world_settings = world.get_world_settings()
    require(
        world_settings.get_editor_property("default_game_mode") == game_mode_class,
        "Product Map GameMode override mismatch",
    )
    require(
        math.isclose(
            float(world_settings.get_editor_property("kill_z")),
            KILL_Z_CM,
            rel_tol=0.0,
            abs_tol=0.1,
        ),
        "Product Map KillZ mismatch",
    )

    actors = actor_subsystem.get_all_level_actors()
    by_label = {actor.get_actor_label(): actor for actor in actors}
    require(len(by_label) == len(actors), "Product Map actor labels are not unique")
    require(
        len(actors) == len(EXPECTED_LABELS),
        f"Product Map actor count mismatch: {len(actors)} != {len(EXPECTED_LABELS)}",
    )
    require(
        set(by_label) == EXPECTED_LABELS,
        f"Product Map labels mismatch: {sorted(set(by_label) ^ EXPECTED_LABELS)}",
    )

    expected_base_classes = {
        PLANET_LABEL: PLANET_BODY_CLASS,
        PLAYER_START_LABEL: PLAYER_START_CLASS,
        MAIN_LIGHT_LABEL: DIRECTIONAL_LIGHT_CLASS,
        FILL_LIGHT_LABEL: DIRECTIONAL_LIGHT_CLASS,
        SKY_LIGHT_LABEL: SKY_LIGHT_CLASS,
        SKY_ATMOSPHERE_LABEL: SKY_ATMOSPHERE_CLASS,
    }
    for label, expected_class in expected_base_classes.items():
        require(
            class_path(by_label[label]) == expected_class,
            f"Product base actor class mismatch {label}: {class_path(by_label[label])}",
        )

    planet = by_label[PLANET_LABEL]
    require(vector_length(planet.get_actor_location()) <= 0.1, "Planet is not at world origin")
    require(
        tuple_nearly_equal(planet.get_actor_scale3d(), (1.0, 1.0, 1.0), 1.0e-4),
        f"Product Planet actor scale mismatch: {planet.get_actor_scale3d()}",
    )
    require(
        math.isclose(float(planet.get_surface_radius()), PLANET_RADIUS_CM, abs_tol=1.0e-4),
        "Product Planet radius mismatch",
    )
    planet_mesh = planet.get_editor_property("planet_mesh")
    require(planet_mesh is not None, "Product Planet mesh unavailable")
    require(
        planet_mesh.get_editor_property("static_mesh") == load_asset_checked(SPHERE_MESH),
        "Product Planet static mesh mismatch",
    )
    expected_planet_mesh_scale = PLANET_RADIUS_CM / ENGINE_SPHERE_RADIUS_CM
    require(
        tuple_nearly_equal(
            planet_mesh.get_editor_property("relative_scale3d"),
            (
                expected_planet_mesh_scale,
                expected_planet_mesh_scale,
                expected_planet_mesh_scale,
            ),
            1.0e-4,
        ),
        "Product Planet mesh relative scale mismatch",
    )
    require(
        planet_mesh.get_editor_property("mobility") == unreal.ComponentMobility.STATIC,
        "Product Planet mesh is not Static",
    )
    require(
        str(planet_mesh.get_collision_profile_name()) == "BlockAll",
        "Product Planet collision profile is not BlockAll",
    )
    require(
        planet_mesh.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS,
        "Product Planet collision is not QueryAndPhysics",
    )
    require(
        not bool(planet_mesh.get_editor_property("generate_overlap_events")),
        "Product Planet overlap events must be disabled",
    )
    require(
        planet_mesh.get_material(0) == load_asset_checked(PLANET_MATERIAL),
        "Product Planet material mismatch",
    )

    defaults = unreal.get_default_object(character_class)
    capsule = defaults.get_editor_property("capsule_component")
    expected_spawn_distance = (
        PLANET_RADIUS_CM
        + float(capsule.get_scaled_capsule_half_height())
        + SPAWN_CLEARANCE_CM
    )
    player_start_location = by_label[PLAYER_START_LABEL].get_actor_location()
    require(
        vector_distance(
            player_start_location,
            (0.0, 0.0, expected_spawn_distance),
        )
        <= 0.1,
        f"Product PlayerStart mismatch: {player_start_location}",
    )
    require(
        rotator_nearly_equal(
            by_label[PLAYER_START_LABEL].get_actor_rotation(),
            PLAYER_START_ROTATION,
            1.0e-3,
        ),
        "Product PlayerStart rotation mismatch",
    )

    main_component = by_label[MAIN_LIGHT_LABEL].get_editor_property(
        "directional_light_component"
    )
    fill_component = by_label[FILL_LIGHT_LABEL].get_editor_property(
        "directional_light_component"
    )
    require(main_component is not None and fill_component is not None, "Light component missing")
    require(
        rotator_nearly_equal(
            by_label[MAIN_LIGHT_LABEL].get_actor_rotation(),
            MAIN_LIGHT_ROTATION,
            1.0e-3,
        ),
        "Main DirectionalLight rotation mismatch",
    )
    require(
        rotator_nearly_equal(
            by_label[FILL_LIGHT_LABEL].get_actor_rotation(),
            FILL_LIGHT_ROTATION,
            1.0e-3,
        ),
        "Fill DirectionalLight rotation mismatch",
    )
    require(
        main_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        "Main DirectionalLight is not Movable",
    )
    require(
        fill_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        "Fill DirectionalLight is not Movable",
    )
    require(
        math.isclose(float(main_component.get_editor_property("intensity")), 5.0, abs_tol=1.0e-4),
        "Main DirectionalLight intensity mismatch",
    )
    require(
        math.isclose(float(fill_component.get_editor_property("intensity")), 1.5, abs_tol=1.0e-4),
        "Fill DirectionalLight intensity mismatch",
    )
    require(
        bool(main_component.get_editor_property("atmosphere_sun_light")),
        "Main DirectionalLight is not the atmosphere sun",
    )
    require(
        not bool(fill_component.get_editor_property("atmosphere_sun_light")),
        "Fill DirectionalLight must not become a second atmosphere sun",
    )
    main_priority = int(
        main_component.get_editor_property("forward_shading_priority")
    )
    fill_priority = int(
        fill_component.get_editor_property("forward_shading_priority")
    )
    require(
        main_priority == MAIN_FORWARD_SHADING_PRIORITY,
        f"Main DirectionalLight forward shading priority mismatch: {main_priority}",
    )
    require(
        fill_priority == FILL_FORWARD_SHADING_PRIORITY,
        f"Fill DirectionalLight forward shading priority mismatch: {fill_priority}",
    )

    sky_component = by_label[SKY_LIGHT_LABEL].get_editor_property("light_component")
    require(sky_component is not None, "SkyLight component unavailable")
    require(
        sky_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        "SkyLight is not Movable",
    )
    require(
        bool(sky_component.get_editor_property("real_time_capture")),
        "SkyLight real-time capture is disabled",
    )
    require(
        math.isclose(
            float(sky_component.get_editor_property("intensity")),
            1.5,
            abs_tol=1.0e-4,
        ),
        "SkyLight intensity mismatch",
    )

    atmosphere_actor = by_label[SKY_ATMOSPHERE_LABEL]
    atmosphere_component = atmosphere_actor.get_editor_property("sky_atmosphere_component")
    require(atmosphere_component is not None, "SkyAtmosphere component unavailable")
    require(
        vector_length(atmosphere_actor.get_actor_location()) <= 0.1,
        "SkyAtmosphere does not share the Planet center",
    )
    require(
        atmosphere_component.get_editor_property("transform_mode")
        == unreal.SkyAtmosphereTransformMode.PLANET_CENTER_AT_COMPONENT_TRANSFORM,
        "SkyAtmosphere transform mode mismatch",
    )
    require(
        math.isclose(
            float(atmosphere_component.get_editor_property("bottom_radius")),
            ATMOSPHERE_BOTTOM_RADIUS_KM,
            abs_tol=1.0e-4,
        ),
        "SkyAtmosphere bottom radius mismatch",
    )
    require(
        math.isclose(
            float(atmosphere_component.get_editor_property("atmosphere_height")),
            ATMOSPHERE_HEIGHT_KM,
            abs_tol=1.0e-4,
        ),
        "SkyAtmosphere height mismatch",
    )

    mesh_assets = {CUBE_MESH: load_asset_checked(CUBE_MESH)}
    material_assets = {
        path: load_asset_checked(path)
        for path in {
            ROUTE_MATERIAL,
            PLAZA_MATERIAL,
            LANDMARK_MATERIAL_A,
            LANDMARK_MATERIAL_B,
        }
    }
    for spec in STATIC_SPECS:
        audit_static_actor(
            by_label[str(spec["label"])],
            spec,
            mesh_assets,
            material_assets,
        )

    info(
        "STARTER_MAP_AUDIT="
        f"actors={len(actors)}, base={len(BASE_LABELS)}, "
        f"route={ROUTE_TILE_COUNT}, plaza=9, landmarkPieces=12, obstacles=1, "
        f"radius={PLANET_RADIUS_CM:.1f}cm, planetCollision=BlockAll/QueryAndPhysics, "
        f"forwardPriority={main_priority}/{fill_priority}"
    )


def main() -> None:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    preflight_sources(asset_subsystem)
    initial_state = preflight_existing_targets(asset_subsystem)

    asset_subsystem.make_directory(f"{TARGET_ROOT}/Characters")
    asset_subsystem.make_directory(f"{TARGET_ROOT}/GameModes")
    asset_subsystem.make_directory(f"{TARGET_ROOT}/Maps")

    character_blueprint, character_created = create_or_load_character(asset_subsystem)
    character_class = audit_character(
        asset_subsystem,
        character_blueprint,
        character_created,
    )
    game_mode_blueprint, game_mode_created = create_or_load_game_mode(
        asset_subsystem,
        character_class,
    )
    game_mode_class = audit_game_mode(
        asset_subsystem,
        game_mode_blueprint,
        character_class,
        game_mode_created,
    )

    if asset_subsystem.does_asset_exist(TARGET_MAP):
        audit_map(character_class, game_mode_class)
    else:
        create_map(character_class, game_mode_class)
        audit_map(character_class, game_mode_class)

    mode = "existing"
    if any(initial_state):
        mode = "resumed" if CREATED_THIS_RUN else "existing"
    elif CREATED_THIS_RUN:
        mode = "created"
    info(
        "STARTER_CREATED_THIS_RUN="
        + (",".join(CREATED_THIS_RUN) if CREATED_THIS_RUN else "none")
    )
    info(f"STARTER_CONTENT_RESULT=VERIFIED mode={mode}")


try:
    main()
except Exception as exc:
    unreal.log_error(
        "[EverPlanet Starter Content] "
        f"STARTER_CONTENT_RESULT=FAILED error={exc}"
    )
    unreal.log_error(
        "[EverPlanet Starter Content] "
        "STARTER_ROLLBACK_CANDIDATES="
        + (",".join(reversed(CREATED_THIS_RUN)) if CREATED_THIS_RUN else "none")
    )
    raise
