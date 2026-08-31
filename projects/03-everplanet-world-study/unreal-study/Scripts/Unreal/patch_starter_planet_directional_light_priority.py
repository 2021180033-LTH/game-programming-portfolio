"""Patch exactly one known Starter Planet lighting contract.

Run only through UnrealEditor.exe with -ExecutePythonScript while every other
Unreal Editor process is closed.  The script never creates, deletes, renames,
or broadly repairs content.  It accepts only the exact legacy priority state
0/0 or the exact final state 1/0 for the existing product map.
"""

from __future__ import annotations

import math

import unreal


TARGET_MAP = "/Game/EverPlanet/Product/Maps/Lvl_FirstPlanet_Graybox"
TARGET_GAME_MODE = "/Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode"

DIRECTIONAL_LIGHT_CLASS = "/Script/Engine.DirectionalLight"
MAIN_LIGHT_LABEL = "DirectionalLight_Main"
FILL_LIGHT_LABEL = "DirectionalLight_Fill"
MAIN_LIGHT_ROTATION = (-35.0, -135.0, 0.0)
FILL_LIGHT_ROTATION = (35.0, 45.0, 0.0)
MAIN_INTENSITY = 5.0
FILL_INTENSITY = 1.5
MAIN_FORWARD_SHADING_PRIORITY = 1
FILL_FORWARD_SHADING_PRIORITY = 0

BASE_LABELS = {
    "PlanetBody_0",
    "PlayerStart_0",
    MAIN_LIGHT_LABEL,
    FILL_LIGHT_LABEL,
    "SkyLight_0",
    "SkyAtmosphere_0",
}
ROUTE_LABELS = {f"RouteTile_{index:03d}" for index in range(72)}
PLAZA_LABELS = {
    f"NorthPlaza_R{row}_C{column}"
    for row in range(3)
    for column in range(3)
}
GATE_LABELS = {
    f"{prefix}_{piece}"
    for prefix in ("NorthGate", "EastGate", "SouthGate", "WestGate")
    for piece in ("Left", "Right", "Top")
}
EXPECTED_LABELS = (
    BASE_LABELS | ROUTE_LABELS | PLAZA_LABELS | GATE_LABELS | {"JumpBarrier_0"}
)

MUTATED_IN_MEMORY = False
SAVED = False


def info(message: str) -> None:
    unreal.log(f"[EverPlanet Starter Light Priority Patch] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        unreal.log_error(f"[EverPlanet Starter Light Priority Patch] {message}")
        raise RuntimeError(message)


def class_path(obj: unreal.Object | None) -> str:
    return obj.get_class().get_path_name() if obj else "None"


def angle_distance_degrees(actual: float, expected: float) -> float:
    return abs((actual - expected + 180.0) % 360.0 - 180.0)


def rotator_nearly_equal(
    actual: unreal.Rotator,
    expected: tuple[float, float, float],
    tolerance: float,
) -> bool:
    expected_pitch, expected_yaw, expected_roll = expected
    return (
        angle_distance_degrees(float(actual.pitch), expected_pitch) <= tolerance
        and angle_distance_degrees(float(actual.yaw), expected_yaw) <= tolerance
        and angle_distance_degrees(float(actual.roll), expected_roll) <= tolerance
    )


def audit_loaded_map(
    allowed_priorities: set[tuple[int, int]],
) -> tuple[
    unreal.LevelEditorSubsystem,
    unreal.ActorComponent,
    unreal.ActorComponent,
    tuple[int, int],
]:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(asset_subsystem is not None, "EditorAssetSubsystem unavailable")
    require(level_subsystem is not None, "LevelEditorSubsystem unavailable")
    require(actor_subsystem is not None, "EditorActorSubsystem unavailable")
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    require(asset_subsystem.does_asset_exist(TARGET_MAP), f"Target map missing: {TARGET_MAP}")
    require(
        asset_subsystem.does_asset_exist(TARGET_GAME_MODE),
        f"Target GameMode missing: {TARGET_GAME_MODE}",
    )
    require(level_subsystem.load_level(TARGET_MAP), f"Target map load failed: {TARGET_MAP}")

    world = editor_subsystem.get_editor_world()
    require(world is not None, "Target map has no Editor World")
    expected_game_mode = unreal.EditorAssetLibrary.load_blueprint_class(TARGET_GAME_MODE)
    require(expected_game_mode is not None, "Target GameMode generated class could not be loaded")
    require(
        world.get_world_settings().get_editor_property("default_game_mode")
        == expected_game_mode,
        "Target map GameMode override mismatch",
    )

    actors = actor_subsystem.get_all_level_actors()
    by_label = {actor.get_actor_label(): actor for actor in actors}
    require(len(EXPECTED_LABELS) == 100, "Internal expected label contract is not 100")
    require(len(by_label) == len(actors), "Target map actor labels are not unique")
    require(
        len(actors) == len(EXPECTED_LABELS) and set(by_label) == EXPECTED_LABELS,
        "Unexpected target map actor set: "
        f"count={len(actors)}, labelDiff={sorted(set(by_label) ^ EXPECTED_LABELS)}",
    )

    directional_labels = {
        label
        for label, actor in by_label.items()
        if class_path(actor) == DIRECTIONAL_LIGHT_CLASS
    }
    require(
        directional_labels == {MAIN_LIGHT_LABEL, FILL_LIGHT_LABEL},
        f"Unexpected DirectionalLight actor set: {sorted(directional_labels)}",
    )
    main_light = by_label[MAIN_LIGHT_LABEL]
    fill_light = by_label[FILL_LIGHT_LABEL]
    require(
        class_path(main_light) == DIRECTIONAL_LIGHT_CLASS
        and class_path(fill_light) == DIRECTIONAL_LIGHT_CLASS,
        "Main or Fill actor class mismatch",
    )
    require(
        rotator_nearly_equal(main_light.get_actor_rotation(), MAIN_LIGHT_ROTATION, 1.0e-3),
        "Main DirectionalLight rotation mismatch",
    )
    require(
        rotator_nearly_equal(fill_light.get_actor_rotation(), FILL_LIGHT_ROTATION, 1.0e-3),
        "Fill DirectionalLight rotation mismatch",
    )

    main_component = main_light.get_editor_property("directional_light_component")
    fill_component = fill_light.get_editor_property("directional_light_component")
    require(main_component is not None and fill_component is not None, "Light component missing")
    require(
        main_component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE
        and fill_component.get_editor_property("mobility")
        == unreal.ComponentMobility.MOVABLE,
        "Main or Fill mobility mismatch",
    )
    require(
        math.isclose(
            float(main_component.get_editor_property("intensity")),
            MAIN_INTENSITY,
            rel_tol=0.0,
            abs_tol=1.0e-4,
        )
        and math.isclose(
            float(fill_component.get_editor_property("intensity")),
            FILL_INTENSITY,
            rel_tol=0.0,
            abs_tol=1.0e-4,
        ),
        "Main or Fill intensity mismatch",
    )
    require(
        bool(main_component.get_editor_property("atmosphere_sun_light"))
        and not bool(fill_component.get_editor_property("atmosphere_sun_light")),
        "Main or Fill atmosphere-sun role mismatch",
    )

    priorities = (
        int(main_component.get_editor_property("forward_shading_priority")),
        int(fill_component.get_editor_property("forward_shading_priority")),
    )
    require(
        priorities in allowed_priorities,
        "Refusing unexpected DirectionalLight priority state before mutation: "
        f"main={priorities[0]}, fill={priorities[1]}, "
        f"allowed={sorted(allowed_priorities)}",
    )
    return level_subsystem, main_component, fill_component, priorities


def main() -> None:
    global MUTATED_IN_MEMORY, SAVED

    level_subsystem, main_component, fill_component, priorities = audit_loaded_map(
        {(0, 0), (1, 0)}
    )
    mode = "legacy" if priorities == (0, 0) else "already-patched"
    info(
        "STARTER_LIGHT_PRIORITY_PREFLIGHT="
        f"actors=100, priorities={priorities[0]}/{priorities[1]}, "
        f"atmosphereSun=1/0, mode={mode}"
    )

    result_mode = "already-patched"
    if priorities == (0, 0):
        main_component.set_editor_property(
            "forward_shading_priority",
            MAIN_FORWARD_SHADING_PRIORITY,
        )
        MUTATED_IN_MEMORY = True
        immediate_priorities = (
            int(main_component.get_editor_property("forward_shading_priority")),
            int(fill_component.get_editor_property("forward_shading_priority")),
        )
        require(
            immediate_priorities
            == (MAIN_FORWARD_SHADING_PRIORITY, FILL_FORWARD_SHADING_PRIORITY),
            f"In-memory priority readback mismatch: {immediate_priorities}",
        )
        info("STARTER_LIGHT_PRIORITY_MUTATION=main:0->1, fill:0->0")
        require(level_subsystem.save_current_level(), "Target map save failed")
        SAVED = True
        info(f"STARTER_LIGHT_PRIORITY_SAVE=success map={TARGET_MAP}")
        result_mode = "patched"

    _, _, _, verified_priorities = audit_loaded_map({(1, 0)})
    info(
        "STARTER_LIGHT_PRIORITY_AUDIT="
        f"actors=100, priorities={verified_priorities[0]}/{verified_priorities[1]}, "
        "atmosphereSun=1/0"
    )
    info(f"STARTER_LIGHT_PRIORITY_PATCH_RESULT=VERIFIED mode={result_mode}")


try:
    main()
except Exception as exc:
    unreal.log_error(
        "[EverPlanet Starter Light Priority Patch] "
        "STARTER_LIGHT_PRIORITY_PATCH_RESULT=FAILED "
        f"mutatedInMemory={int(MUTATED_IN_MEMORY)} saved={int(SAVED)} error={exc}"
    )
    raise
