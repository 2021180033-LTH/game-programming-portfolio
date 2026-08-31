"""Create or read-only audit the first production Planet's Town/Field slice.

Run only through UnrealEditor.exe with ``-ExecutePythonScript`` while every
other Unreal process is closed.  This generator is deliberately fail-closed:

* the four targets must be either all missing or all already present;
* an existing target is never repaired, overwritten, compiled, or saved;
* the existing Product Character, Product GameMode, and exact-100 graybox are
  protected by SHA-256 before/after checks and are never mutation targets;
* a failed fresh run rolls back only assets recorded in CREATED_THIS_RUN.

Fresh targets (exactly four):

* /Game/EverPlanet/Product/World/DA_EverPlanetWorldCatalog
* /Game/EverPlanet/Product/GameModes/BP_EverPlanetRegionalGameMode
* /Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Town
* /Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Field
"""

from __future__ import annotations

import gc
import hashlib
import json
import math
import os
import re

import unreal


# Existing Product assets are read-only inputs.  Do not add them to TARGET_ASSETS.
SOURCE_CHARACTER = "/Game/EverPlanet/Product/Characters/BP_EverPlanetPlayerCharacter"
SOURCE_GAME_MODE = "/Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode"
SOURCE_GRAYBOX = "/Game/EverPlanet/Product/Maps/Lvl_FirstPlanet_Graybox"
SOURCE_CHARACTER_CLASS_PATH = (
    "/Game/EverPlanet/Product/Characters/"
    "BP_EverPlanetPlayerCharacter.BP_EverPlanetPlayerCharacter_C"
)
SOURCE_GAME_MODE_CLASS_PATH = (
    "/Game/EverPlanet/Product/GameModes/"
    "BP_EverPlanetGameMode.BP_EverPlanetGameMode_C"
)

TARGET_CATALOG = "/Game/EverPlanet/Product/World/DA_EverPlanetWorldCatalog"
TARGET_REGIONAL_GAME_MODE = (
    "/Game/EverPlanet/Product/GameModes/BP_EverPlanetRegionalGameMode"
)
TARGET_TOWN_MAP = (
    "/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Town"
)
TARGET_FIELD_MAP = (
    "/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Field"
)
TARGET_ASSETS = (
    TARGET_CATALOG,
    TARGET_REGIONAL_GAME_MODE,
    TARGET_TOWN_MAP,
    TARGET_FIELD_MAP,
)

CATALOG_CLASS_PATH = "/Script/EverPlanet.EverPlanetWorldCatalog"
REGION_DESCRIPTOR_CLASS_PATH = "/Script/EverPlanet.EverPlanetRegionDescriptor"
REGION_PLAYER_START_CLASS_PATH = "/Script/EverPlanet.EverPlanetRegionPlayerStart"
REGION_PORTAL_CLASS_PATH = "/Script/EverPlanet.EverPlanetRegionPortal"
PLANET_BODY_CLASS_PATH = "/Script/EverPlanet.PlanetBody"
STATIC_MESH_ACTOR_CLASS_PATH = "/Script/Engine.StaticMeshActor"
GENERIC_PLAYER_START_CLASS_PATH = "/Script/Engine.PlayerStart"
DIRECTIONAL_LIGHT_CLASS_PATH = "/Script/Engine.DirectionalLight"
SKY_LIGHT_CLASS_PATH = "/Script/Engine.SkyLight"
SKY_ATMOSPHERE_CLASS_PATH = "/Script/Engine.SkyAtmosphere"
WORLD_CLASS_PATH = "/Script/Engine.World"
NATIVE_GAME_MODE_CLASS_PATH = "/Script/EverPlanet.EverPlanetGameMode"
GLOBE_CAMERA_COMPONENT_CLASS_PATH = "/Script/EverPlanet.PlanetGlobeCameraComponent"

STRUCT_WRAPPER_NAMES = (
    "EverPlanetWorldAddress",
    "EverPlanetRegionKey",
    "EverPlanetPlanetDefinition",
    "EverPlanetRegionDefinition",
    "EverPlanetTravelLinkDefinition",
)

CUBE_MESH = "/Engine/BasicShapes/Cube"
SPHERE_MESH = "/Engine/BasicShapes/Sphere"
PLANET_MATERIAL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02"
ROUTE_MATERIAL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark"
PLAZA_MATERIAL = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray"
LANDMARK_MATERIAL_A = "/Game/LevelPrototyping/Materials/MI_DefaultColorway"
LANDMARK_MATERIAL_B = (
    "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_Round"
)

PLANET_ID = "planet.first"
TOWN_REGION_ID = "region.first.town"
FIELD_REGION_ID = "region.first.field"
TOWN_START_ANCHOR = "spawn.town.start"
TOWN_FROM_FIELD_ANCHOR = "spawn.town.from_field"
FIELD_FROM_TOWN_ANCHOR = "spawn.field.from_town"
TOWN_TO_FIELD_LINK = "link.first.town_to_field"
FIELD_TO_TOWN_LINK = "link.first.field_to_town"
TOWN_PORTAL_ID = "portal.town.east_gate"
FIELD_PORTAL_ID = "portal.field.west_gate"
FIELD_VISITED_QUEST = "quest.first.field_visited"

PLANET_RADIUS_CM = 10000.0
ENGINE_SPHERE_RADIUS_CM = 50.0
SPAWN_CLEARANCE_CM = 25.0
KILL_Z_CM = -1000000.0
ATMOSPHERE_BOTTOM_RADIUS_KM = PLANET_RADIUS_CM / 100000.0
ATMOSPHERE_HEIGHT_KM = 10.0

TOWN_START_ANGLE = 0.0
TOWN_RETURN_ANGLE = 8.0
TOWN_PORTAL_ANGLE = 12.0
FIELD_ARRIVAL_ANGLE = -8.0
FIELD_PORTAL_ANGLE = -12.0
MIN_PORTAL_ANCHOR_ARC_CM = 600.0

MAIN_LIGHT_ROTATION = (-35.0, -135.0, 0.0)
FILL_LIGHT_ROTATION = (35.0, 45.0, 0.0)
MAIN_FORWARD_SHADING_PRIORITY = 1
FILL_FORWARD_SHADING_PRIORITY = 0

TOWN_DESCRIPTOR_LABEL = "RegionDescriptor_Town"
TOWN_START_LABEL = "RegionalStart_Town_Start"
TOWN_RETURN_LABEL = "RegionalStart_Town_FromField"
TOWN_PORTAL_LABEL = "RegionPortal_Town_EastGate"
FIELD_DESCRIPTOR_LABEL = "RegionDescriptor_Field"
FIELD_START_LABEL = "RegionalStart_Field_FromTown"
FIELD_PORTAL_LABEL = "RegionPortal_Field_WestGate"

TOWN_SHELL_LABELS = {
    "PlanetBody_0",
    "DirectionalLight_Main",
    "DirectionalLight_Fill",
    "SkyLight_0",
    "SkyAtmosphere_0",
    "TownPlaza_0",
    "TownHouse_Left",
    "TownHouse_Right",
    "TownGate_Left",
    "TownGate_Right",
}
FIELD_SHELL_LABELS = {
    "PlanetBody_0",
    "DirectionalLight_Main",
    "DirectionalLight_Fill",
    "SkyLight_0",
    "SkyAtmosphere_0",
    "FieldPath_0",
    "FieldRock_0",
    "FieldRock_1",
    "FieldMarker_Left",
    "FieldMarker_Right",
}
TOWN_REGIONAL_LABELS = {
    TOWN_DESCRIPTOR_LABEL,
    TOWN_START_LABEL,
    TOWN_RETURN_LABEL,
    TOWN_PORTAL_LABEL,
}
FIELD_REGIONAL_LABELS = {
    FIELD_DESCRIPTOR_LABEL,
    FIELD_START_LABEL,
    FIELD_PORTAL_LABEL,
}
TOWN_EXPECTED_LABELS = TOWN_SHELL_LABELS | TOWN_REGIONAL_LABELS
FIELD_EXPECTED_LABELS = FIELD_SHELL_LABELS | FIELD_REGIONAL_LABELS

PROTECTED_ASSET_FILES = {
    SOURCE_CHARACTER: ".uasset",
    SOURCE_GAME_MODE: ".uasset",
    SOURCE_GRAYBOX: ".umap",
}
TARGET_ASSET_FILES = {
    TARGET_CATALOG: ".uasset",
    TARGET_REGIONAL_GAME_MODE: ".uasset",
    TARGET_TOWN_MAP: ".umap",
    TARGET_FIELD_MAP: ".umap",
}

CREATED_THIS_RUN: list[str] = []
NATIVE_CLASSES: dict[str, unreal.Class] = {}
STRUCT_WRAPPERS: dict[str, type] = {}
PROTECTED_HASHES_BEFORE: dict[str, str] = {}


def info(message: str) -> None:
    unreal.log(f"[EverPlanet First Planet Regions] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet First Planet Regions] {message}")
        raise RuntimeError(message)


def audit_json_value(value: object) -> object:
    """Return a deterministic JSON primitive tree or reject the caller.

    Audit issues must never retain Unreal wrappers, Actors, exceptions, or
    tracebacks while another editor world is loaded.  Callers therefore reduce
    every observed value to primitives before adding it to the collector.
    """
    if value is None or type(value) in (bool, int, str):
        return value
    if type(value) is float:
        return value if math.isfinite(value) else str(value)
    if type(value) in (list, tuple):
        return [audit_json_value(item) for item in value]
    if type(value) is dict:
        return {
            str(key): audit_json_value(item)
            for key, item in sorted(value.items(), key=lambda pair: str(pair[0]))
        }
    raise TypeError(
        "Audit diagnostics must be JSON primitives, not "
        f"{type(value).__module__}.{type(value).__qualname__}"
    )


def audit_issue(
    issues: list[dict[str, object]],
    scope: str,
    check: str,
    actual: object,
    expected: object,
) -> None:
    issues.append(
        {
            "scope": scope,
            "check": check,
            "actual": audit_json_value(actual),
            "expected": audit_json_value(expected),
        }
    )


def audit_expect(
    issues: list[dict[str, object]],
    condition: bool,
    scope: str,
    check: str,
    actual: object,
    expected: object,
) -> bool:
    if condition:
        return True
    audit_issue(issues, scope, check, actual, expected)
    return False


def audit_exception_text(exc: Exception) -> str:
    # Convert immediately: the collector never receives or retains ``exc``.
    return f"{type(exc).__module__}.{type(exc).__qualname__}: {exc}"


def audit_catalog_validation_result(result: object) -> tuple[bool, str]:
    """Interpret ValidateCatalog without invoking fail-fast audit helpers."""
    shape = "scalar"
    raw_valid: object = result
    raw_error: object = ""
    if type(result) is str:
        shape = "ue-bool-success-out-error"
        raw_valid = True
        raw_error = result
    elif result is None:
        shape = "ue-bool-false-out-discarded"
        raw_valid = False
        raw_error = None
    elif type(result) in (tuple, list) and len(result) == 2:
        shape = f"{type(result).__name__}:2"
        raw_valid, raw_error = result
    elif type(result) is dict and set(result) == {"return_value", "out_error"}:
        shape = "dict:return_value+out_error"
        raw_valid = result["return_value"]
        raw_error = result["out_error"]
    elif hasattr(result, "return_value") and hasattr(result, "out_error"):
        shape = "wrapper:return_value+out_error"
        try:
            raw_valid = getattr(result, "return_value")
            raw_error = getattr(result, "out_error")
        except Exception as exc:
            exception_text = audit_exception_text(exc)
            del exc
            return False, f"wrapper-attribute-read-failed:{exception_text}"
    else:
        getter = getattr(result, "get_editor_property", None)
        if not callable(getter):
            return False, f"unsupported-result-shape:{qualified_type_name(result)}"
        shape = "wrapper:get_editor_property"
        try:
            raw_valid = getter("return_value")
            raw_error = getter("out_error")
        except Exception as exc:
            exception_text = audit_exception_text(exc)
            del exc
            return False, f"wrapper-property-read-failed:{exception_text}"

    valid_is_typed = type(raw_valid) is bool or (
        type(raw_valid) is int and raw_valid in (0, 1)
    )
    error_is_typed = raw_error is None or type(raw_error) is str
    parsed_valid = bool(raw_valid) if valid_is_typed else False
    parsed_error = "" if raw_error is None else str(raw_error)
    success = valid_is_typed and error_is_typed and parsed_valid and not parsed_error
    diagnostic = (
        f"shape={shape},validType={qualified_type_name(raw_valid)},"
        f"valid={parsed_valid},errorType={qualified_type_name(raw_error)},"
        f"error={parsed_error}"
    )
    return success, diagnostic


def canonical_asset_object_path(package_path: str) -> str:
    asset_name = package_path.rsplit("/", 1)[1]
    return f"{package_path}.{asset_name}"


def canonical_blueprint_class_path(package_path: str) -> str:
    return canonical_asset_object_path(package_path) + "_C"


def audit_soft_reference_package(value: object) -> str:
    """Return a package diagnostic without fail-fast logging side effects."""
    try:
        candidate = value
        to_soft_path = getattr(candidate, "to_soft_object_path", None)
        if callable(to_soft_path):
            candidate = to_soft_path()
        get_long_package_name = getattr(candidate, "get_long_package_name", None)
        if callable(get_long_package_name):
            package_name = str(get_long_package_name())
        else:
            if isinstance(candidate, unreal.Object):
                text = candidate.get_path_name()
            else:
                get_asset_path_string = getattr(candidate, "get_asset_path_string", None)
                text = (
                    str(get_asset_path_string())
                    if callable(get_asset_path_string)
                    else str(candidate)
                )
            game_index = text.find("/Game/")
            if game_index < 0:
                return f"<invalid-no-game-package:{text}>"
            text = text[game_index:]
            for delimiter in ("'", '"', " "):
                text = text.split(delimiter, 1)[0]
            package_name = text.split(".", 1)[0]
        if "UEDPIE_" in package_name:
            return f"<invalid-pie-package:{package_name}>"
        return package_name
    except Exception as exc:
        exception_text = audit_exception_text(exc)
        del exc
        return f"<invalid-exception:{exception_text}>"


def vector_json(value: unreal.Vector) -> list[float]:
    return [float(value.x), float(value.y), float(value.z)]


def rotator_json(value: unreal.Rotator) -> list[float]:
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def object_path(obj: unreal.Object | None) -> str:
    return obj.get_path_name() if obj else "None"


def class_path(obj: unreal.Object | None) -> str:
    return obj.get_class().get_path_name() if obj else "None"


def class_is_child_of(actual_class: unreal.Class, expected_parent: unreal.Class) -> bool:
    return bool(unreal.MathLibrary.class_is_child_of(actual_class, expected_parent))


def load_class_checked(path: str) -> unreal.Class:
    loaded = unreal.load_class(None, path)
    require(loaded is not None, f"Required class could not be loaded: {path}")
    return loaded


def load_asset_checked(path: str) -> unreal.Object:
    loaded = unreal.EditorAssetLibrary.load_asset(path)
    require(loaded is not None, f"Required asset could not be loaded: {path}")
    return loaded


def load_blueprint_class_checked(path: str) -> unreal.Class:
    loaded = unreal.EditorAssetLibrary.load_blueprint_class(path)
    require(loaded is not None, f"Generated Blueprint class could not be loaded: {path}")
    return loaded


def compile_blueprint_checked(blueprint: unreal.Blueprint, label: str) -> None:
    result = unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(bool(result), f"Blueprint compilation failed: {label}")
    info(f"FIRST_PLANET_REGIONS_CREATE_BLUEPRINT_COMPILE=success asset={label}")


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
        unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1]),
        False,
    )
    require(actor is not None, f"Failed to spawn actor: {label}")
    set_actor_label(actor, label)
    return actor


def struct_property(value: object, property_name: str) -> object:
    getter = getattr(value, "get_editor_property", None)
    if callable(getter):
        return getter(property_name)
    return getattr(value, property_name)


def make_struct(wrapper_name: str, **properties: object) -> object:
    wrapper = STRUCT_WRAPPERS[wrapper_name]
    value = wrapper()
    setter = getattr(value, "set_editor_property", None)
    require(callable(setter), f"USTRUCT wrapper is not editor-writable: {wrapper_name}")
    for property_name, property_value in properties.items():
        setter(property_name, property_value)
    return value


def stable_id(value: object) -> str:
    text = str(value)
    separators = "._-"
    require(
        0 < len(text) <= 128
        and text[0] not in separators
        and text[-1] not in separators
        and all(
            "0" <= character <= "9"
            or "a" <= character <= "z"
            or character in separators
            for character in text
        ),
        f"Invalid stable ID: {text}",
    )
    return text


def vector_length(vector: unreal.Vector) -> float:
    return math.sqrt(float(vector.x) ** 2 + float(vector.y) ** 2 + float(vector.z) ** 2)


def vector_distance(
    actual: unreal.Vector,
    expected: tuple[float, float, float],
) -> float:
    return math.sqrt(
        (float(actual.x) - expected[0]) ** 2
        + (float(actual.y) - expected[1]) ** 2
        + (float(actual.z) - expected[2]) ** 2
    )


def vector_dot(actual: unreal.Vector, expected: tuple[float, float, float]) -> float:
    return (
        float(actual.x) * expected[0]
        + float(actual.y) * expected[1]
        + float(actual.z) * expected[2]
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
    pitch, yaw, roll = expected_pitch_yaw_roll
    return (
        angle_distance_degrees(float(actual.pitch), pitch) <= tolerance
        and angle_distance_degrees(float(actual.yaw), yaw) <= tolerance
        and angle_distance_degrees(float(actual.roll), roll) <= tolerance
    )


def surface_frame(angle_degrees: float) -> dict[str, tuple[float, float, float]]:
    radians = math.radians(angle_degrees)
    return {
        "normal": (math.sin(radians), 0.0, math.cos(radians)),
        "forward": (math.cos(radians), 0.0, -math.sin(radians)),
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


def actor_surface_spec(
    label: str,
    angle_degrees: float,
    radial_up_offset: float,
    forward_offset: float = 0.0,
    right_offset: float = 0.0,
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
) -> dict[str, object]:
    frame = surface_frame(angle_degrees)
    return {
        "label": label,
        "angle": angle_degrees,
        "location": local_surface_location(
            angle_degrees,
            forward_offset,
            right_offset,
            radial_up_offset,
        ),
        "rotation": frame["rotation"],
        "scale": scale,
        "forward": frame["forward"],
        "up": frame["normal"],
    }


def static_surface_spec(
    label: str,
    angle_degrees: float,
    forward_offset: float,
    right_offset: float,
    up_offset: float,
    scale: tuple[float, float, float],
    material_path: str,
    collision_profile: str,
) -> dict[str, object]:
    spec = actor_surface_spec(
        label,
        angle_degrees,
        up_offset,
        forward_offset,
        right_offset,
        scale,
    )
    spec.update(
        {
            "mesh_path": CUBE_MESH,
            "material_path": material_path,
            "collision_profile": collision_profile,
        }
    )
    return spec


TOWN_STATIC_SPECS = (
    static_surface_spec(
        "TownPlaza_0", 2.0, 0.0, 0.0, 8.0,
        (10.0, 10.0, 0.12), PLAZA_MATERIAL, "NoCollision"
    ),
    static_surface_spec(
        "TownHouse_Left", 4.0, -250.0, -900.0, 250.0,
        (4.0, 5.0, 5.0), LANDMARK_MATERIAL_A, "BlockAll"
    ),
    static_surface_spec(
        "TownHouse_Right", 4.0, -250.0, 900.0, 250.0,
        (4.0, 5.0, 5.0), LANDMARK_MATERIAL_B, "BlockAll"
    ),
    static_surface_spec(
        "TownGate_Left", 11.0, 0.0, -500.0, 300.0,
        (1.0, 1.0, 6.0), LANDMARK_MATERIAL_A, "BlockAll"
    ),
    static_surface_spec(
        "TownGate_Right", 11.0, 0.0, 500.0, 300.0,
        (1.0, 1.0, 6.0), LANDMARK_MATERIAL_A, "BlockAll"
    ),
)

FIELD_STATIC_SPECS = (
    static_surface_spec(
        "FieldPath_0", -4.0, 0.0, 0.0, 8.0,
        (15.0, 3.0, 0.10), ROUTE_MATERIAL, "NoCollision"
    ),
    static_surface_spec(
        "FieldRock_0", -3.0, 200.0, -700.0, 150.0,
        (3.0, 4.0, 3.0), LANDMARK_MATERIAL_B, "BlockAll"
    ),
    static_surface_spec(
        "FieldRock_1", -6.0, -200.0, 650.0, 200.0,
        (4.0, 3.0, 4.0), LANDMARK_MATERIAL_B, "BlockAll"
    ),
    static_surface_spec(
        "FieldMarker_Left", -11.0, 0.0, -500.0, 250.0,
        (1.0, 1.0, 5.0), LANDMARK_MATERIAL_A, "BlockAll"
    ),
    static_surface_spec(
        "FieldMarker_Right", -11.0, 0.0, 500.0, 250.0,
        (1.0, 1.0, 5.0), LANDMARK_MATERIAL_A, "BlockAll"
    ),
)


def asset_disk_path(asset_path: str, extension: str) -> str:
    require(asset_path.startswith("/Game/"), f"Not a /Game asset: {asset_path}")
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    relative = asset_path[len("/Game/") :].replace("/", os.sep) + extension
    return os.path.normpath(os.path.join(content_dir, relative))


def target_package_presence(
    asset_subsystem: unreal.EditorAssetSubsystem,
    asset_path: str,
) -> tuple[bool, bool, str]:
    require(asset_path in TARGET_ASSET_FILES, f"Not an exact target asset: {asset_path}")
    package_file = asset_disk_path(asset_path, TARGET_ASSET_FILES[asset_path])
    return (
        bool(asset_subsystem.does_asset_exist(asset_path)),
        os.path.isfile(package_file),
        package_file,
    )


def sha256_file(path: str) -> str:
    require(os.path.isfile(path), f"Protected source package file is missing: {path}")
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def snapshot_protected_hashes() -> dict[str, str]:
    result: dict[str, str] = {}
    for asset_path, extension in PROTECTED_ASSET_FILES.items():
        package_file = asset_disk_path(asset_path, extension)
        result[asset_path] = sha256_file(package_file)
    return result


def log_protected_hashes(marker: str, hashes: dict[str, str]) -> None:
    for asset_path in (SOURCE_CHARACTER, SOURCE_GAME_MODE, SOURCE_GRAYBOX):
        info(f"{marker}=asset={asset_path},sha256={hashes[asset_path]}")


def verify_protected_hashes_unchanged(marker: str) -> None:
    after = snapshot_protected_hashes()
    log_protected_hashes(marker, after)
    for asset_path, before in PROTECTED_HASHES_BEFORE.items():
        require(
            after[asset_path] == before,
            "Protected source package changed on disk: "
            f"{asset_path}, before={before}, after={after[asset_path]}",
        )
    info(f"{marker}_UNCHANGED=True assets=3")


def soft_reference_package(value: object) -> str:
    candidate = value
    to_soft_path = getattr(candidate, "to_soft_object_path", None)
    if callable(to_soft_path):
        candidate = to_soft_path()

    get_long_package_name = getattr(candidate, "get_long_package_name", None)
    if callable(get_long_package_name):
        package_name = str(get_long_package_name())
    else:
        if isinstance(candidate, unreal.Object):
            text = candidate.get_path_name()
        else:
            get_asset_path_string = getattr(candidate, "get_asset_path_string", None)
            text = str(get_asset_path_string()) if callable(get_asset_path_string) else str(candidate)
        game_index = text.find("/Game/")
        require(game_index >= 0, f"Soft reference has no /Game package: {text}")
        text = text[game_index:]
        for delimiter in ("'", '"', " "):
            text = text.split(delimiter, 1)[0]
        package_name = text.split(".", 1)[0]
    require("UEDPIE_" not in package_name, f"PIE map reference is forbidden: {package_name}")
    return package_name


def current_world_checked(expected_package: str) -> unreal.World:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    world = editor_subsystem.get_editor_world()
    require(world is not None, "Editor World unavailable")
    actual_package = soft_reference_package(world)
    require(
        actual_package == expected_package,
        f"Editor World package mismatch: actual={actual_package}, expected={expected_package}",
    )
    return world


def resolve_native_api_before_mutation() -> None:
    class_paths = (
        CATALOG_CLASS_PATH,
        REGION_DESCRIPTOR_CLASS_PATH,
        REGION_PLAYER_START_CLASS_PATH,
        REGION_PORTAL_CLASS_PATH,
        PLANET_BODY_CLASS_PATH,
        STATIC_MESH_ACTOR_CLASS_PATH,
        GENERIC_PLAYER_START_CLASS_PATH,
        DIRECTIONAL_LIGHT_CLASS_PATH,
        SKY_LIGHT_CLASS_PATH,
        SKY_ATMOSPHERE_CLASS_PATH,
        WORLD_CLASS_PATH,
        NATIVE_GAME_MODE_CLASS_PATH,
        GLOBE_CAMERA_COMPONENT_CLASS_PATH,
    )
    for path in class_paths:
        NATIVE_CLASSES[path] = load_class_checked(path)

    for wrapper_name in STRUCT_WRAPPER_NAMES:
        wrapper = getattr(unreal, wrapper_name, None)
        require(wrapper is not None, f"Required USTRUCT Python wrapper missing: unreal.{wrapper_name}")
        probe = wrapper()
        require(probe is not None, f"Required USTRUCT wrapper could not instantiate: {wrapper_name}")
        STRUCT_WRAPPERS[wrapper_name] = wrapper

    # Exercise nested wrappers before any asset/package mutation.
    probe_address = make_struct(
        "EverPlanetWorldAddress",
        planet_id=PLANET_ID,
        region_id=TOWN_REGION_ID,
        spawn_anchor_id=TOWN_START_ANCHOR,
    )
    probe_key = make_struct(
        "EverPlanetRegionKey",
        planet_id=PLANET_ID,
        region_id=TOWN_REGION_ID,
    )
    probe_link = make_struct(
        "EverPlanetTravelLinkDefinition",
        link_id=TOWN_TO_FIELD_LINK,
        source=probe_key,
        source_portal_id=TOWN_PORTAL_ID,
        destination=probe_address,
    )
    require(
        stable_id(struct_property(probe_link, "link_id")) == TOWN_TO_FIELD_LINK,
        "USTRUCT nested write/read probe failed",
    )
    info(
        "FIRST_PLANET_REGIONS_PREFLIGHT_NATIVE_API="
        f"classes={len(class_paths)},structWrappers={len(STRUCT_WRAPPERS)},status=resolved"
    )


def required_editor_subsystems() -> tuple[
    unreal.EditorAssetSubsystem,
    unreal.LevelEditorSubsystem,
    unreal.EditorActorSubsystem,
    unreal.UnrealEditorSubsystem,
]:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    require(level_subsystem is not None, "LevelEditorSubsystem unavailable")
    require(actor_subsystem is not None, "EditorActorSubsystem unavailable")
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    return asset_subsystem, level_subsystem, actor_subsystem, editor_subsystem


def audit_protected_sources(
    asset_subsystem: unreal.EditorAssetSubsystem,
    level_subsystem: unreal.LevelEditorSubsystem,
    actor_subsystem: unreal.EditorActorSubsystem,
) -> tuple[unreal.Class, unreal.Class]:
    for path in (
        SOURCE_CHARACTER,
        SOURCE_GAME_MODE,
        SOURCE_GRAYBOX,
        CUBE_MESH,
        SPHERE_MESH,
        PLANET_MATERIAL,
        ROUTE_MATERIAL,
        PLAZA_MATERIAL,
        LANDMARK_MATERIAL_A,
        LANDMARK_MATERIAL_B,
    ):
        require(asset_subsystem.does_asset_exist(path), f"Required source asset missing: {path}")
        load_asset_checked(path)

    source_character_bp = load_asset_checked(SOURCE_CHARACTER)
    source_game_mode_bp = load_asset_checked(SOURCE_GAME_MODE)
    require(isinstance(source_character_bp, unreal.Blueprint), "Product Character is not a Blueprint")
    require(isinstance(source_game_mode_bp, unreal.Blueprint), "Product GameMode is not a Blueprint")

    source_character_class = load_blueprint_class_checked(SOURCE_CHARACTER)
    require(
        object_path(source_character_class) == SOURCE_CHARACTER_CLASS_PATH,
        "Product Character generated class path mismatch: "
        f"{object_path(source_character_class)}",
    )
    character_defaults = unreal.get_default_object(source_character_class)
    require(character_defaults is not None, "Product Character defaults unavailable")
    camera_rig = character_defaults.get_editor_property("planet_globe_camera_component")
    require(camera_rig is not None, "Product Character globe camera component unavailable")
    require(
        class_path(camera_rig) == GLOBE_CAMERA_COMPONENT_CLASS_PATH,
        f"Product Character globe camera class mismatch: {class_path(camera_rig)}",
    )
    require(
        float(camera_rig.get_editor_property("camera_yaw_degrees_per_input_unit")) == 0.0
        and float(camera_rig.get_editor_property("camera_pitch_degrees_per_input_unit")) == 0.0,
        "Product Character must already have exact fixed mouse yaw/pitch 0/0",
    )

    source_game_mode_class = load_blueprint_class_checked(SOURCE_GAME_MODE)
    require(
        object_path(source_game_mode_class) == SOURCE_GAME_MODE_CLASS_PATH,
        f"Product GameMode generated class path mismatch: {object_path(source_game_mode_class)}",
    )
    native_game_mode = NATIVE_CLASSES[NATIVE_GAME_MODE_CLASS_PATH]
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(source_game_mode_bp)
        == native_game_mode,
        "Product GameMode native parent mismatch",
    )
    game_mode_defaults = unreal.get_default_object(source_game_mode_class)
    require(game_mode_defaults is not None, "Product GameMode defaults unavailable")
    require(
        game_mode_defaults.get_editor_property("default_pawn_class") == source_character_class,
        "Product GameMode does not use the fixed Product Character",
    )
    require(
        game_mode_defaults.get_editor_property("require_regional_spawn_flow") is False,
        "Product base GameMode must remain regional=false",
    )

    require(level_subsystem.load_level(SOURCE_GRAYBOX), "Protected exact-100 graybox load failed")
    current_world_checked(SOURCE_GRAYBOX)
    source_actors = actor_subsystem.get_all_level_actors()
    source_labels = [actor.get_actor_label() for actor in source_actors]
    require(
        len(source_actors) == 100 and len(source_labels) == len(set(source_labels)),
        "Protected Product graybox is no longer the exact-100 unique-label baseline: "
        f"actors={len(source_actors)}, uniqueLabels={len(set(source_labels))}",
    )

    info(
        "FIRST_PLANET_REGIONS_PREFLIGHT_PROTECTED="
        "characterFixedCamera=0/0,baseGameModeRegional=false,grayboxActors=100"
    )
    return source_character_class, source_game_mode_class


def preflight_target_state(
    asset_subsystem: unreal.EditorAssetSubsystem,
) -> str:
    states = tuple(asset_subsystem.does_asset_exist(path) for path in TARGET_ASSETS)
    require(
        states == (False, False, False, False) or states == (True, True, True, True),
        "First Planet targets must be all-none or all-existing; refusing partial repair: "
        + ", ".join(
            f"{path}={'existing' if exists else 'missing'}"
            for path, exists in zip(TARGET_ASSETS, states)
        ),
    )
    mode = "fresh" if not any(states) else "already-exact-candidate"
    info(
        "FIRST_PLANET_REGIONS_PREFLIGHT_TARGETS="
        + ",".join(
            f"{path}={'existing' if exists else 'missing'}"
            for path, exists in zip(TARGET_ASSETS, states)
        )
        + f",mode={mode}"
    )
    return mode


def create_regional_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    source_game_mode_class: unreal.Class,
) -> unreal.Blueprint:
    require(
        not asset_subsystem.does_asset_exist(TARGET_REGIONAL_GAME_MODE),
        "Refusing to overwrite Regional GameMode",
    )
    duplicated = asset_subsystem.duplicate_asset(
        SOURCE_GAME_MODE,
        TARGET_REGIONAL_GAME_MODE,
    )
    if asset_subsystem.does_asset_exist(TARGET_REGIONAL_GAME_MODE):
        CREATED_THIS_RUN.append(TARGET_REGIONAL_GAME_MODE)
    require(isinstance(duplicated, unreal.Blueprint), "Regional GameMode duplication failed")
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicated, source_game_mode_class)
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(duplicated)
        == source_game_mode_class,
        "Regional GameMode reparent did not produce the exact Product BP parent",
    )
    compile_blueprint_checked(duplicated, TARGET_REGIONAL_GAME_MODE)

    generated_class = load_blueprint_class_checked(TARGET_REGIONAL_GAME_MODE)
    defaults = unreal.get_default_object(generated_class)
    require(defaults is not None, "Regional GameMode defaults unavailable")
    defaults.modify()
    defaults.set_editor_property("require_regional_spawn_flow", True)
    require(
        defaults.get_editor_property("require_regional_spawn_flow") is True,
        "Regional GameMode policy did not become true in memory",
    )
    require(
        asset_subsystem.save_asset(TARGET_REGIONAL_GAME_MODE, False),
        "Regional GameMode save failed",
    )
    info(
        "FIRST_PLANET_REGIONS_SAVE="
        f"asset={TARGET_REGIONAL_GAME_MODE},type=Blueprint,status=success"
    )
    return duplicated


def audit_regional_game_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    source_character_class: unreal.Class,
    source_game_mode_class: unreal.Class,
) -> unreal.Class:
    blueprint = asset_subsystem.load_asset(TARGET_REGIONAL_GAME_MODE)
    require(isinstance(blueprint, unreal.Blueprint), "Regional GameMode is not a Blueprint")
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(
        actual_parent == source_game_mode_class,
        "Regional GameMode parent is not exact BP_EverPlanetGameMode_C: "
        f"actual={object_path(actual_parent)}, expected={SOURCE_GAME_MODE_CLASS_PATH}",
    )
    generated_class = load_blueprint_class_checked(TARGET_REGIONAL_GAME_MODE)
    defaults = unreal.get_default_object(generated_class)
    parent_defaults = unreal.get_default_object(source_game_mode_class)
    require(defaults is not None and parent_defaults is not None, "GameMode defaults unavailable")
    require(
        defaults.get_editor_property("require_regional_spawn_flow") is True,
        "Regional GameMode child CDO must be regional=true",
    )
    require(
        parent_defaults.get_editor_property("require_regional_spawn_flow") is False,
        "Product parent GameMode CDO must remain regional=false",
    )
    require(
        defaults.get_editor_property("default_pawn_class") == source_character_class,
        "Regional GameMode did not inherit the fixed Product Character",
    )
    require(
        defaults.get_editor_property("player_controller_class")
        == parent_defaults.get_editor_property("player_controller_class"),
        "Regional GameMode PlayerController differs from Product parent",
    )
    info(
        "FIRST_PLANET_REGIONS_AUDIT_GAMEMODE="
        f"asset={TARGET_REGIONAL_GAME_MODE},parent={SOURCE_GAME_MODE_CLASS_PATH},"
        "childRegional=true,parentRegional=false,fixedCameraPawn=inherited"
    )
    return generated_class


def collect_regional_game_mode_audit(
    asset_subsystem: unreal.EditorAssetSubsystem,
    source_character_class: unreal.Class,
    source_game_mode_class: unreal.Class,
    issues: list[dict[str, object]],
) -> None:
    scope = "GameMode"
    issue_start = len(issues)
    blueprint = asset_subsystem.load_asset(TARGET_REGIONAL_GAME_MODE)
    if not audit_expect(
        issues,
        isinstance(blueprint, unreal.Blueprint),
        scope,
        "asset-class",
        class_path(blueprint),
        "/Script/Engine.Blueprint",
    ):
        return

    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    actual_parent_path = object_path(actual_parent)
    audit_expect(
        issues,
        actual_parent_path == SOURCE_GAME_MODE_CLASS_PATH,
        scope,
        "parent-class-path",
        actual_parent_path,
        SOURCE_GAME_MODE_CLASS_PATH,
    )

    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(
        TARGET_REGIONAL_GAME_MODE
    )
    actual_generated_class_path = object_path(generated_class)
    expected_generated_class_path = canonical_blueprint_class_path(
        TARGET_REGIONAL_GAME_MODE
    )
    if not audit_expect(
        issues,
        actual_generated_class_path == expected_generated_class_path,
        scope,
        "generated-class-path",
        actual_generated_class_path,
        expected_generated_class_path,
    ):
        return

    defaults = unreal.get_default_object(generated_class)
    parent_defaults = unreal.get_default_object(source_game_mode_class)
    defaults_ready = audit_expect(
        issues,
        defaults is not None,
        scope,
        "child-default-object",
        defaults is not None,
        True,
    )
    parent_defaults_ready = audit_expect(
        issues,
        parent_defaults is not None,
        scope,
        "parent-default-object",
        parent_defaults is not None,
        True,
    )

    if defaults_ready:
        child_regional = bool(
            defaults.get_editor_property("require_regional_spawn_flow")
        )
        audit_expect(
            issues,
            child_regional,
            scope,
            "child-require-regional-spawn-flow",
            child_regional,
            True,
        )
        actual_pawn_path = object_path(
            defaults.get_editor_property("default_pawn_class")
        )
        audit_expect(
            issues,
            actual_pawn_path == SOURCE_CHARACTER_CLASS_PATH,
            scope,
            "default-pawn-class-path",
            actual_pawn_path,
            SOURCE_CHARACTER_CLASS_PATH,
        )

    if parent_defaults_ready:
        parent_regional = bool(
            parent_defaults.get_editor_property("require_regional_spawn_flow")
        )
        audit_expect(
            issues,
            not parent_regional,
            scope,
            "parent-require-regional-spawn-flow",
            parent_regional,
            False,
        )

    if defaults_ready and parent_defaults_ready:
        child_controller_path = object_path(
            defaults.get_editor_property("player_controller_class")
        )
        parent_controller_path = object_path(
            parent_defaults.get_editor_property("player_controller_class")
        )
        audit_expect(
            issues,
            child_controller_path == parent_controller_path,
            scope,
            "player-controller-class-path",
            child_controller_path,
            parent_controller_path,
        )

    if len(issues) == issue_start:
        info(
            "FIRST_PLANET_REGIONS_AUDIT_GAMEMODE="
            f"asset={TARGET_REGIONAL_GAME_MODE},parent={SOURCE_GAME_MODE_CLASS_PATH},"
            "childRegional=true,parentRegional=false,fixedCameraPawn=inherited"
        )


def configure_skeleton_world(
    world: unreal.World,
    game_mode_class: unreal.Class,
) -> None:
    world_settings = world.get_world_settings()
    require(world_settings is not None, "Skeleton WorldSettings unavailable")
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    world_settings.set_editor_property("kill_z", KILL_Z_CM)


def create_skeleton_map(
    target_map: str,
    game_mode_class: unreal.Class,
) -> None:
    _, level_subsystem, actor_subsystem, _ = required_editor_subsystems()
    require(
        not unreal.EditorAssetLibrary.does_asset_exist(target_map),
        f"Refusing to overwrite regional map: {target_map}",
    )
    created = level_subsystem.new_level(target_map, False)
    if unreal.EditorAssetLibrary.does_asset_exist(target_map):
        CREATED_THIS_RUN.append(target_map)
    require(created, f"Non-partitioned skeleton map creation failed: {target_map}")
    world = current_world_checked(target_map)
    require(
        len(actor_subsystem.get_all_level_actors()) == 0,
        f"Fresh skeleton map unexpectedly contains authored actors: {target_map}",
    )
    configure_skeleton_world(world, game_mode_class)
    require(level_subsystem.save_current_level(), f"Skeleton map save failed: {target_map}")
    info(
        "FIRST_PLANET_REGIONS_CREATE_SKELETON="
        f"map={target_map},partitioned=false,actors=0"
    )
    info(f"FIRST_PLANET_REGIONS_SAVE=asset={target_map},type=MapSkeleton,status=success")


def reload_saved_world_for_catalog(target_map: str) -> unreal.World:
    # Never retain the editor UWorld returned while authoring a map across a
    # later NewLevel call.  UE destroys/cleanses that editor world when the next
    # skeleton is created, so reacquire the saved package object only after both
    # skeleton maps have been saved.
    saved_world = load_asset_checked(target_map)
    require(class_path(saved_world) == WORLD_CLASS_PATH, f"Saved map is not a World: {target_map}")
    require(
        soft_reference_package(saved_world) == target_map,
        f"Saved map object is not the exact non-PIE target: {target_map}",
    )
    return saved_world


def build_catalog_structs(
    town_world: unreal.World,
    field_world: unreal.World,
) -> tuple[object, list[object], list[object], list[object]]:
    require(
        soft_reference_package(town_world) == TARGET_TOWN_MAP
        and soft_reference_package(field_world) == TARGET_FIELD_MAP,
        "Catalog must use the exact saved non-PIE Town/Field World objects",
    )
    initial_address = make_struct(
        "EverPlanetWorldAddress",
        planet_id=PLANET_ID,
        region_id=TOWN_REGION_ID,
        spawn_anchor_id=TOWN_START_ANCHOR,
    )
    planet = make_struct(
        "EverPlanetPlanetDefinition",
        planet_id=PLANET_ID,
        display_name="First Planet",
        default_region_id=TOWN_REGION_ID,
        region_ids=[TOWN_REGION_ID, FIELD_REGION_ID],
    )
    town = make_struct(
        "EverPlanetRegionDefinition",
        planet_id=PLANET_ID,
        region_id=TOWN_REGION_ID,
        display_name="Town",
        map=town_world,
        default_spawn_anchor_id=TOWN_START_ANCHOR,
        spawn_anchor_ids=[TOWN_START_ANCHOR, TOWN_FROM_FIELD_ANCHOR],
    )
    field = make_struct(
        "EverPlanetRegionDefinition",
        planet_id=PLANET_ID,
        region_id=FIELD_REGION_ID,
        display_name="Field",
        map=field_world,
        default_spawn_anchor_id=FIELD_FROM_TOWN_ANCHOR,
        spawn_anchor_ids=[FIELD_FROM_TOWN_ANCHOR],
    )
    town_key = make_struct(
        "EverPlanetRegionKey",
        planet_id=PLANET_ID,
        region_id=TOWN_REGION_ID,
    )
    field_key = make_struct(
        "EverPlanetRegionKey",
        planet_id=PLANET_ID,
        region_id=FIELD_REGION_ID,
    )
    field_destination = make_struct(
        "EverPlanetWorldAddress",
        planet_id=PLANET_ID,
        region_id=FIELD_REGION_ID,
        spawn_anchor_id=FIELD_FROM_TOWN_ANCHOR,
    )
    town_destination = make_struct(
        "EverPlanetWorldAddress",
        planet_id=PLANET_ID,
        region_id=TOWN_REGION_ID,
        spawn_anchor_id=TOWN_FROM_FIELD_ANCHOR,
    )
    town_to_field = make_struct(
        "EverPlanetTravelLinkDefinition",
        link_id=TOWN_TO_FIELD_LINK,
        source=town_key,
        source_portal_id=TOWN_PORTAL_ID,
        destination=field_destination,
        initially_unlocked=True,
        required_quest_flag="None",
        arrival_quest_flag=FIELD_VISITED_QUEST,
    )
    field_to_town = make_struct(
        "EverPlanetTravelLinkDefinition",
        link_id=FIELD_TO_TOWN_LINK,
        source=field_key,
        source_portal_id=FIELD_PORTAL_ID,
        destination=town_destination,
        initially_unlocked=False,
        required_quest_flag=FIELD_VISITED_QUEST,
        arrival_quest_flag="None",
    )
    return initial_address, [planet], [town, field], [town_to_field, field_to_town]


def qualified_type_name(value: object) -> str:
    value_type = type(value)
    return f"{value_type.__module__}.{value_type.__qualname__}"


def deterministic_repr(value: object) -> str:
    try:
        text = repr(value)
    except Exception as exc:
        text = f"<repr-error:{qualified_type_name(exc)}>"
    text = re.sub(r"0x[0-9a-fA-F]+", "0x<address>", text)
    return text.replace("\r", "\\r").replace("\n", "\\n")


def diagnostic_soft_package(value: object) -> str:
    try:
        return soft_reference_package(value)
    except Exception as exc:
        return (
            f"<invalid:{qualified_type_name(exc)}:"
            f"{deterministic_repr(exc)}>"
        )


def catalog_assembly_contract(catalog: unreal.Object, phase: str) -> str:
    initial = catalog.get_editor_property("initial_address")
    planets = list(catalog.get_editor_property("planets"))
    regions = list(catalog.get_editor_property("regions"))
    links = list(catalog.get_editor_property("travel_links"))

    initial_text = (
        f"{struct_property(initial, 'planet_id')}/"
        f"{struct_property(initial, 'region_id')}@"
        f"{struct_property(initial, 'spawn_anchor_id')}"
    )
    planet_entries = []
    for planet in planets:
        region_ids = "+".join(
            str(value) for value in struct_property(planet, "region_ids")
        )
        planet_entries.append(
            f"{struct_property(planet, 'planet_id')}:"
            f"default={struct_property(planet, 'default_region_id')}:"
            f"regions={region_ids}"
        )

    region_entries = []
    for region in regions:
        spawn_anchor_ids = "+".join(
            str(value) for value in struct_property(region, "spawn_anchor_ids")
        )
        region_entries.append(
            f"{struct_property(region, 'planet_id')}/"
            f"{struct_property(region, 'region_id')}:"
            f"map={diagnostic_soft_package(struct_property(region, 'map'))}:"
            f"default={struct_property(region, 'default_spawn_anchor_id')}:"
            f"anchors={spawn_anchor_ids}"
        )

    link_entries = []
    for link in links:
        source = struct_property(link, "source")
        destination = struct_property(link, "destination")
        unlocked = struct_property(link, "initially_unlocked")
        if type(unlocked) is bool:
            unlocked_text = "true" if unlocked else "false"
        elif type(unlocked) is int and unlocked in (0, 1):
            unlocked_text = "true" if unlocked == 1 else "false"
        else:
            unlocked_text = deterministic_repr(unlocked)
        link_entries.append(
            f"{struct_property(link, 'link_id')}:"
            f"source={struct_property(source, 'planet_id')}/"
            f"{struct_property(source, 'region_id')}@"
            f"{struct_property(link, 'source_portal_id')}:"
            f"destination={struct_property(destination, 'planet_id')}/"
            f"{struct_property(destination, 'region_id')}@"
            f"{struct_property(destination, 'spawn_anchor_id')}:"
            f"unlocked={unlocked_text}:"
            f"required={struct_property(link, 'required_quest_flag')}:"
            f"arrival={struct_property(link, 'arrival_quest_flag')}"
        )

    return (
        f"phase={phase},planetCount={len(planets)},regionCount={len(regions)},"
        f"linkCount={len(links)},initial={initial_text},"
        f"planets=[{'|'.join(planet_entries)}],"
        f"regions=[{'|'.join(region_entries)}],"
        f"links=[{'|'.join(link_entries)}]"
    )


def unpack_native_catalog_validation(
    result: object,
    phase: str,
) -> tuple[bool, str, str]:
    result_type = qualified_type_name(result)
    result_repr = deterministic_repr(result)
    shape = "scalar"
    raw_valid: object = result
    raw_error: object = ""
    shape_error = ""
    out_error_state = "available"

    if type(result) is str:
        # UE 5.8 PyGenUtil exposes bool-return + one OutError as the OutError
        # scalar only when the native bool is true.
        shape = "ue-bool-success-out-error"
        raw_valid = True
        raw_error = result
    elif result is None:
        # On native false the same wrapper returns None and discards OutError.
        # Preserve the failure, but never invent or infer the lost error text.
        shape = "ue-bool-false-out-discarded"
        raw_valid = False
        raw_error = None
        out_error_state = "discarded-by-ue-python"
    elif isinstance(result, (tuple, list)):
        shape = f"{type(result).__name__}:{len(result)}"
        if len(result) == 2:
            raw_valid, raw_error = result
        else:
            shape_error = "sequence-length-must-be-2"
    elif isinstance(result, dict):
        shape = "dict:return_value+out_error"
        if set(result.keys()) == {"return_value", "out_error"}:
            raw_valid = result["return_value"]
            raw_error = result["out_error"]
        else:
            shape_error = "dict-keys-must-be-return_value+out_error"
    elif hasattr(result, "return_value") and hasattr(result, "out_error"):
        shape = "wrapper:return_value+out_error"
        try:
            raw_valid = getattr(result, "return_value")
            raw_error = getattr(result, "out_error")
        except Exception as exc:
            shape_error = (
                f"wrapper-attribute-read-failed:{qualified_type_name(exc)}:"
                f"{deterministic_repr(exc)}"
            )
    else:
        getter = getattr(result, "get_editor_property", None)
        if callable(getter):
            shape = "wrapper:get_editor_property"
            try:
                raw_valid = getter("return_value")
                raw_error = getter("out_error")
            except Exception as exc:
                shape_error = (
                    f"wrapper-property-read-failed:{qualified_type_name(exc)}:"
                    f"{deterministic_repr(exc)}"
                )

    parsed_valid: bool | None = None
    if not shape_error:
        if type(raw_valid) is bool:
            parsed_valid = raw_valid
        elif type(raw_valid) is int and raw_valid in (0, 1):
            # Some generated Python wrappers expose native bool as exact 0/1.
            # Do not accept any other integer or generic truthy object.
            parsed_valid = raw_valid == 1
        else:
            shape_error = (
                "return-value-must-be-bool-or-exact-0-or-1:"
                f"{qualified_type_name(raw_valid)}"
            )

    parsed_error = ""
    if not shape_error:
        if raw_error is None:
            parsed_error = ""
        elif type(raw_error) is str:
            parsed_error = raw_error
        else:
            shape_error = (
                f"out-error-must-be-str-or-none:{qualified_type_name(raw_error)}"
            )

    parsed_valid_text = (
        "unparsed"
        if parsed_valid is None
        else ("true" if parsed_valid else "false")
    )
    diagnostic = (
        f"phase={phase},resultType={result_type},shape={shape},"
        f"resultRepr={result_repr},"
        f"validType={qualified_type_name(raw_valid)},"
        f"validRepr={deterministic_repr(raw_valid)},"
        f"errorType={qualified_type_name(raw_error)},"
        f"errorRepr={deterministic_repr(raw_error)},"
        f"outErrorState={out_error_state},"
        f"parsedValid={parsed_valid_text},"
        f"shapeError={deterministic_repr(shape_error)}"
    )
    info(f"FIRST_PLANET_REGIONS_CATALOG_VALIDATE_RETURN={diagnostic}")

    if shape_error:
        raise RuntimeError(
            f"Unexpected ValidateCatalog return contract during {phase}: {diagnostic}"
        )
    require(parsed_valid is not None, f"ValidateCatalog parse invariant failed: {diagnostic}")
    if parsed_valid and parsed_error:
        raise RuntimeError(
            f"ValidateCatalog succeeded with a non-empty error during {phase}: {diagnostic}"
        )
    if not parsed_valid:
        error_text = (
            "<native-out-error-discarded-by-ue-python>"
            if out_error_state == "discarded-by-ue-python"
            else (parsed_error if parsed_error else "<native-error-empty>")
        )
        raise RuntimeError(
            f"Catalog validation failed during {phase}: {error_text}; {diagnostic}"
        )
    return parsed_valid, parsed_error, diagnostic


def validate_catalog_checked(catalog: unreal.Object, phase: str) -> None:
    info(
        "FIRST_PLANET_REGIONS_CATALOG_ASSEMBLY="
        + catalog_assembly_contract(catalog, phase)
    )
    result = catalog.validate_catalog()
    unpack_native_catalog_validation(result, phase)


def create_catalog(
    asset_subsystem: unreal.EditorAssetSubsystem,
    town_world: unreal.World,
    field_world: unreal.World,
) -> unreal.Object:
    require(
        not asset_subsystem.does_asset_exist(TARGET_CATALOG),
        "Refusing to overwrite World Catalog",
    )
    catalog_class = NATIVE_CLASSES[CATALOG_CLASS_PATH]
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", catalog_class)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    require(asset_tools is not None, "AssetTools unavailable")
    asset_name = TARGET_CATALOG.rsplit("/", 1)[1]
    package_path = TARGET_CATALOG.rsplit("/", 1)[0]
    catalog = asset_tools.create_asset(asset_name, package_path, catalog_class, factory)
    require(catalog is not None, "Native World Catalog DataAsset creation failed")
    if asset_subsystem.does_asset_exist(TARGET_CATALOG):
        CREATED_THIS_RUN.append(TARGET_CATALOG)
    require(class_path(catalog) == CATALOG_CLASS_PATH, f"Catalog class mismatch: {class_path(catalog)}")

    initial, planets, regions, links = build_catalog_structs(town_world, field_world)
    catalog.modify()
    catalog.set_editor_property("initial_address", initial)
    catalog.set_editor_property("planets", planets)
    catalog.set_editor_property("regions", regions)
    catalog.set_editor_property("travel_links", links)
    validate_catalog_checked(catalog, "create")
    require(asset_subsystem.save_asset(TARGET_CATALOG, False), "World Catalog save failed")
    info(
        "FIRST_PLANET_REGIONS_CREATE_CATALOG="
        f"asset={TARGET_CATALOG},planets=1,regions=2,links=2,validation=passed"
    )
    info(f"FIRST_PLANET_REGIONS_SAVE=asset={TARGET_CATALOG},type=DataAsset,status=success")
    return catalog


def address_tuple(value: object) -> tuple[str, str, str]:
    return (
        stable_id(struct_property(value, "planet_id")),
        stable_id(struct_property(value, "region_id")),
        stable_id(struct_property(value, "spawn_anchor_id")),
    )


def region_key_tuple(value: object) -> tuple[str, str]:
    return (
        stable_id(struct_property(value, "planet_id")),
        stable_id(struct_property(value, "region_id")),
    )


def fname_text(value: object) -> str:
    text = str(value)
    return "None" if text in ("", "None") else text


def audit_catalog(
    asset_subsystem: unreal.EditorAssetSubsystem,
    issues: list[dict[str, object]],
) -> None:
    scope = "Catalog"
    issue_start = len(issues)
    catalog = asset_subsystem.load_asset(TARGET_CATALOG)
    actual_catalog_path = object_path(catalog)
    expected_catalog_path = canonical_asset_object_path(TARGET_CATALOG)
    if not audit_expect(
        issues,
        actual_catalog_path == expected_catalog_path,
        scope,
        "asset-object-path",
        actual_catalog_path,
        expected_catalog_path,
    ):
        return
    actual_catalog_class_path = class_path(catalog)
    if not audit_expect(
        issues,
        actual_catalog_class_path == CATALOG_CLASS_PATH,
        scope,
        "asset-class-path",
        actual_catalog_class_path,
        CATALOG_CLASS_PATH,
    ):
        return

    try:
        validation_ok, validation_diagnostic = audit_catalog_validation_result(
            catalog.validate_catalog()
        )
        audit_expect(
            issues,
            validation_ok,
            scope,
            "native-validation",
            validation_diagnostic,
            "valid=true,error=empty",
        )
    except Exception as exc:
        validation_exception = audit_exception_text(exc)
        del exc
        audit_issue(
            issues,
            scope,
            "native-validation-exception",
            validation_exception,
            "no exception",
        )

    initial = catalog.get_editor_property("initial_address")
    actual_initial = (
        str(struct_property(initial, "planet_id")),
        str(struct_property(initial, "region_id")),
        str(struct_property(initial, "spawn_anchor_id")),
    )
    expected_initial = (PLANET_ID, TOWN_REGION_ID, TOWN_START_ANCHOR)
    audit_expect(
        issues,
        actual_initial == expected_initial,
        scope,
        "initial-address",
        actual_initial,
        expected_initial,
    )

    planets = list(catalog.get_editor_property("planets"))
    audit_expect(
        issues,
        len(planets) == 1,
        scope,
        "planet-count",
        len(planets),
        1,
    )
    if len(planets) == 1:
        planet = planets[0]
        actual_planet_id = str(struct_property(planet, "planet_id"))
        audit_expect(
            issues,
            actual_planet_id == PLANET_ID,
            scope,
            "planet-id",
            actual_planet_id,
            PLANET_ID,
        )
        actual_display_name = str(struct_property(planet, "display_name"))
        audit_expect(
            issues,
            actual_display_name == "First Planet",
            scope,
            "planet-display-name",
            actual_display_name,
            "First Planet",
        )
        actual_default_region = str(struct_property(planet, "default_region_id"))
        audit_expect(
            issues,
            actual_default_region == TOWN_REGION_ID,
            scope,
            "planet-default-region-id",
            actual_default_region,
            TOWN_REGION_ID,
        )
        actual_region_ids = [
            str(value) for value in struct_property(planet, "region_ids")
        ]
        expected_region_ids = [TOWN_REGION_ID, FIELD_REGION_ID]
        audit_expect(
            issues,
            actual_region_ids == expected_region_ids,
            scope,
            "planet-region-ids",
            actual_region_ids,
            expected_region_ids,
        )

    regions = list(catalog.get_editor_property("regions"))
    audit_expect(
        issues,
        len(regions) == 2,
        scope,
        "region-count",
        len(regions),
        2,
    )
    region_id_values = [
        str(struct_property(region, "region_id")) for region in regions
    ]
    region_id_counts = {
        region_id: region_id_values.count(region_id)
        for region_id in sorted(set(region_id_values))
    }
    region_by_id = {
        region_id: region
        for region_id, region in zip(region_id_values, regions)
        if region_id_counts[region_id] == 1
    }
    actual_region_ids = sorted(region_id_values)
    expected_region_ids = sorted((TOWN_REGION_ID, FIELD_REGION_ID))
    audit_expect(
        issues,
        actual_region_ids == expected_region_ids,
        scope,
        "region-ids",
        actual_region_ids,
        expected_region_ids,
    )
    region_contracts = (
        (
            TOWN_REGION_ID,
            "Town",
            TARGET_TOWN_MAP,
            TOWN_START_ANCHOR,
            [TOWN_START_ANCHOR, TOWN_FROM_FIELD_ANCHOR],
        ),
        (
            FIELD_REGION_ID,
            "Field",
            TARGET_FIELD_MAP,
            FIELD_FROM_TOWN_ANCHOR,
            [FIELD_FROM_TOWN_ANCHOR],
        ),
    )
    for (
        expected_region_id,
        expected_display_name,
        expected_map,
        expected_default_anchor,
        expected_anchors,
    ) in region_contracts:
        region = region_by_id.get(expected_region_id)
        if region is None:
            continue
        actual_owner = str(struct_property(region, "planet_id"))
        audit_expect(
            issues,
            actual_owner == PLANET_ID,
            scope,
            f"region-owner:{expected_region_id}",
            actual_owner,
            PLANET_ID,
        )
        actual_display_name = str(struct_property(region, "display_name"))
        audit_expect(
            issues,
            actual_display_name == expected_display_name,
            scope,
            f"region-display-name:{expected_region_id}",
            actual_display_name,
            expected_display_name,
        )
        actual_map = audit_soft_reference_package(struct_property(region, "map"))
        audit_expect(
            issues,
            actual_map == expected_map,
            scope,
            f"region-map:{expected_region_id}",
            actual_map,
            expected_map,
        )
        actual_default_anchor = str(
            struct_property(region, "default_spawn_anchor_id")
        )
        audit_expect(
            issues,
            actual_default_anchor == expected_default_anchor,
            scope,
            f"region-default-anchor:{expected_region_id}",
            actual_default_anchor,
            expected_default_anchor,
        )
        actual_anchors = [
            str(value) for value in struct_property(region, "spawn_anchor_ids")
        ]
        audit_expect(
            issues,
            actual_anchors == expected_anchors,
            scope,
            f"region-anchor-ids:{expected_region_id}",
            actual_anchors,
            expected_anchors,
        )

    links = list(catalog.get_editor_property("travel_links"))
    audit_expect(issues, len(links) == 2, scope, "link-count", len(links), 2)
    link_id_values = [str(struct_property(link, "link_id")) for link in links]
    link_id_counts = {
        link_id: link_id_values.count(link_id)
        for link_id in sorted(set(link_id_values))
    }
    link_by_id = {
        link_id: link
        for link_id, link in zip(link_id_values, links)
        if link_id_counts[link_id] == 1
    }
    actual_link_ids = sorted(link_id_values)
    expected_link_ids = sorted((TOWN_TO_FIELD_LINK, FIELD_TO_TOWN_LINK))
    audit_expect(
        issues,
        actual_link_ids == expected_link_ids,
        scope,
        "link-ids",
        actual_link_ids,
        expected_link_ids,
    )
    link_contracts = (
        (
            TOWN_TO_FIELD_LINK,
            (PLANET_ID, TOWN_REGION_ID),
            TOWN_PORTAL_ID,
            (PLANET_ID, FIELD_REGION_ID, FIELD_FROM_TOWN_ANCHOR),
            True,
            "None",
            FIELD_VISITED_QUEST,
        ),
        (
            FIELD_TO_TOWN_LINK,
            (PLANET_ID, FIELD_REGION_ID),
            FIELD_PORTAL_ID,
            (PLANET_ID, TOWN_REGION_ID, TOWN_FROM_FIELD_ANCHOR),
            False,
            FIELD_VISITED_QUEST,
            "None",
        ),
    )
    expected_scope = unreal.EverPlanetTravelScope.INTRA_PLANET
    for (
        expected_link_id,
        expected_source,
        expected_portal_id,
        expected_destination,
        expected_unlocked,
        expected_required_quest,
        expected_arrival_quest,
    ) in link_contracts:
        link = link_by_id.get(expected_link_id)
        if link is None:
            continue
        actual_scope = struct_property(link, "scope")
        audit_expect(
            issues,
            actual_scope == unreal.EverPlanetTravelScope.INTRA_PLANET,
            scope,
            f"link-scope:{expected_link_id}",
            str(actual_scope),
            str(expected_scope),
        )
        source = struct_property(link, "source")
        actual_source = (
            str(struct_property(source, "planet_id")),
            str(struct_property(source, "region_id")),
        )
        audit_expect(
            issues,
            actual_source == expected_source,
            scope,
            f"link-source:{expected_link_id}",
            actual_source,
            expected_source,
        )
        actual_portal_id = str(struct_property(link, "source_portal_id"))
        audit_expect(
            issues,
            actual_portal_id == expected_portal_id,
            scope,
            f"link-source-portal:{expected_link_id}",
            actual_portal_id,
            expected_portal_id,
        )
        destination = struct_property(link, "destination")
        actual_destination = (
            str(struct_property(destination, "planet_id")),
            str(struct_property(destination, "region_id")),
            str(struct_property(destination, "spawn_anchor_id")),
        )
        audit_expect(
            issues,
            actual_destination == expected_destination,
            scope,
            f"link-destination:{expected_link_id}",
            actual_destination,
            expected_destination,
        )
        actual_unlocked = struct_property(link, "initially_unlocked")
        audit_expect(
            issues,
            actual_unlocked is expected_unlocked,
            scope,
            f"link-initially-unlocked:{expected_link_id}",
            bool(actual_unlocked),
            expected_unlocked,
        )
        actual_required_quest = fname_text(
            struct_property(link, "required_quest_flag")
        )
        audit_expect(
            issues,
            actual_required_quest == expected_required_quest,
            scope,
            f"link-required-quest:{expected_link_id}",
            actual_required_quest,
            expected_required_quest,
        )
        actual_arrival_quest = fname_text(
            struct_property(link, "arrival_quest_flag")
        )
        audit_expect(
            issues,
            actual_arrival_quest == expected_arrival_quest,
            scope,
            f"link-arrival-quest:{expected_link_id}",
            actual_arrival_quest,
            expected_arrival_quest,
        )

    if len(issues) == issue_start:
        info(
            "FIRST_PLANET_REGIONS_AUDIT_CATALOG="
            "class=native,planets=1,regions=2,links=2,initial=planet.first/region.first.town@spawn.town.start,"
            "townToField=initial+arrivalQuest,fieldToTown=locked+requiredQuest,mapRefs=saved-non-PIE"
        )


def set_component_movable(actor: unreal.Actor, property_name: str) -> unreal.ActorComponent:
    component = actor.get_editor_property(property_name)
    require(component is not None, f"Missing component {property_name} on {actor.get_name()}")
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    return component


def spawn_planet_shell(
    actor_subsystem: unreal.EditorActorSubsystem,
    spawn_distance: float,
    static_specs: tuple[dict[str, object], ...],
) -> None:
    planet = spawn_actor(
        actor_subsystem,
        NATIVE_CLASSES[PLANET_BODY_CLASS_PATH],
        (0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0),
        "PlanetBody_0",
    )
    planet.set_editor_property("surface_radius", PLANET_RADIUS_CM)
    planet_mesh = planet.get_editor_property("planet_mesh")
    require(planet_mesh is not None, "Planet mesh component unavailable")
    planet_mesh.set_material(0, load_asset_checked(PLANET_MATERIAL))

    main_light = spawn_actor(
        actor_subsystem,
        NATIVE_CLASSES[DIRECTIONAL_LIGHT_CLASS_PATH],
        (0.0, 0.0, spawn_distance + 1000.0),
        MAIN_LIGHT_ROTATION,
        "DirectionalLight_Main",
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
        NATIVE_CLASSES[DIRECTIONAL_LIGHT_CLASS_PATH],
        (0.0, 0.0, -spawn_distance - 1000.0),
        FILL_LIGHT_ROTATION,
        "DirectionalLight_Fill",
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
        NATIVE_CLASSES[SKY_LIGHT_CLASS_PATH],
        (0.0, 0.0, spawn_distance + 500.0),
        (0.0, 0.0, 0.0),
        "SkyLight_0",
    )
    sky_component = set_component_movable(sky_light, "light_component")
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_editor_property("intensity", 1.5)

    sky_atmosphere = spawn_actor(
        actor_subsystem,
        NATIVE_CLASSES[SKY_ATMOSPHERE_CLASS_PATH],
        (0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0),
        "SkyAtmosphere_0",
    )
    atmosphere_component = sky_atmosphere.get_editor_property("sky_atmosphere_component")
    require(atmosphere_component is not None, "SkyAtmosphere component unavailable")
    atmosphere_component.set_editor_property(
        "transform_mode",
        unreal.SkyAtmosphereTransformMode.PLANET_CENTER_AT_COMPONENT_TRANSFORM,
    )
    atmosphere_component.set_editor_property("bottom_radius", ATMOSPHERE_BOTTOM_RADIUS_KM)
    atmosphere_component.set_editor_property("atmosphere_height", ATMOSPHERE_HEIGHT_KM)

    mesh = load_asset_checked(CUBE_MESH)
    materials = {
        path: load_asset_checked(path)
        for path in {
            ROUTE_MATERIAL,
            PLAZA_MATERIAL,
            LANDMARK_MATERIAL_A,
            LANDMARK_MATERIAL_B,
        }
    }
    for spec in static_specs:
        label = str(spec["label"])
        actor = spawn_actor(
            actor_subsystem,
            NATIVE_CLASSES[STATIC_MESH_ACTOR_CLASS_PATH],
            spec["location"],
            spec["rotation"],
            label,
        )
        component = actor.get_editor_property("static_mesh_component")
        require(component is not None, f"Static mesh component unavailable: {label}")
        require(bool(component.set_static_mesh(mesh)), f"Static mesh assignment failed: {label}")
        component.set_material(0, materials[str(spec["material_path"])])
        component.set_collision_profile_name(str(spec["collision_profile"]))
        component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        actor.set_actor_scale3d(unreal.Vector(*spec["scale"]))


def spawn_descriptor(
    actor_subsystem: unreal.EditorActorSubsystem,
    catalog: unreal.Object,
    region_id: str,
    label: str,
) -> unreal.Actor:
    descriptor = spawn_actor(
        actor_subsystem,
        NATIVE_CLASSES[REGION_DESCRIPTOR_CLASS_PATH],
        (0.0, 0.0, 0.0),
        (0.0, 0.0, 0.0),
        label,
    )
    descriptor.set_editor_property("world_catalog", catalog)
    descriptor.set_editor_property("planet_id", PLANET_ID)
    descriptor.set_editor_property("region_id", region_id)
    return descriptor


def spawn_regional_start(
    actor_subsystem: unreal.EditorActorSubsystem,
    label: str,
    anchor_id: str,
    angle_degrees: float,
    spawn_distance: float,
) -> unreal.Actor:
    spec = actor_surface_spec(
        label,
        angle_degrees,
        spawn_distance - PLANET_RADIUS_CM,
    )
    start = spawn_actor(
        actor_subsystem,
        NATIVE_CLASSES[REGION_PLAYER_START_CLASS_PATH],
        spec["location"],
        spec["rotation"],
        label,
    )
    start.set_editor_property("spawn_anchor_id", anchor_id)
    return start


def spawn_region_portal(
    actor_subsystem: unreal.EditorActorSubsystem,
    label: str,
    portal_id: str,
    link_id: str,
    angle_degrees: float,
) -> unreal.Actor:
    spec = actor_surface_spec(label, angle_degrees, 0.0)
    portal = spawn_actor(
        actor_subsystem,
        NATIVE_CLASSES[REGION_PORTAL_CLASS_PATH],
        spec["location"],
        spec["rotation"],
        label,
    )
    portal.set_editor_property("portal_id", portal_id)
    portal.set_editor_property("travel_link_id", link_id)
    return portal


def populate_region_map(
    target_map: str,
    region_name: str,
    game_mode_class: unreal.Class,
    source_character_class: unreal.Class,
    catalog: unreal.Object,
) -> None:
    _, level_subsystem, actor_subsystem, _ = required_editor_subsystems()
    require(level_subsystem.load_level(target_map), f"Regional map load failed: {target_map}")
    world = current_world_checked(target_map)
    require(
        len(actor_subsystem.get_all_level_actors()) == 0,
        f"Skeleton map is not empty before population: {target_map}",
    )
    configure_skeleton_world(world, game_mode_class)

    character_defaults = unreal.get_default_object(source_character_class)
    require(character_defaults is not None, "Product Character defaults unavailable")
    capsule = character_defaults.get_editor_property("capsule_component")
    require(capsule is not None, "Product Character capsule unavailable")
    capsule_half_height = float(capsule.get_scaled_capsule_half_height())
    spawn_distance = PLANET_RADIUS_CM + capsule_half_height + SPAWN_CLEARANCE_CM

    if region_name == "Town":
        static_specs = TOWN_STATIC_SPECS
        spawn_planet_shell(actor_subsystem, spawn_distance, static_specs)
        spawn_descriptor(actor_subsystem, catalog, TOWN_REGION_ID, TOWN_DESCRIPTOR_LABEL)
        spawn_regional_start(
            actor_subsystem,
            TOWN_START_LABEL,
            TOWN_START_ANCHOR,
            TOWN_START_ANGLE,
            spawn_distance,
        )
        spawn_regional_start(
            actor_subsystem,
            TOWN_RETURN_LABEL,
            TOWN_FROM_FIELD_ANCHOR,
            TOWN_RETURN_ANGLE,
            spawn_distance,
        )
        spawn_region_portal(
            actor_subsystem,
            TOWN_PORTAL_LABEL,
            TOWN_PORTAL_ID,
            TOWN_TO_FIELD_LINK,
            TOWN_PORTAL_ANGLE,
        )
        expected_count = 14
    elif region_name == "Field":
        static_specs = FIELD_STATIC_SPECS
        spawn_planet_shell(actor_subsystem, spawn_distance, static_specs)
        spawn_descriptor(actor_subsystem, catalog, FIELD_REGION_ID, FIELD_DESCRIPTOR_LABEL)
        spawn_regional_start(
            actor_subsystem,
            FIELD_START_LABEL,
            FIELD_FROM_TOWN_ANCHOR,
            FIELD_ARRIVAL_ANGLE,
            spawn_distance,
        )
        spawn_region_portal(
            actor_subsystem,
            FIELD_PORTAL_LABEL,
            FIELD_PORTAL_ID,
            FIELD_TO_TOWN_LINK,
            FIELD_PORTAL_ANGLE,
        )
        expected_count = 13
    else:
        raise RuntimeError(f"Unsupported region population contract: {region_name}")

    actual_count = len(actor_subsystem.get_all_level_actors())
    require(
        actual_count == expected_count,
        f"{region_name} population cardinality mismatch: {actual_count} != {expected_count}",
    )
    require(level_subsystem.save_current_level(), f"Populated map save failed: {target_map}")
    info(
        "FIRST_PLANET_REGIONS_CREATE_MAP="
        f"map={target_map},region={region_name},actors={actual_count},"
        f"radius={PLANET_RADIUS_CM:.1f},capsuleHalf={capsule_half_height:.1f},"
        f"spawnDistance={spawn_distance:.1f}"
    )
    info(f"FIRST_PLANET_REGIONS_SAVE=asset={target_map},type=PopulatedMap,status=success")


def audit_actor_transform(
    actor: unreal.Actor,
    expected: dict[str, object],
    label: str,
    scope: str,
    issues: list[dict[str, object]],
) -> None:
    actual_location = actor.get_actor_location()
    expected_location = expected["location"]
    audit_expect(
        issues,
        vector_distance(actual_location, expected_location) <= 0.1,
        scope,
        f"actor-location:{label}",
        vector_json(actual_location),
        expected_location,
    )
    actual_rotation = actor.get_actor_rotation()
    expected_rotation = expected["rotation"]
    audit_expect(
        issues,
        rotator_nearly_equal(actual_rotation, expected_rotation, 1.0e-3),
        scope,
        f"actor-rotation:{label}",
        rotator_json(actual_rotation),
        expected_rotation,
    )
    actual_scale = actor.get_actor_scale3d()
    expected_scale = expected["scale"]
    audit_expect(
        issues,
        tuple_nearly_equal(actual_scale, expected_scale, 1.0e-4),
        scope,
        f"actor-scale:{label}",
        vector_json(actual_scale),
        expected_scale,
    )
    actual_forward = actor.get_actor_forward_vector()
    expected_forward = expected["forward"]
    audit_expect(
        issues,
        vector_dot(actual_forward, expected_forward) >= 0.99999,
        scope,
        f"actor-forward:{label}",
        vector_json(actual_forward),
        expected_forward,
    )
    actual_up = actor.get_actor_up_vector()
    expected_up = expected["up"]
    audit_expect(
        issues,
        vector_dot(actual_up, expected_up) >= 0.99999,
        scope,
        f"actor-up:{label}",
        vector_json(actual_up),
        expected_up,
    )


def audit_static_actor(
    actor: unreal.Actor,
    spec: dict[str, object],
    scope: str,
    issues: list[dict[str, object]],
) -> None:
    label = str(spec["label"])
    actual_class_path = class_path(actor)
    class_ok = audit_expect(
        issues,
        actual_class_path == STATIC_MESH_ACTOR_CLASS_PATH,
        scope,
        f"static-class:{label}",
        actual_class_path,
        STATIC_MESH_ACTOR_CLASS_PATH,
    )
    audit_actor_transform(actor, spec, label, scope, issues)
    if not class_ok:
        return
    component = actor.get_editor_property("static_mesh_component")
    if not audit_expect(
        issues,
        component is not None,
        scope,
        f"static-component:{label}",
        component is not None,
        True,
    ):
        return
    actual_mesh_path = object_path(component.get_editor_property("static_mesh"))
    expected_mesh_path = canonical_asset_object_path(str(spec["mesh_path"]))
    audit_expect(
        issues,
        actual_mesh_path == expected_mesh_path,
        scope,
        f"static-mesh:{label}",
        actual_mesh_path,
        expected_mesh_path,
    )
    actual_material_path = object_path(component.get_material(0))
    expected_material_path = canonical_asset_object_path(str(spec["material_path"]))
    audit_expect(
        issues,
        actual_material_path == expected_material_path,
        scope,
        f"static-material:{label}",
        actual_material_path,
        expected_material_path,
    )
    actual_collision_profile = str(component.get_collision_profile_name())
    expected_collision_profile = str(spec["collision_profile"])
    audit_expect(
        issues,
        actual_collision_profile == expected_collision_profile,
        scope,
        f"static-collision-profile:{label}",
        actual_collision_profile,
        expected_collision_profile,
    )
    actual_mobility = component.get_editor_property("mobility")
    audit_expect(
        issues,
        actual_mobility == unreal.ComponentMobility.STATIC,
        scope,
        f"static-mobility:{label}",
        str(actual_mobility),
        str(unreal.ComponentMobility.STATIC),
    )


def audit_planet(
    by_label: dict[str, unreal.Actor],
    scope: str,
    issues: list[dict[str, object]],
) -> None:
    planet = by_label.get("PlanetBody_0")
    if planet is None:
        return
    actual_class_path = class_path(planet)
    class_ok = audit_expect(
        issues,
        actual_class_path == PLANET_BODY_CLASS_PATH,
        scope,
        "planet-class",
        actual_class_path,
        PLANET_BODY_CLASS_PATH,
    )
    actual_location = planet.get_actor_location()
    audit_expect(
        issues,
        vector_length(actual_location) <= 0.1,
        scope,
        "planet-location",
        vector_json(actual_location),
        [0.0, 0.0, 0.0],
    )
    actual_scale = planet.get_actor_scale3d()
    audit_expect(
        issues,
        tuple_nearly_equal(actual_scale, (1.0, 1.0, 1.0), 1.0e-4),
        scope,
        "planet-actor-scale",
        vector_json(actual_scale),
        [1.0, 1.0, 1.0],
    )
    if not class_ok:
        return
    actual_radius = float(planet.get_surface_radius())
    audit_expect(
        issues,
        math.isclose(actual_radius, PLANET_RADIUS_CM, abs_tol=1.0e-4),
        scope,
        "planet-surface-radius",
        actual_radius,
        PLANET_RADIUS_CM,
    )
    planet_mesh = planet.get_editor_property("planet_mesh")
    if not audit_expect(
        issues,
        planet_mesh is not None,
        scope,
        "planet-mesh-component",
        planet_mesh is not None,
        True,
    ):
        return
    actual_mesh_path = object_path(planet_mesh.get_editor_property("static_mesh"))
    expected_mesh_path = canonical_asset_object_path(SPHERE_MESH)
    audit_expect(
        issues,
        actual_mesh_path == expected_mesh_path,
        scope,
        "planet-static-mesh",
        actual_mesh_path,
        expected_mesh_path,
    )
    expected_scale = PLANET_RADIUS_CM / ENGINE_SPHERE_RADIUS_CM
    actual_mesh_scale = planet_mesh.get_editor_property("relative_scale3d")
    expected_mesh_scale = (expected_scale, expected_scale, expected_scale)
    audit_expect(
        issues,
        tuple_nearly_equal(actual_mesh_scale, expected_mesh_scale, 1.0e-4),
        scope,
        "planet-mesh-relative-scale",
        vector_json(actual_mesh_scale),
        expected_mesh_scale,
    )
    actual_material_path = object_path(planet_mesh.get_material(0))
    expected_material_path = canonical_asset_object_path(PLANET_MATERIAL)
    audit_expect(
        issues,
        actual_material_path == expected_material_path,
        scope,
        "planet-material",
        actual_material_path,
        expected_material_path,
    )
    actual_profile = str(planet_mesh.get_collision_profile_name())
    audit_expect(
        issues,
        actual_profile == "BlockAll",
        scope,
        "planet-collision-profile",
        actual_profile,
        "BlockAll",
    )
    actual_collision = planet_mesh.get_collision_enabled()
    audit_expect(
        issues,
        actual_collision == unreal.CollisionEnabled.QUERY_AND_PHYSICS,
        scope,
        "planet-collision-enabled",
        str(actual_collision),
        str(unreal.CollisionEnabled.QUERY_AND_PHYSICS),
    )
    actual_overlap = bool(planet_mesh.get_editor_property("generate_overlap_events"))
    audit_expect(
        issues,
        not actual_overlap,
        scope,
        "planet-generate-overlap-events",
        actual_overlap,
        False,
    )


def audit_lighting(
    by_label: dict[str, unreal.Actor],
    spawn_distance: float | None,
    scope: str,
    issues: list[dict[str, object]],
) -> None:
    light_contracts = (
        (
            "DirectionalLight_Main",
            None if spawn_distance is None else (0.0, 0.0, spawn_distance + 1000.0),
            MAIN_LIGHT_ROTATION,
            5.0,
            True,
            MAIN_FORWARD_SHADING_PRIORITY,
        ),
        (
            "DirectionalLight_Fill",
            None if spawn_distance is None else (0.0, 0.0, -spawn_distance - 1000.0),
            FILL_LIGHT_ROTATION,
            1.5,
            False,
            FILL_FORWARD_SHADING_PRIORITY,
        ),
    )
    for label, expected_location, expected_rotation, expected_intensity, expected_sun, expected_priority in light_contracts:
        light = by_label.get(label)
        if light is None:
            continue
        actual_class_path = class_path(light)
        class_ok = audit_expect(
            issues,
            actual_class_path == DIRECTIONAL_LIGHT_CLASS_PATH,
            scope,
            f"light-class:{label}",
            actual_class_path,
            DIRECTIONAL_LIGHT_CLASS_PATH,
        )
        if expected_location is not None:
            actual_location = light.get_actor_location()
            audit_expect(
                issues,
                vector_distance(actual_location, expected_location) <= 0.1,
                scope,
                f"light-location:{label}",
                vector_json(actual_location),
                expected_location,
            )
        actual_rotation = light.get_actor_rotation()
        audit_expect(
            issues,
            rotator_nearly_equal(actual_rotation, expected_rotation, 1.0e-3),
            scope,
            f"light-rotation:{label}",
            rotator_json(actual_rotation),
            expected_rotation,
        )
        # A*Light constructors intentionally scale their editor sprite/root to
        # 2.5. That value is neither authored here nor gameplay state, so actor
        # scale is deliberately outside the regional-map audit contract.
        if not class_ok:
            continue
        component = light.get_editor_property("directional_light_component")
        if not audit_expect(
            issues,
            component is not None,
            scope,
            f"light-component:{label}",
            component is not None,
            True,
        ):
            continue
        actual_mobility = component.get_editor_property("mobility")
        audit_expect(
            issues,
            actual_mobility == unreal.ComponentMobility.MOVABLE,
            scope,
            f"light-mobility:{label}",
            str(actual_mobility),
            str(unreal.ComponentMobility.MOVABLE),
        )
        actual_intensity = float(component.get_editor_property("intensity"))
        audit_expect(
            issues,
            math.isclose(actual_intensity, expected_intensity, abs_tol=1.0e-4),
            scope,
            f"light-intensity:{label}",
            actual_intensity,
            expected_intensity,
        )
        actual_sun = bool(component.get_editor_property("atmosphere_sun_light"))
        audit_expect(
            issues,
            actual_sun is expected_sun,
            scope,
            f"light-atmosphere-sun:{label}",
            actual_sun,
            expected_sun,
        )
        actual_priority = int(component.get_editor_property("forward_shading_priority"))
        audit_expect(
            issues,
            actual_priority == expected_priority,
            scope,
            f"light-forward-shading-priority:{label}",
            actual_priority,
            expected_priority,
        )

    sky = by_label.get("SkyLight_0")
    if sky is not None:
        actual_class_path = class_path(sky)
        class_ok = audit_expect(
            issues,
            actual_class_path == SKY_LIGHT_CLASS_PATH,
            scope,
            "sky-light-class",
            actual_class_path,
            SKY_LIGHT_CLASS_PATH,
        )
        if spawn_distance is not None:
            actual_location = sky.get_actor_location()
            expected_location = (0.0, 0.0, spawn_distance + 500.0)
            audit_expect(
                issues,
                vector_distance(actual_location, expected_location) <= 0.1,
                scope,
                "sky-light-location",
                vector_json(actual_location),
                expected_location,
            )
        # SkyLight actor scale is not authored and has no regional gameplay meaning.
        if class_ok:
            sky_component = sky.get_editor_property("light_component")
            if audit_expect(
                issues,
                sky_component is not None,
                scope,
                "sky-light-component",
                sky_component is not None,
                True,
            ):
                actual_mobility = sky_component.get_editor_property("mobility")
                audit_expect(
                    issues,
                    actual_mobility == unreal.ComponentMobility.MOVABLE,
                    scope,
                    "sky-light-mobility",
                    str(actual_mobility),
                    str(unreal.ComponentMobility.MOVABLE),
                )
                actual_capture = bool(
                    sky_component.get_editor_property("real_time_capture")
                )
                audit_expect(
                    issues,
                    actual_capture,
                    scope,
                    "sky-light-real-time-capture",
                    actual_capture,
                    True,
                )
                actual_intensity = float(
                    sky_component.get_editor_property("intensity")
                )
                audit_expect(
                    issues,
                    math.isclose(actual_intensity, 1.5, abs_tol=1.0e-4),
                    scope,
                    "sky-light-intensity",
                    actual_intensity,
                    1.5,
                )

    atmosphere = by_label.get("SkyAtmosphere_0")
    if atmosphere is not None:
        actual_class_path = class_path(atmosphere)
        class_ok = audit_expect(
            issues,
            actual_class_path == SKY_ATMOSPHERE_CLASS_PATH,
            scope,
            "sky-atmosphere-class",
            actual_class_path,
            SKY_ATMOSPHERE_CLASS_PATH,
        )
        actual_location = atmosphere.get_actor_location()
        audit_expect(
            issues,
            vector_length(actual_location) <= 0.1,
            scope,
            "sky-atmosphere-center",
            vector_json(actual_location),
            [0.0, 0.0, 0.0],
        )
        # Transform mode uses translation for the planet center; actor scale is
        # neither authored nor meaningful to this atmosphere contract.
        if class_ok:
            atmosphere_component = atmosphere.get_editor_property(
                "sky_atmosphere_component"
            )
            if audit_expect(
                issues,
                atmosphere_component is not None,
                scope,
                "sky-atmosphere-component",
                atmosphere_component is not None,
                True,
            ):
                actual_mode = atmosphere_component.get_editor_property(
                    "transform_mode"
                )
                expected_mode = (
                    unreal.SkyAtmosphereTransformMode.PLANET_CENTER_AT_COMPONENT_TRANSFORM
                )
                audit_expect(
                    issues,
                    actual_mode == expected_mode,
                    scope,
                    "sky-atmosphere-transform-mode",
                    str(actual_mode),
                    str(expected_mode),
                )
                actual_bottom_radius = float(
                    atmosphere_component.get_editor_property("bottom_radius")
                )
                audit_expect(
                    issues,
                    math.isclose(
                        actual_bottom_radius,
                        ATMOSPHERE_BOTTOM_RADIUS_KM,
                        abs_tol=1.0e-4,
                    ),
                    scope,
                    "sky-atmosphere-bottom-radius",
                    actual_bottom_radius,
                    ATMOSPHERE_BOTTOM_RADIUS_KM,
                )
                actual_height = float(
                    atmosphere_component.get_editor_property("atmosphere_height")
                )
                audit_expect(
                    issues,
                    math.isclose(
                        actual_height,
                        ATMOSPHERE_HEIGHT_KM,
                        abs_tol=1.0e-4,
                    ),
                    scope,
                    "sky-atmosphere-height",
                    actual_height,
                    ATMOSPHERE_HEIGHT_KM,
                )


def subclass_count(actors: list[unreal.Actor], class_path_name: str) -> int:
    expected_class = NATIVE_CLASSES[class_path_name]
    return sum(
        1
        for actor in actors
        if class_is_child_of(actor.get_class(), expected_class)
    )


def central_arc_distance_cm(
    left: unreal.Vector,
    right: unreal.Vector,
) -> float | None:
    left_length = vector_length(left)
    right_length = vector_length(right)
    if left_length <= 1.0 or right_length <= 1.0:
        return None
    cosine = (
        float(left.x) * float(right.x)
        + float(left.y) * float(right.y)
        + float(left.z) * float(right.z)
    ) / (left_length * right_length)
    cosine = max(-1.0, min(1.0, cosine))
    return PLANET_RADIUS_CM * math.acos(cosine)


def audit_portal_components(
    portal: unreal.Actor,
    label: str,
    scope: str,
    issues: list[dict[str, object]],
) -> None:
    visual = portal.get_editor_property("portal_visual")
    trigger = portal.get_editor_property("trigger")
    visual_ready = audit_expect(
        issues,
        visual is not None,
        scope,
        f"portal-visual-component:{label}",
        visual is not None,
        True,
    )
    trigger_ready = audit_expect(
        issues,
        trigger is not None,
        scope,
        f"portal-trigger-component:{label}",
        trigger is not None,
        True,
    )
    if visual_ready:
        actual_mesh_path = object_path(visual.get_editor_property("static_mesh"))
        expected_mesh_path = canonical_asset_object_path(CUBE_MESH)
        audit_expect(
            issues,
            actual_mesh_path == expected_mesh_path,
            scope,
            f"portal-visual-mesh:{label}",
            actual_mesh_path,
            expected_mesh_path,
        )
        actual_collision = visual.get_collision_enabled()
        audit_expect(
            issues,
            actual_collision == unreal.CollisionEnabled.NO_COLLISION,
            scope,
            f"portal-visual-collision:{label}",
            str(actual_collision),
            str(unreal.CollisionEnabled.NO_COLLISION),
        )
        actual_location = visual.get_editor_property("relative_location")
        audit_expect(
            issues,
            tuple_nearly_equal(actual_location, (0.0, 0.0, 200.0), 1.0e-4),
            scope,
            f"portal-visual-relative-location:{label}",
            vector_json(actual_location),
            [0.0, 0.0, 200.0],
        )
        actual_scale = visual.get_editor_property("relative_scale3d")
        audit_expect(
            issues,
            tuple_nearly_equal(actual_scale, (0.35, 2.5, 4.0), 1.0e-4),
            scope,
            f"portal-visual-relative-scale:{label}",
            vector_json(actual_scale),
            [0.35, 2.5, 4.0],
        )
    if not trigger_ready:
        return
    actual_trigger_collision = trigger.get_collision_enabled()
    audit_expect(
        issues,
        actual_trigger_collision == unreal.CollisionEnabled.QUERY_ONLY,
        scope,
        f"portal-trigger-collision:{label}",
        str(actual_trigger_collision),
        str(unreal.CollisionEnabled.QUERY_ONLY),
    )
    actual_overlap = bool(trigger.get_editor_property("generate_overlap_events"))
    audit_expect(
        issues,
        actual_overlap,
        scope,
        f"portal-trigger-overlap-events:{label}",
        actual_overlap,
        True,
    )
    actual_trigger_location = trigger.get_editor_property("relative_location")
    audit_expect(
        issues,
        tuple_nearly_equal(actual_trigger_location, (0.0, 0.0, 200.0), 1.0e-4),
        scope,
        f"portal-trigger-relative-location:{label}",
        vector_json(actual_trigger_location),
        [0.0, 0.0, 200.0],
    )
    actual_extent = trigger.get_unscaled_box_extent()
    audit_expect(
        issues,
        tuple_nearly_equal(actual_extent, (100.0, 300.0, 400.0), 1.0e-4),
        scope,
        f"portal-trigger-box-extent:{label}",
        vector_json(actual_extent),
        [100.0, 300.0, 400.0],
    )
    actual_object_type = trigger.get_collision_object_type()
    audit_expect(
        issues,
        actual_object_type == unreal.CollisionChannel.ECC_WORLD_DYNAMIC,
        scope,
        f"portal-trigger-object-type:{label}",
        str(actual_object_type),
        str(unreal.CollisionChannel.ECC_WORLD_DYNAMIC),
    )

    pawn_channel = unreal.CollisionChannel.ECC_PAWN
    actual_pawn_response = trigger.get_collision_response_to_channel(pawn_channel)
    audit_expect(
        issues,
        actual_pawn_response == unreal.CollisionResponseType.ECR_OVERLAP,
        scope,
        f"portal-trigger-response:{label}:{pawn_channel}",
        str(actual_pawn_response),
        str(unreal.CollisionResponseType.ECR_OVERLAP),
    )
    non_pawn_channels = (
        unreal.CollisionChannel.ECC_WORLD_STATIC,
        unreal.CollisionChannel.ECC_WORLD_DYNAMIC,
        unreal.CollisionChannel.ECC_VISIBILITY,
        unreal.CollisionChannel.ECC_CAMERA,
        unreal.CollisionChannel.ECC_PHYSICS_BODY,
        unreal.CollisionChannel.ECC_VEHICLE,
        unreal.CollisionChannel.ECC_DESTRUCTIBLE,
    )
    for channel in non_pawn_channels:
        actual_response = trigger.get_collision_response_to_channel(channel)
        audit_expect(
            issues,
            actual_response == unreal.CollisionResponseType.ECR_IGNORE,
            scope,
            f"portal-trigger-response:{label}:{channel}",
            str(actual_response),
            str(unreal.CollisionResponseType.ECR_IGNORE),
        )


def audit_region_map(
    target_map: str,
    region_name: str,
    source_character_class: unreal.Class,
    issues: list[dict[str, object]],
) -> None:
    scope = f"{region_name}Map"
    issue_start = len(issues)
    if region_name == "Town":
        expected_labels = TOWN_EXPECTED_LABELS
        expected_count = 14
        static_specs = TOWN_STATIC_SPECS
        descriptor_label = TOWN_DESCRIPTOR_LABEL
        region_id = TOWN_REGION_ID
        start_contracts = (
            (TOWN_START_LABEL, TOWN_START_ANCHOR, TOWN_START_ANGLE),
            (TOWN_RETURN_LABEL, TOWN_FROM_FIELD_ANCHOR, TOWN_RETURN_ANGLE),
        )
        portal_label = TOWN_PORTAL_LABEL
        portal_id = TOWN_PORTAL_ID
        link_id = TOWN_TO_FIELD_LINK
        portal_angle = TOWN_PORTAL_ANGLE
    elif region_name == "Field":
        expected_labels = FIELD_EXPECTED_LABELS
        expected_count = 13
        static_specs = FIELD_STATIC_SPECS
        descriptor_label = FIELD_DESCRIPTOR_LABEL
        region_id = FIELD_REGION_ID
        start_contracts = (
            (FIELD_START_LABEL, FIELD_FROM_TOWN_ANCHOR, FIELD_ARRIVAL_ANGLE),
        )
        portal_label = FIELD_PORTAL_LABEL
        portal_id = FIELD_PORTAL_ID
        link_id = FIELD_TO_TOWN_LINK
        portal_angle = FIELD_PORTAL_ANGLE
    else:
        audit_issue(
            issues,
            scope,
            "region-contract",
            region_name,
            ["Town", "Field"],
        )
        return

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    level_ready = audit_expect(
        issues,
        level_subsystem is not None,
        scope,
        "level-editor-subsystem",
        level_subsystem is not None,
        True,
    )
    actor_ready = audit_expect(
        issues,
        actor_subsystem is not None,
        scope,
        "editor-actor-subsystem",
        actor_subsystem is not None,
        True,
    )
    editor_ready = audit_expect(
        issues,
        editor_subsystem is not None,
        scope,
        "unreal-editor-subsystem",
        editor_subsystem is not None,
        True,
    )
    if not level_ready or not editor_ready:
        return

    # Python exceptions and ordinary false returns are collected below. A
    # native fatal/crash inside load_level terminates the process and is outside
    # any Python collector's control.
    try:
        loaded = bool(level_subsystem.load_level(target_map))
    except Exception as exc:
        load_exception = audit_exception_text(exc)
        del exc
        audit_issue(
            issues,
            scope,
            "map-load-exception",
            load_exception,
            "no exception",
        )
        return
    if not audit_expect(
        issues,
        loaded,
        scope,
        "map-load",
        loaded,
        True,
    ):
        return

    world = editor_subsystem.get_editor_world()
    if not audit_expect(
        issues,
        world is not None,
        scope,
        "editor-world",
        world is not None,
        True,
    ):
        return
    actual_world_package = str(world.get_outermost().get_name())
    if not audit_expect(
        issues,
        actual_world_package == target_map,
        scope,
        "editor-world-package",
        actual_world_package,
        target_map,
    ):
        return

    world_settings = world.get_world_settings()
    if audit_expect(
        issues,
        world_settings is not None,
        scope,
        "world-settings",
        world_settings is not None,
        True,
    ):
        actual_game_mode_path = object_path(
            world_settings.get_editor_property("default_game_mode")
        )
        expected_game_mode_path = canonical_blueprint_class_path(
            TARGET_REGIONAL_GAME_MODE
        )
        audit_expect(
            issues,
            actual_game_mode_path == expected_game_mode_path,
            scope,
            "world-settings-game-mode-class-path",
            actual_game_mode_path,
            expected_game_mode_path,
        )
        actual_kill_z = float(world_settings.get_editor_property("kill_z"))
        audit_expect(
            issues,
            math.isclose(actual_kill_z, KILL_Z_CM, abs_tol=0.1),
            scope,
            "world-settings-kill-z",
            actual_kill_z,
            KILL_Z_CM,
        )

    if not actor_ready:
        return
    raw_actors = list(actor_subsystem.get_all_level_actors())
    null_actor_count = sum(1 for actor in raw_actors if actor is None)
    audit_expect(
        issues,
        null_actor_count == 0,
        scope,
        "null-actor-count",
        null_actor_count,
        0,
    )
    actors = [actor for actor in raw_actors if actor is not None]
    labels = [actor.get_actor_label() for actor in actors]
    label_counts: dict[str, int] = {}
    by_label: dict[str, unreal.Actor] = {}
    for actor, label in zip(actors, labels):
        label_counts[label] = label_counts.get(label, 0) + 1
        by_label[label] = actor
    duplicate_labels = sorted(
        label for label, count in label_counts.items() if count > 1
    )
    audit_expect(
        issues,
        not duplicate_labels,
        scope,
        "duplicate-actor-labels",
        duplicate_labels,
        [],
    )
    audit_expect(
        issues,
        len(actors) == expected_count,
        scope,
        "actor-count",
        len(actors),
        expected_count,
    )
    actual_label_set = set(label_counts)
    audit_expect(
        issues,
        actual_label_set == expected_labels,
        scope,
        "actor-labels",
        sorted(actual_label_set),
        sorted(expected_labels),
    )
    for duplicate_label in duplicate_labels:
        by_label.pop(duplicate_label, None)

    spawn_distance: float | None = None
    character_defaults = unreal.get_default_object(source_character_class)
    defaults_ready = audit_expect(
        issues,
        character_defaults is not None,
        scope,
        "character-default-object",
        character_defaults is not None,
        True,
    )
    if defaults_ready:
        capsule = character_defaults.get_editor_property("capsule_component")
        capsule_ready = audit_expect(
            issues,
            capsule is not None,
            scope,
            "character-capsule-component",
            capsule is not None,
            True,
        )
        if capsule_ready:
            capsule_half_height = float(capsule.get_scaled_capsule_half_height())
            spawn_distance = (
                PLANET_RADIUS_CM + capsule_half_height + SPAWN_CLEARANCE_CM
            )

    expected_cardinalities = {
        PLANET_BODY_CLASS_PATH: 1,
        REGION_DESCRIPTOR_CLASS_PATH: 1,
        REGION_PLAYER_START_CLASS_PATH: len(start_contracts),
        REGION_PORTAL_CLASS_PATH: 1,
        STATIC_MESH_ACTOR_CLASS_PATH: 5,
        DIRECTIONAL_LIGHT_CLASS_PATH: 2,
        SKY_LIGHT_CLASS_PATH: 1,
        SKY_ATMOSPHERE_CLASS_PATH: 1,
    }
    for expected_class_path, expected_class_count in expected_cardinalities.items():
        actual_count = subclass_count(actors, expected_class_path)
        audit_expect(
            issues,
            actual_count == expected_class_count,
            scope,
            f"subclass-count:{expected_class_path}",
            actual_count,
            expected_class_count,
        )
    generic_start_count = sum(
        1 for actor in actors if class_path(actor) == GENERIC_PLAYER_START_CLASS_PATH
    )
    player_start_super_count = subclass_count(actors, GENERIC_PLAYER_START_CLASS_PATH)
    audit_expect(
        issues,
        generic_start_count == 0,
        scope,
        "ordinary-player-start-count",
        generic_start_count,
        0,
    )
    audit_expect(
        issues,
        player_start_super_count == len(start_contracts),
        scope,
        "player-start-superclass-count",
        player_start_super_count,
        len(start_contracts),
    )

    audit_planet(by_label, scope, issues)
    audit_lighting(by_label, spawn_distance, scope, issues)
    for spec in static_specs:
        label = str(spec["label"])
        static_actor = by_label.get(label)
        if static_actor is not None:
            audit_static_actor(static_actor, spec, scope, issues)

    descriptor = by_label.get(descriptor_label)
    if descriptor is not None:
        actual_descriptor_class = class_path(descriptor)
        descriptor_class_ok = audit_expect(
            issues,
            actual_descriptor_class == REGION_DESCRIPTOR_CLASS_PATH,
            scope,
            "descriptor-class",
            actual_descriptor_class,
            REGION_DESCRIPTOR_CLASS_PATH,
        )
        # The descriptor is a data-only anchor. Its transform is neither
        # explicitly authored nor used by regional gameplay and is not audited.
        if descriptor_class_ok:
            actual_catalog_path = object_path(
                descriptor.get_editor_property("world_catalog")
            )
            expected_catalog_path = canonical_asset_object_path(TARGET_CATALOG)
            audit_expect(
                issues,
                actual_catalog_path == expected_catalog_path,
                scope,
                "descriptor-catalog-path",
                actual_catalog_path,
                expected_catalog_path,
            )
            actual_planet_id = str(descriptor.get_editor_property("planet_id"))
            audit_expect(
                issues,
                actual_planet_id == PLANET_ID,
                scope,
                "descriptor-planet-id",
                actual_planet_id,
                PLANET_ID,
            )
            actual_region_id = str(descriptor.get_editor_property("region_id"))
            audit_expect(
                issues,
                actual_region_id == region_id,
                scope,
                "descriptor-region-id",
                actual_region_id,
                region_id,
            )

    start_actors: list[unreal.Actor] = []
    for label, anchor_id, angle in start_contracts:
        start = by_label.get(label)
        if start is None:
            continue
        start_actors.append(start)
        actual_start_class = class_path(start)
        start_class_ok = audit_expect(
            issues,
            actual_start_class == REGION_PLAYER_START_CLASS_PATH,
            scope,
            f"regional-start-class:{label}",
            actual_start_class,
            REGION_PLAYER_START_CLASS_PATH,
        )
        if start_class_ok:
            actual_anchor_id = str(start.get_editor_property("spawn_anchor_id"))
            audit_expect(
                issues,
                actual_anchor_id == anchor_id,
                scope,
                f"regional-start-anchor-id:{label}",
                actual_anchor_id,
                anchor_id,
            )
        if spawn_distance is not None:
            start_spec = actor_surface_spec(
                label,
                angle,
                spawn_distance - PLANET_RADIUS_CM,
            )
            audit_actor_transform(start, start_spec, label, scope, issues)

    portal = by_label.get(portal_label)
    portal_arc_distances: list[float] = []
    if portal is not None:
        actual_portal_class = class_path(portal)
        portal_class_ok = audit_expect(
            issues,
            actual_portal_class == REGION_PORTAL_CLASS_PATH,
            scope,
            "portal-class",
            actual_portal_class,
            REGION_PORTAL_CLASS_PATH,
        )
        if portal_class_ok:
            actual_portal_id = str(portal.get_editor_property("portal_id"))
            audit_expect(
                issues,
                actual_portal_id == portal_id,
                scope,
                "portal-id",
                actual_portal_id,
                portal_id,
            )
            actual_link_id = str(portal.get_editor_property("travel_link_id"))
            audit_expect(
                issues,
                actual_link_id == link_id,
                scope,
                "portal-travel-link-id",
                actual_link_id,
                link_id,
            )
        portal_spec = actor_surface_spec(portal_label, portal_angle, 0.0)
        audit_actor_transform(portal, portal_spec, portal_label, scope, issues)
        if portal_class_ok:
            audit_portal_components(portal, portal_label, scope, issues)

        portal_location = portal.get_actor_location()
        for start in start_actors:
            start_label = start.get_actor_label()
            start_location = start.get_actor_location()
            arc_distance = central_arc_distance_cm(portal_location, start_location)
            if arc_distance is None:
                audit_issue(
                    issues,
                    scope,
                    f"portal-anchor-arc:{start_label}",
                    "undefined-at-planet-center",
                    f">={MIN_PORTAL_ANCHOR_ARC_CM}",
                )
            else:
                portal_arc_distances.append(arc_distance)
                audit_expect(
                    issues,
                    arc_distance >= MIN_PORTAL_ANCHOR_ARC_CM,
                    scope,
                    f"portal-anchor-arc:{start_label}",
                    arc_distance,
                    f">={MIN_PORTAL_ANCHOR_ARC_CM}",
                )
            euclidean_distance = math.dist(
                vector_json(portal_location),
                vector_json(start_location),
            )
            audit_expect(
                issues,
                euclidean_distance >= MIN_PORTAL_ANCHOR_ARC_CM,
                scope,
                f"portal-anchor-euclidean:{start_label}",
                euclidean_distance,
                f">={MIN_PORTAL_ANCHOR_ARC_CM}",
            )

    if len(issues) == issue_start:
        info(
            "FIRST_PLANET_REGIONS_AUDIT_MAP="
            f"map={target_map},region={PLANET_ID}/{region_id},actors={len(actors)},"
            f"shell=10,descriptor=1,regionalStarts={len(start_contracts)},portals=1,"
            f"static=5,ordinaryPlayerStarts={generic_start_count},"
            f"playerStartSubclassCount={player_start_super_count},"
            f"portalAnchorArcMin={min(portal_arc_distances):.3f}cm,"
            "worldSettingsGameMode=exact-child,transforms=exact"
        )


def exact_reload_audit(
    asset_subsystem: unreal.EditorAssetSubsystem,
    source_character_class: unreal.Class,
    source_game_mode_class: unreal.Class,
) -> None:
    issues: list[dict[str, object]] = []

    try:
        collect_regional_game_mode_audit(
            asset_subsystem,
            source_character_class,
            source_game_mode_class,
            issues,
        )
    except Exception as exc:
        exception_text = audit_exception_text(exc)
        del exc
        audit_issue(
            issues,
            "GameMode",
            "audit-python-exception",
            exception_text,
            "no exception",
        )

    try:
        audit_catalog(asset_subsystem, issues)
    except Exception as exc:
        exception_text = audit_exception_text(exc)
        del exc
        audit_issue(
            issues,
            "Catalog",
            "audit-python-exception",
            exception_text,
            "no exception",
        )

    for target_map, region_name in (
        (TARGET_TOWN_MAP, "Town"),
        (TARGET_FIELD_MAP, "Field"),
    ):
        try:
            audit_region_map(
                target_map,
                region_name,
                source_character_class,
                issues,
            )
        except Exception as exc:
            exception_text = audit_exception_text(exc)
            del exc
            audit_issue(
                issues,
                f"{region_name}Map",
                "audit-python-exception",
                exception_text,
                "no exception",
            )

    if issues:
        scope_counts: dict[str, int] = {}
        for issue in issues:
            issue_scope = str(issue["scope"])
            scope_counts[issue_scope] = scope_counts.get(issue_scope, 0) + 1
            info(
                "FIRST_PLANET_REGIONS_AUDIT_MISMATCH="
                + json.dumps(
                    issue,
                    ensure_ascii=True,
                    allow_nan=False,
                    sort_keys=True,
                    separators=(",", ":"),
                )
            )
        failure_summary = {
            "issueCount": len(issues),
            "scopeCounts": scope_counts,
        }
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_AUDIT_FAILURE="
            + json.dumps(
                audit_json_value(failure_summary),
                ensure_ascii=True,
                allow_nan=False,
                sort_keys=True,
                separators=(",", ":"),
            )
        )
        raise RuntimeError(
            f"First Planet exact audit collected {len(issues)} mismatch(es)"
        )

    info(
        "FIRST_PLANET_REGIONS_AUDIT="
        "assets=4,catalog=exact,gameMode=exact,town=14,field=13,status=passed"
    )


def rollback_created_assets() -> None:
    if not CREATED_THIS_RUN:
        info("FIRST_PLANET_REGIONS_ROLLBACK=not-needed assets=none")
        return

    unsafe_candidates = [path for path in CREATED_THIS_RUN if path not in TARGET_ASSETS]
    if unsafe_candidates:
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=FAILED "
            f"reason=unsafe-candidates assets={unsafe_candidates}"
        )
        return

    # The caller releases the exception object and traceback before reaching
    # this function. Collect both Python and Unreal references first so stale
    # current-run wrappers cannot keep target packages alive.
    python_collected_before_release = gc.collect()
    unreal.collect_garbage()
    info(
        "FIRST_PLANET_REGIONS_ROLLBACK_GC="
        f"phase=before-world-release,pythonCollected={python_collected_before_release},"
        "unreal=complete"
    )

    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if asset_subsystem is None:
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=failed reason=no-asset-subsystem"
        )
        return

    # Release references from a current-run map before deleting only exact
    # targets.  Deletion is forbidden unless both the load result and the
    # resulting editor-world package prove that the current-run map is closed.
    remaining_text = ",".join(CREATED_THIS_RUN)
    if level_subsystem is None:
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK_RELEASE_FAILED="
            f"reason=no-level-subsystem remaining={remaining_text}"
        )
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=FAILED "
            f"reason=release-world-no-level-subsystem remaining={remaining_text}"
        )
        return
    if not asset_subsystem.does_asset_exist(SOURCE_GRAYBOX):
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK_RELEASE_FAILED="
            f"reason=source-graybox-missing remaining={remaining_text}"
        )
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=FAILED "
            f"reason=release-world-source-missing remaining={remaining_text}"
        )
        return
    try:
        released = bool(level_subsystem.load_level(SOURCE_GRAYBOX))
        if not released:
            unreal.log_error(
                "[EverPlanet First Planet Regions] "
                "FIRST_PLANET_REGIONS_ROLLBACK_RELEASE_FAILED="
                f"reason=load-returned-false remaining={remaining_text}"
            )
            unreal.log_error(
                "[EverPlanet First Planet Regions] "
                "FIRST_PLANET_REGIONS_ROLLBACK=FAILED "
                f"reason=release-world-load-false remaining={remaining_text}"
            )
            return
        current_world_checked(SOURCE_GRAYBOX)
    except Exception as exc:
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK_RELEASE_FAILED="
            f"reason=exception error={exc} remaining={remaining_text}"
        )
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=FAILED "
            f"reason=release-world-exception remaining={remaining_text}"
        )
        return
    info(
        "FIRST_PLANET_REGIONS_ROLLBACK_RELEASE_WORLD="
        f"success map={SOURCE_GRAYBOX}"
    )

    # The protected Graybox is now the exact current editor World. A second
    # synchronous collection releases the former current-run World/package
    # before any exact-target delete request.
    python_collected_after_release = gc.collect()
    unreal.collect_garbage()
    info(
        "FIRST_PLANET_REGIONS_ROLLBACK_GC="
        f"phase=after-world-release,pythonCollected={python_collected_after_release},"
        "unreal=complete"
    )

    failures: list[str] = []
    for asset_path in reversed(CREATED_THIS_RUN):
        registry_present_before, disk_present_before, package_file = target_package_presence(
            asset_subsystem,
            asset_path,
        )
        if not registry_present_before and not disk_present_before:
            info(
                "FIRST_PLANET_REGIONS_ROLLBACK_ASSET=already-missing "
                f"asset={asset_path},registryPresent=false,diskPresent=false"
            )
            continue
        if not registry_present_before or not disk_present_before:
            failures.append(f"pre-delete-presence-disagreement:{asset_path}")
            unreal.log_error(
                "[EverPlanet First Planet Regions] "
                "FIRST_PLANET_REGIONS_ROLLBACK_ASSET=refused-presence-disagreement "
                f"asset={asset_path},registryPresent={str(registry_present_before).lower()},"
                f"diskPresent={str(disk_present_before).lower()},file={package_file}"
            )
            continue
        deleted = bool(asset_subsystem.delete_asset(asset_path))
        registry_present_after, disk_present_after, _ = target_package_presence(
            asset_subsystem,
            asset_path,
        )
        info(
            "FIRST_PLANET_REGIONS_ROLLBACK_ASSET=requested "
            f"asset={asset_path},deleteReturn={str(deleted).lower()},"
            f"registryPresent={str(registry_present_after).lower()},"
            f"diskPresent={str(disk_present_after).lower()}"
        )
        if deleted and not registry_present_after and not disk_present_after:
            info(
                "FIRST_PLANET_REGIONS_ROLLBACK_ASSET=verified-absent "
                f"asset={asset_path},registryPresent=false,diskPresent=false"
            )
        else:
            failures.append(
                f"{asset_path}:deleteReturn={str(deleted).lower()}:"
                f"registryPresent={str(registry_present_after).lower()}:"
                f"diskPresent={str(disk_present_after).lower()}"
            )
            unreal.log_error(
                "[EverPlanet First Planet Regions] "
                "FIRST_PLANET_REGIONS_ROLLBACK_ASSET=failed "
                f"asset={asset_path},deleteReturn={str(deleted).lower()},"
                f"registryPresent={str(registry_present_after).lower()},"
                f"diskPresent={str(disk_present_after).lower()},file={package_file}"
            )

    remaining: list[str] = []
    for asset_path in CREATED_THIS_RUN:
        registry_present, disk_present, package_file = target_package_presence(
            asset_subsystem,
            asset_path,
        )
        if registry_present or disk_present:
            remaining.append(
                f"{asset_path}:registryPresent={str(registry_present).lower()}:"
                f"diskPresent={str(disk_present).lower()}:file={package_file}"
            )
    if failures or remaining:
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=FAILED "
            f"failures={failures},remaining={remaining}"
        )
    else:
        info(
            "FIRST_PLANET_REGIONS_ROLLBACK=complete assets="
            + ",".join(reversed(CREATED_THIS_RUN))
        )


def create_fresh_content(
    asset_subsystem: unreal.EditorAssetSubsystem,
    source_character_class: unreal.Class,
    source_game_mode_class: unreal.Class,
) -> None:
    for directory in (
        "/Game/EverPlanet/Product/World",
        "/Game/EverPlanet/Product/GameModes",
        "/Game/EverPlanet/Product/Planets/FirstPlanet/Maps",
    ):
        asset_subsystem.make_directory(directory)

    create_regional_game_mode(asset_subsystem, source_game_mode_class)
    regional_game_mode_class = audit_regional_game_mode(
        asset_subsystem,
        source_character_class,
        source_game_mode_class,
    )
    create_skeleton_map(TARGET_TOWN_MAP, regional_game_mode_class)
    create_skeleton_map(TARGET_FIELD_MAP, regional_game_mode_class)
    town_world = reload_saved_world_for_catalog(TARGET_TOWN_MAP)
    field_world = reload_saved_world_for_catalog(TARGET_FIELD_MAP)
    catalog = create_catalog(asset_subsystem, town_world, field_world)

    # TSoftObjectPtr<UWorld> keeps only the serialized paths in the Catalog,
    # but the two Python wrappers above are hard GC roots through
    # FPyReferenceCollector.  Release both before the first LoadLevel call:
    # retaining Town breaks the first transition, while retaining Field would
    # break the following Town -> Field transition.  Python GC must run first
    # so Unreal's collector no longer sees either wrapper.
    del town_world, field_world
    python_collected = gc.collect()
    unreal.collect_garbage()
    info(
        "FIRST_PLANET_REGIONS_RELEASE_CATALOG_WORLD_REFS="
        f"pythonCollected={python_collected},unreal=complete"
    )

    populate_region_map(
        TARGET_TOWN_MAP,
        "Town",
        regional_game_mode_class,
        source_character_class,
        catalog,
    )
    populate_region_map(
        TARGET_FIELD_MAP,
        "Field",
        regional_game_mode_class,
        source_character_class,
        catalog,
    )


def main() -> None:
    global PROTECTED_HASHES_BEFORE

    resolve_native_api_before_mutation()
    asset_subsystem, level_subsystem, actor_subsystem, _ = required_editor_subsystems()
    PROTECTED_HASHES_BEFORE = snapshot_protected_hashes()
    log_protected_hashes("FIRST_PLANET_REGIONS_PREFLIGHT_SOURCE_SHA256", PROTECTED_HASHES_BEFORE)
    source_character_class, source_game_mode_class = audit_protected_sources(
        asset_subsystem,
        level_subsystem,
        actor_subsystem,
    )
    mode = preflight_target_state(asset_subsystem)

    if mode == "fresh":
        create_fresh_content(
            asset_subsystem,
            source_character_class,
            source_game_mode_class,
        )
        result_mode = "created"
    else:
        result_mode = "already-exact"

    # Loading each saved map here is the disk-backed exact reload audit.  The
    # already-existing branch reaches only these read operations; it never saves.
    exact_reload_audit(
        asset_subsystem,
        source_character_class,
        source_game_mode_class,
    )
    verify_protected_hashes_unchanged("FIRST_PLANET_REGIONS_AUDIT_SOURCE_SHA256")
    info(
        "FIRST_PLANET_REGIONS_CREATED_THIS_RUN="
        + (",".join(CREATED_THIS_RUN) if CREATED_THIS_RUN else "none")
    )
    info(
        "FIRST_PLANET_REGIONS_RESULT=VERIFIED "
        f"mode={result_mode},assets=4,townActors=14,fieldActors=13"
    )


failure_message: str | None = None
try:
    main()
except Exception as exc:
    # Preserve text only. Leaving this except block releases the exception and
    # traceback, including create_fresh_content locals that may reference the
    # current-run Town/Field World and Catalog packages.
    failure_message = f"{qualified_type_name(exc)}: {exc}"

if failure_message is not None:
    unreal.log_error(
        "[EverPlanet First Planet Regions] "
        f"FIRST_PLANET_REGIONS_FAILURE=error={failure_message}"
    )
    unreal.log_error(
        "[EverPlanet First Planet Regions] "
        "FIRST_PLANET_REGIONS_RESULT=FAILED createdThisRun="
        + (",".join(CREATED_THIS_RUN) if CREATED_THIS_RUN else "none")
    )
    try:
        rollback_created_assets()
    except Exception as rollback_error:
        # Rollback is best-effort and exact-target only. Never let a rollback
        # diagnostic failure replace the original generator failure or skip the
        # protected-package hash audit.
        unreal.log_error(
            "[EverPlanet First Planet Regions] "
            "FIRST_PLANET_REGIONS_ROLLBACK=FAILED reason=exception "
            f"error={qualified_type_name(rollback_error)}: {rollback_error}"
        )
    if PROTECTED_HASHES_BEFORE:
        try:
            verify_protected_hashes_unchanged(
                "FIRST_PLANET_REGIONS_ROLLBACK_SOURCE_SHA256"
            )
        except Exception as hash_error:
            unreal.log_error(
                "[EverPlanet First Planet Regions] "
                "FIRST_PLANET_REGIONS_ROLLBACK_SOURCE_SHA256=FAILED "
                f"error={hash_error}"
            )
    raise RuntimeError(failure_message)
