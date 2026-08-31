"""Patch exactly one known Starter Planet camera-input contract.

Run only through UnrealEditor.exe with -ExecutePythonScript while every other
Unreal Editor process is closed.  The script never creates, deletes, renames,
or broadly repairs content.  It accepts only the exact legacy product camera
state (yaw 1, pitch 0) or the exact final state (yaw 0, pitch 0).

This is a Product-only policy patch.  Native defaults and /Tests/M2 assets keep
their look-input capability so the isolated camera test harness remains useful.
"""

from __future__ import annotations

import math

import unreal


TARGET_CHARACTER = (
    "/Game/EverPlanet/Product/Characters/BP_EverPlanetPlayerCharacter"
)
EXPECTED_PARENT_CLASS = "/Script/EverPlanet.EverPlanetGlobeCameraCharacter"
EXPECTED_CAMERA_COMPONENT_CLASS = "/Script/EverPlanet.PlanetGlobeCameraComponent"

LEGACY_YAW_PER_INPUT = 1.0
FIXED_YAW_PER_INPUT = 0.0
FIXED_PITCH_PER_INPUT = 0.0

CAMERA_CONTRACT = (
    ("camera_arm_length", 1600.0),
    ("camera_down_pitch_degrees", 38.0),
    ("camera_horizontal_fov_degrees", 60.0),
    ("camera_local_up_offset", 100.0),
    ("camera_look_ahead_distance", 0.0),
    ("camera_collision_probe_size", 18.0),
    ("camera_pitch_degrees_per_input_unit", FIXED_PITCH_PER_INPUT),
    ("minimum_camera_down_pitch_degrees", 38.0),
    ("maximum_camera_down_pitch_degrees", 38.0),
)

MUTATED_IN_MEMORY = False
SAVED = False


def info(message: str) -> None:
    unreal.log(f"[EverPlanet Starter Fixed Camera Patch] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet Starter Fixed Camera Patch] {message}")
        raise RuntimeError(message)


def object_path(obj: unreal.Object | None) -> str:
    return obj.get_path_name() if obj else "None"


def class_path(obj: unreal.Object | None) -> str:
    return obj.get_class().get_path_name() if obj else "None"


def nearly_equal(actual: float, expected: float) -> bool:
    return math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-4)


def audit_character(
    allowed_yaw_values: tuple[float, ...],
) -> tuple[
    unreal.EditorAssetSubsystem,
    unreal.Blueprint,
    unreal.Object,
    unreal.ActorComponent,
    float,
]:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    require(
        asset_subsystem.does_asset_exist(TARGET_CHARACTER),
        f"Target Character missing: {TARGET_CHARACTER}",
    )

    blueprint = asset_subsystem.load_asset(TARGET_CHARACTER)
    require(isinstance(blueprint, unreal.Blueprint), "Target Character is not a Blueprint")

    expected_parent = unreal.load_class(None, EXPECTED_PARENT_CLASS)
    require(expected_parent is not None, "Expected native parent class could not be loaded")
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(
        actual_parent == expected_parent,
        "Unexpected target Character parent before mutation: "
        f"{object_path(actual_parent)}",
    )

    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(TARGET_CHARACTER)
    require(generated_class is not None, "Target generated class could not be loaded")
    defaults = unreal.get_default_object(generated_class)
    require(defaults is not None, "Target Character defaults unavailable")

    camera_rig = defaults.get_editor_property("planet_globe_camera_component")
    require(camera_rig is not None, "Target globe camera component unavailable")
    require(
        class_path(camera_rig) == EXPECTED_CAMERA_COMPONENT_CLASS,
        "Unexpected target camera component class before mutation: "
        f"{class_path(camera_rig)}",
    )

    for property_name, expected in CAMERA_CONTRACT:
        actual = float(camera_rig.get_editor_property(property_name))
        if property_name == "camera_pitch_degrees_per_input_unit":
            require(
                actual == expected,
                "Refusing non-exact pitch-input state before mutation: "
                f"actual={actual}, expected exact={expected}",
            )
            continue
        require(
            nearly_equal(actual, expected),
            "Refusing unexpected non-yaw camera state before mutation: "
            f"{property_name}={actual}, expected={expected}",
        )

    actual_yaw = float(
        camera_rig.get_editor_property("camera_yaw_degrees_per_input_unit")
    )
    require(
        actual_yaw in allowed_yaw_values,
        "Refusing unexpected yaw-input state before mutation: "
        f"actual={actual_yaw}, allowed={allowed_yaw_values}",
    )
    return asset_subsystem, blueprint, defaults, camera_rig, actual_yaw


def main() -> None:
    global MUTATED_IN_MEMORY, SAVED

    asset_subsystem, _, defaults, camera_rig, actual_yaw = audit_character(
        (LEGACY_YAW_PER_INPUT, FIXED_YAW_PER_INPUT)
    )
    mode = "legacy" if actual_yaw == LEGACY_YAW_PER_INPUT else "already-patched"
    info(
        "STARTER_FIXED_CAMERA_PREFLIGHT="
        f"asset={TARGET_CHARACTER}, yawInput={actual_yaw:.1f}, "
        f"pitchInput={FIXED_PITCH_PER_INPUT:.1f}, pitchRange=38..38, mode={mode}"
    )

    result_mode = "already-patched"
    if actual_yaw == LEGACY_YAW_PER_INPUT:
        defaults.modify()
        camera_rig.modify()
        camera_rig.set_editor_property(
            "camera_yaw_degrees_per_input_unit",
            FIXED_YAW_PER_INPUT,
        )
        MUTATED_IN_MEMORY = True
        immediate_yaw = float(
            camera_rig.get_editor_property("camera_yaw_degrees_per_input_unit")
        )
        require(
            immediate_yaw == FIXED_YAW_PER_INPUT,
            f"In-memory yaw-input readback mismatch: {immediate_yaw}",
        )
        info("STARTER_FIXED_CAMERA_MUTATION=yawInput:1.0->0.0, pitchInput:0.0->0.0")

        require(
            asset_subsystem.save_asset(TARGET_CHARACTER, False),
            "Target Character save failed",
        )
        SAVED = True
        info(f"STARTER_FIXED_CAMERA_SAVE=success asset={TARGET_CHARACTER}")
        result_mode = "patched"

    _, _, _, _, verified_yaw = audit_character((FIXED_YAW_PER_INPUT,))
    info(
        "STARTER_FIXED_CAMERA_AUDIT="
        f"yawInput={verified_yaw:.1f}, pitchInput={FIXED_PITCH_PER_INPUT:.1f}, "
        "arm=1600.0, pitch=38.0, hfov=60.0, pitchRange=38..38"
    )
    info(f"STARTER_FIXED_CAMERA_PATCH_RESULT=VERIFIED mode={result_mode}")


try:
    main()
except Exception as exc:
    unreal.log_error(
        "[EverPlanet Starter Fixed Camera Patch] "
        "STARTER_FIXED_CAMERA_PATCH_RESULT=FAILED "
        f"mutatedInMemory={int(MUTATED_IN_MEMORY)} saved={int(SAVED)} error={exc}"
    )
    raise
