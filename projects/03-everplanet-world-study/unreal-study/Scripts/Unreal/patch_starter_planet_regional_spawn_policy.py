"""Enable the regional spawn gate on exactly one Product GameMode asset.

Run only through UnrealEditor.exe with -ExecutePythonScript while every other
Unreal Editor process is closed. The shared native GameMode intentionally
defaults this policy to false so legacy, M1, and M2 GameModes stay unchanged.
This script accepts only the exact Product Blueprint and never creates,
deletes, renames, reparents, or broadly repairs content.
"""

from __future__ import annotations

import re

import unreal


TARGET_GAME_MODE = "/Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode"
TARGET_MAP = "/Game/EverPlanet/Product/Maps/Lvl_FirstPlanet_Graybox"
EXPECTED_PARENT_CLASS = "/Script/EverPlanet.EverPlanetGameMode"
EXPECTED_DESCRIPTOR_CLASS = "/Script/EverPlanet.EverPlanetRegionDescriptor"
EXPECTED_PLAYER_START_CLASS = "/Script/EverPlanet.EverPlanetRegionPlayerStart"
EXPECTED_CATALOG_CLASS = "/Script/EverPlanet.EverPlanetWorldCatalog"
PROPERTY_NAME = "require_regional_spawn_flow"

MUTATED_IN_MEMORY = False
SAVED = False


def info(message: str) -> None:
    unreal.log(f"[EverPlanet Starter Regional Spawn Patch] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet Starter Regional Spawn Patch] {message}")
        raise RuntimeError(message)


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
    info(f"STARTER_REGIONAL_POLICY_CATALOG_VALIDATE_RETURN={diagnostic}")

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


def object_path(obj: unreal.Object | None) -> str:
    return obj.get_path_name() if obj else "None"


def class_path(obj: unreal.Object | None) -> str:
    return obj.get_class().get_path_name() if obj else "None"


def class_is_child_of(actual_class: unreal.Class, expected_parent: unreal.Class) -> bool:
    return bool(unreal.MathLibrary.class_is_child_of(actual_class, expected_parent))


def struct_property(value: object, property_name: str) -> object:
    getter = getattr(value, "get_editor_property", None)
    if callable(getter):
        return getter(property_name)
    return getattr(value, property_name)


def stable_id(value: object) -> str:
    text = str(value)
    separators = "._-"
    require(
        0 < len(text) <= 128
        and text[0] not in separators
        and text[-1] not in separators
        and all("0" <= character <= "9" or "a" <= character <= "z" or character in separators for character in text),
        f"Invalid stable ID in regional map readiness contract: {text}",
    )
    return text


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
        game_path_index = text.find("/Game/")
        require(game_path_index >= 0, f"Map soft reference has no /Game package: {text}")
        text = text[game_path_index:]
        for delimiter in ("'", '"', " "):
            text = text.split(delimiter, 1)[0]
        package_name = text.split(".", 1)[0]

    require(package_name == TARGET_MAP, "Catalog region map package mismatch: "
            f"actual={package_name}, expected={TARGET_MAP}")
    return package_name


def audit_target_map_readiness() -> tuple[str, str, str]:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    require(level_subsystem is not None, "LevelEditorSubsystem unavailable")
    require(actor_subsystem is not None, "EditorActorSubsystem unavailable")
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    require(
        asset_subsystem.does_asset_exist(TARGET_MAP),
        f"Target map missing: {TARGET_MAP}",
    )
    require(level_subsystem.load_level(TARGET_MAP), f"Target map load failed: {TARGET_MAP}")

    world = editor_subsystem.get_editor_world()
    require(world is not None, "Target map has no editor world")
    expected_game_mode = unreal.EditorAssetLibrary.load_blueprint_class(TARGET_GAME_MODE)
    require(expected_game_mode is not None, "Target Product GameMode class could not be loaded")
    actual_game_mode = world.get_world_settings().get_editor_property("default_game_mode")
    require(
        actual_game_mode == expected_game_mode,
        "Target map WorldSettings GameMode override mismatch: "
        f"actual={object_path(actual_game_mode)}, expected={object_path(expected_game_mode)}",
    )

    actors = actor_subsystem.get_all_level_actors()
    descriptor_class = unreal.load_class(None, EXPECTED_DESCRIPTOR_CLASS)
    player_start_class = unreal.load_class(None, EXPECTED_PLAYER_START_CLASS)
    catalog_class = unreal.load_class(None, EXPECTED_CATALOG_CLASS)
    require(descriptor_class is not None, "Native region descriptor class could not be loaded")
    require(player_start_class is not None, "Native regional PlayerStart class could not be loaded")
    require(catalog_class is not None, "Native world catalog class could not be loaded")
    descriptors = [
        actor
        for actor in actors
        if class_is_child_of(actor.get_class(), descriptor_class)
    ]
    require(
        len(descriptors) == 1,
        "Regional spawn policy requires exactly one native region descriptor; "
        f"found={len(descriptors)}",
    )
    descriptor = descriptors[0]
    catalog = descriptor.get_editor_property("world_catalog")
    require(catalog is not None, "Region descriptor has no world catalog")
    require(
        class_is_child_of(catalog.get_class(), catalog_class),
        "Region descriptor catalog class mismatch: "
        f"actual={class_path(catalog)}, expected={EXPECTED_CATALOG_CLASS}",
    )
    validation_result = catalog.validate_catalog()
    unpack_native_catalog_validation(
        validation_result,
        "before-product-spawn-policy-mutation",
    )

    descriptor_planet = stable_id(descriptor.get_editor_property("planet_id"))
    descriptor_region = stable_id(descriptor.get_editor_property("region_id"))
    initial_address = catalog.get_editor_property("initial_address")
    initial_planet = stable_id(struct_property(initial_address, "planet_id"))
    initial_region = stable_id(struct_property(initial_address, "region_id"))
    initial_anchor = stable_id(struct_property(initial_address, "spawn_anchor_id"))
    require(
        (initial_planet, initial_region) == (descriptor_planet, descriptor_region),
        "Target descriptor must identify the catalog initial region before the Product "
        "spawn gate can be enabled: "
        f"descriptor={descriptor_planet}/{descriptor_region}, "
        f"initial={initial_planet}/{initial_region}",
    )

    matching_regions = []
    for region in catalog.get_editor_property("regions"):
        region_planet = str(struct_property(region, "planet_id"))
        region_id = str(struct_property(region, "region_id"))
        if (region_planet, region_id) == (descriptor_planet, descriptor_region):
            matching_regions.append(region)
    require(
        len(matching_regions) == 1,
        "Catalog must contain exactly one definition for the target descriptor region; "
        f"found={len(matching_regions)}",
    )
    soft_reference_package(struct_property(matching_regions[0], "map"))
    declared_anchors = [
        stable_id(anchor)
        for anchor in struct_property(matching_regions[0], "spawn_anchor_ids")
    ]
    require(
        len(declared_anchors) == len(set(declared_anchors)),
        "Catalog region contains duplicate spawn anchor IDs",
    )
    require(
        initial_anchor in declared_anchors,
        f"Catalog initial anchor is not declared by its region: {initial_anchor}",
    )

    regional_starts = [
        actor
        for actor in actors
        if class_is_child_of(actor.get_class(), player_start_class)
    ]
    require(regional_starts, "Target map has no native regional PlayerStart")
    start_ids = [
        stable_id(start.get_editor_property("spawn_anchor_id"))
        for start in regional_starts
    ]
    require(
        len(start_ids) == len(set(start_ids)),
        "Target map contains duplicate regional PlayerStart anchor IDs",
    )
    require(
        start_ids.count(initial_anchor) == 1,
        "Target map requires exactly one regional PlayerStart for the catalog initial "
        f"anchor {initial_anchor}; found={start_ids.count(initial_anchor)}",
    )
    return descriptor_planet, descriptor_region, initial_anchor


def audit_game_mode(
    expected_policy: bool | None,
) -> tuple[unreal.EditorAssetSubsystem, unreal.Blueprint, unreal.Object, bool]:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    require(
        asset_subsystem.does_asset_exist(TARGET_GAME_MODE),
        f"Target GameMode missing: {TARGET_GAME_MODE}",
    )

    blueprint = asset_subsystem.load_asset(TARGET_GAME_MODE)
    require(isinstance(blueprint, unreal.Blueprint), "Target GameMode is not a Blueprint")

    expected_parent = unreal.load_class(None, EXPECTED_PARENT_CLASS)
    require(expected_parent is not None, "Expected native parent class could not be loaded")
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(
        actual_parent == expected_parent,
        "Unexpected target GameMode parent before mutation: "
        f"{object_path(actual_parent)}",
    )

    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(TARGET_GAME_MODE)
    require(generated_class is not None, "Target generated class could not be loaded")
    defaults = unreal.get_default_object(generated_class)
    require(defaults is not None, "Target GameMode defaults unavailable")

    actual_policy = defaults.get_editor_property(PROPERTY_NAME)
    require(
        isinstance(actual_policy, bool),
        "Regional spawn policy is not an exact bool: "
        f"type={type(actual_policy).__name__}",
    )
    if expected_policy is not None:
        require(
            actual_policy is expected_policy,
            "Regional spawn policy readback mismatch: "
            f"actual={actual_policy}, expected={expected_policy}",
        )
    return asset_subsystem, blueprint, defaults, actual_policy


def main() -> None:
    global MUTATED_IN_MEMORY, SAVED

    planet_id, region_id, spawn_anchor_id = audit_target_map_readiness()
    info(
        "STARTER_REGIONAL_SPAWN_CONTENT_READY="
        f"map={TARGET_MAP}, region={planet_id}/{region_id}, "
        f"initialAnchor={spawn_anchor_id}, descriptor=1"
    )
    asset_subsystem, _, defaults, actual_policy = audit_game_mode(None)
    mode = "already-patched" if actual_policy else "legacy"
    info(
        "STARTER_REGIONAL_SPAWN_PREFLIGHT="
        f"asset={TARGET_GAME_MODE}, requireRegionalSpawnFlow={str(actual_policy).lower()}, "
        f"mode={mode}"
    )

    result_mode = "already-patched"
    if not actual_policy:
        defaults.modify()
        defaults.set_editor_property(PROPERTY_NAME, True)
        MUTATED_IN_MEMORY = True
        require(
            defaults.get_editor_property(PROPERTY_NAME) is True,
            "In-memory regional spawn policy readback mismatch",
        )
        info("STARTER_REGIONAL_SPAWN_MUTATION=false->true")

        require(
            asset_subsystem.save_asset(TARGET_GAME_MODE, False),
            "Target GameMode save failed",
        )
        SAVED = True
        info(f"STARTER_REGIONAL_SPAWN_SAVE=success asset={TARGET_GAME_MODE}")
        result_mode = "patched"

    _, _, _, verified_policy = audit_game_mode(True)
    info(
        "STARTER_REGIONAL_SPAWN_AUDIT="
        f"requireRegionalSpawnFlow={str(verified_policy).lower()}"
    )
    info(f"STARTER_REGIONAL_SPAWN_PATCH_RESULT=VERIFIED mode={result_mode}")


try:
    main()
except Exception as exc:
    unreal.log_error(
        "[EverPlanet Starter Regional Spawn Patch] "
        "STARTER_REGIONAL_SPAWN_PATCH_RESULT=FAILED "
        f"mutatedInMemory={int(MUTATED_IN_MEMORY)} saved={int(SAVED)} error={exc}"
    )
    raise
