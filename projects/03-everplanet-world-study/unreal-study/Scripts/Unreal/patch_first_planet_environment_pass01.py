"""Apply First Planet Environment Pass 01 to the verified Town/Field skeletons.

This is a fail-closed, one-way content patch.  It never changes the Catalog,
GameModes, Character, fixed camera, regional starts, portals, or the legacy
graybox.  A PowerShell runner owns the disk backup/rollback boundary.
"""

from __future__ import annotations

import gc
import hashlib
import math
import os

import unreal


PROJECT_ROOT = os.environ.get(
    "EVERPLANET_PROJECT_ROOT",
    os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")),
)
PASS_VERSION = 1

TOWN_MAP = "/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Town"
FIELD_MAP = "/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Field"
REGIONAL_GAME_MODE = "/Game/EverPlanet/Product/GameModes/BP_EverPlanetRegionalGameMode"
CATALOG = "/Game/EverPlanet/Product/World/DA_EverPlanetWorldCatalog"
PRODUCT_CHARACTER = "/Game/EverPlanet/Product/Characters/BP_EverPlanetPlayerCharacter"
PRODUCT_GAME_MODE = "/Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode"
PRODUCT_GRAYBOX = "/Game/EverPlanet/Product/Maps/Lvl_FirstPlanet_Graybox"

PLANET_BODY_CLASS = "/Script/EverPlanet.PlanetBody"
REGION_DESCRIPTOR_CLASS = "/Script/EverPlanet.EverPlanetRegionDescriptor"
REGION_START_CLASS = "/Script/EverPlanet.EverPlanetRegionPlayerStart"
REGION_PORTAL_CLASS = "/Script/EverPlanet.EverPlanetRegionPortal"
STATIC_MESH_ACTOR_CLASS = "/Script/Engine.StaticMeshActor"
DIRECTIONAL_LIGHT_CLASS = "/Script/Engine.DirectionalLight"
SKY_LIGHT_CLASS = "/Script/Engine.SkyLight"
SKY_ATMOSPHERE_CLASS = "/Script/Engine.SkyAtmosphere"

CUBE_MESH = "/Engine/BasicShapes/Cube"
CYLINDER_MESH = "/Engine/BasicShapes/Cylinder"
SPHERE_MESH = "/Engine/BasicShapes/Sphere"
CONE_MESH = "/Engine/BasicShapes/Cone"
CHAMFER_CUBE_MESH = "/Game/LevelPrototyping/Meshes/SM_ChamferCube"
FLAT_PARENT_MATERIAL = "/Game/LevelPrototyping/Materials/M_FlatCol"
MATERIAL_DIRECTORY = "/Game/EverPlanet/Product/Art/Materials"

PLANET_RADIUS_CM = 10000.0
SPAWN_CLEARANCE_CM = 25.0
TRANSFORM_TOLERANCE = 0.15
ROTATION_TOLERANCE = 0.02

TOWN_BASELINE_SHA256 = "dafbe4dffcff9b228ccf32daeb125f78523327a39c1b68eb2422825a63994c4a"
FIELD_BASELINE_SHA256 = "01a9ac762e4002450c4d2948e398520d0a51667c93f9788e5d1e2957a60a1116"

TOWN_DESCRIPTOR_LABEL = "RegionDescriptor_Town"
TOWN_START_LABEL = "RegionalStart_Town_Start"
TOWN_RETURN_LABEL = "RegionalStart_Town_FromField"
TOWN_PORTAL_LABEL = "RegionPortal_Town_EastGate"
FIELD_DESCRIPTOR_LABEL = "RegionDescriptor_Field"
FIELD_START_LABEL = "RegionalStart_Field_FromTown"
FIELD_PORTAL_LABEL = "RegionPortal_Field_WestGate"

TOWN_BASE_LABELS = {
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
    TOWN_DESCRIPTOR_LABEL,
    TOWN_START_LABEL,
    TOWN_RETURN_LABEL,
    TOWN_PORTAL_LABEL,
}

FIELD_BASE_LABELS = {
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
    FIELD_DESCRIPTOR_LABEL,
    FIELD_START_LABEL,
    FIELD_PORTAL_LABEL,
}

def srgb_hex_to_linear_rgba(value: str) -> tuple[float, float, float, float]:
    """Convert an authored sRGB hex colour to Unreal's linear colour space."""
    if len(value) != 7 or not value.startswith("#"):
        raise ValueError(f"Invalid sRGB hex colour: {value}")

    def channel(component: str) -> float:
        srgb = int(component, 16) / 255.0
        if srgb <= 0.04045:
            return srgb / 12.92
        return ((srgb + 0.055) / 1.055) ** 2.4

    return (
        channel(value[1:3]),
        channel(value[3:5]),
        channel(value[5:7]),
        1.0,
    )


# Colours are authored in sRGB so the palette can be compared directly with the
# reference footage.  They are converted once here before being written to the
# material parameter, which expects linear values.
MATERIAL_DEFINITIONS = (
    ("MI_EP01_Grass", srgb_hex_to_linear_rgba("#3CB352"), 0.92),
    ("MI_EP01_GrassDark", srgb_hex_to_linear_rgba("#0A8E39"), 0.94),
    ("MI_EP01_PathSand", srgb_hex_to_linear_rgba("#C99A52"), 0.96),
    ("MI_EP01_StoneLight", srgb_hex_to_linear_rgba("#E9E6CA"), 0.90),
    ("MI_EP01_WallCream", srgb_hex_to_linear_rgba("#F1D8A8"), 0.88),
    ("MI_EP01_RoofBlue", srgb_hex_to_linear_rgba("#278CEB"), 0.82),
    ("MI_EP01_RoofCoral", srgb_hex_to_linear_rgba("#DC5A45"), 0.84),
    ("MI_EP01_Wood", srgb_hex_to_linear_rgba("#805511"), 0.94),
    ("MI_EP01_FoliageLight", srgb_hex_to_linear_rgba("#58BE57"), 0.93),
    ("MI_EP01_FoliageDark", srgb_hex_to_linear_rgba("#087A36"), 0.95),
    ("MI_EP01_Water", srgb_hex_to_linear_rgba("#2CB8E5"), 0.28),
    ("MI_EP01_Rock", srgb_hex_to_linear_rgba("#687878"), 0.97),
    ("MI_EP01_AccentGold", srgb_hex_to_linear_rgba("#F5B83D"), 0.72),
)

MATERIAL_PATHS = {
    name: f"{MATERIAL_DIRECTORY}/{name}"
    for name, _, _ in MATERIAL_DEFINITIONS
}

PROTECTED_PACKAGE_FILES = {
    CATALOG: os.path.join(
        PROJECT_ROOT,
        "Content",
        "EverPlanet",
        "Product",
        "World",
        "DA_EverPlanetWorldCatalog.uasset",
    ),
    REGIONAL_GAME_MODE: os.path.join(
        PROJECT_ROOT,
        "Content",
        "EverPlanet",
        "Product",
        "GameModes",
        "BP_EverPlanetRegionalGameMode.uasset",
    ),
    PRODUCT_CHARACTER: os.path.join(
        PROJECT_ROOT,
        "Content",
        "EverPlanet",
        "Product",
        "Characters",
        "BP_EverPlanetPlayerCharacter.uasset",
    ),
    PRODUCT_GAME_MODE: os.path.join(
        PROJECT_ROOT,
        "Content",
        "EverPlanet",
        "Product",
        "GameModes",
        "BP_EverPlanetGameMode.uasset",
    ),
    PRODUCT_GRAYBOX: os.path.join(
        PROJECT_ROOT,
        "Content",
        "EverPlanet",
        "Product",
        "Maps",
        "Lvl_FirstPlanet_Graybox.umap",
    ),
    FLAT_PARENT_MATERIAL: os.path.join(
        PROJECT_ROOT,
        "Content",
        "LevelPrototyping",
        "Materials",
        "M_FlatCol.uasset",
    ),
    CHAMFER_CUBE_MESH: os.path.join(
        PROJECT_ROOT,
        "Content",
        "LevelPrototyping",
        "Meshes",
        "SM_ChamferCube.uasset",
    ),
}

PROTECTED_BASELINE_SHA256 = {
    CATALOG: "43519df92556208b1aac175d91c75a07550419304db194445633e9d2b3a7ae13",
    REGIONAL_GAME_MODE: "8f4214ba4a1375c5e7531b8859e4b4f45f82daf5776047a3877c2de9dbcfb96a",
    PRODUCT_CHARACTER: "30977b9eaf9504b2cdeccc80cea1f444dc8ec6da5ae9172ea9f476f58fcb78be",
    PRODUCT_GAME_MODE: "d2e7023b87955e11632511764ab4757c70c01fab6ba8879e9cd86a87fa52decb",
    PRODUCT_GRAYBOX: "1d08e0bda549a06ec0dcbd0f61e659e8f7f088c2ebc596b4bcb8abc0f6943b74",
    FLAT_PARENT_MATERIAL: "41dbbda4b463a224c29ee01765e1f52a8ae63f2f94f1440ee267fe7b1a481585",
    CHAMFER_CUBE_MESH: "b20a3ae9668ec599db519af3f84b67952d0e31ac4427b80ee812232edde91f53",
}


def info(message: str) -> None:
    unreal.log(f"[EverPlanet First Planet Env P01] {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def canonical_object_path(package_path: str) -> str:
    return f"{package_path}.{package_path.rsplit('/', 1)[-1]}"


def object_path(value: unreal.Object | None) -> str:
    return "" if value is None else str(value.get_path_name())


def class_path(value: unreal.Object | None) -> str:
    return "" if value is None else str(value.get_class().get_path_name())


def load_asset(path: str) -> unreal.Object:
    value = unreal.EditorAssetLibrary.load_asset(path)
    require(value is not None, f"Required asset could not be loaded: {path}")
    return value


def load_class(path: str) -> unreal.Class:
    value = unreal.load_class(None, path)
    require(value is not None, f"Required class could not be loaded: {path}")
    return value


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def package_disk_path(package_path: str, extension: str) -> str:
    require(package_path.startswith("/Game/"), f"Unsupported package root: {package_path}")
    relative = package_path.removeprefix("/Game/").replace("/", os.sep)
    return os.path.join(PROJECT_ROOT, "Content", relative + extension)


def snapshot_protected_hashes(marker: str) -> dict[str, str]:
    hashes: dict[str, str] = {}
    for package_path, disk_path in PROTECTED_PACKAGE_FILES.items():
        require(os.path.isfile(disk_path), f"Protected package is missing: {disk_path}")
        digest = sha256_file(disk_path)
        require(
            digest == PROTECTED_BASELINE_SHA256[package_path],
            f"Protected package is not the verified baseline: {package_path}",
        )
        hashes[package_path] = digest
        info(f"{marker}=asset={package_path},sha256={digest}")
    return hashes


def vector_distance(actual: unreal.Vector, expected: tuple[float, float, float]) -> float:
    return math.dist((float(actual.x), float(actual.y), float(actual.z)), expected)


def vector_close(actual: unreal.Vector, expected: tuple[float, float, float], tolerance: float) -> bool:
    return vector_distance(actual, expected) <= tolerance


def rotator_close(actual: unreal.Rotator, expected: tuple[float, float, float]) -> bool:
    def delta(left: float, right: float) -> float:
        return abs((left - right + 180.0) % 360.0 - 180.0)

    return (
        delta(float(actual.pitch), expected[0]) <= ROTATION_TOLERANCE
        and delta(float(actual.yaw), expected[1]) <= ROTATION_TOLERANCE
        and delta(float(actual.roll), expected[2]) <= ROTATION_TOLERANCE
    )


def surface_pose(
    base_angle_degrees: float,
    forward_arc_cm: float,
    right_arc_cm: float,
    up_offset_cm: float,
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    forward_angle_degrees = base_angle_degrees + math.degrees(
        forward_arc_cm / PLANET_RADIUS_CM
    )
    right_angle_degrees = math.degrees(right_arc_cm / PLANET_RADIUS_CM)
    forward_angle = math.radians(forward_angle_degrees)
    right_angle = math.radians(right_angle_degrees)
    cos_right = math.cos(right_angle)
    normal = (
        math.sin(forward_angle) * cos_right,
        math.sin(right_angle),
        math.cos(forward_angle) * cos_right,
    )
    radial_distance = PLANET_RADIUS_CM + up_offset_cm
    location = tuple(value * radial_distance for value in normal)
    rotation = (-forward_angle_degrees, 0.0, right_angle_degrees)
    return location, rotation


def visual_spec(
    label: str,
    mesh_path: str,
    material_name: str,
    base_angle_degrees: float,
    forward_arc_cm: float,
    right_arc_cm: float,
    up_offset_cm: float,
    scale: tuple[float, float, float],
    collision_profile: str,
) -> dict[str, object]:
    location, rotation = surface_pose(
        base_angle_degrees,
        forward_arc_cm,
        right_arc_cm,
        up_offset_cm,
    )
    return {
        "label": label,
        "mesh_path": mesh_path,
        "material_path": MATERIAL_PATHS[material_name],
        "location": location,
        "rotation": rotation,
        "scale": scale,
        "collision_profile": collision_profile,
    }


def build_town_core_specs() -> tuple[dict[str, object], ...]:
    return (
        visual_spec(
            "TownPlaza_0", CYLINDER_MESH, "MI_EP01_StoneLight",
            2.0, 0.0, 0.0, 6.0, (15.5, 15.5, 0.12), "NoCollision"
        ),
        visual_spec(
            "TownHouse_Left", CHAMFER_CUBE_MESH, "MI_EP01_WallCream",
            4.0, -250.0, -900.0, 250.0, (4.0, 5.0, 5.0), "BlockAll"
        ),
        visual_spec(
            "TownHouse_Right", CHAMFER_CUBE_MESH, "MI_EP01_WallCream",
            4.5, -120.0, 1120.0, 225.0, (4.5, 4.2, 4.5), "BlockAll"
        ),
        visual_spec(
            "TownGate_Left", CYLINDER_MESH, "MI_EP01_RoofBlue",
            11.0, 0.0, -500.0, 300.0, (1.0, 1.0, 6.0), "BlockAll"
        ),
        visual_spec(
            "TownGate_Right", CYLINDER_MESH, "MI_EP01_RoofBlue",
            11.0, 0.0, 500.0, 300.0, (1.0, 1.0, 6.0), "BlockAll"
        ),
    )


def build_town_environment_specs() -> tuple[dict[str, object], ...]:
    specs: list[dict[str, object]] = []

    for index in range(12):
        road_right = 40.0 + index * 14.0
        specs.append(
            visual_spec(
                f"Env01_Town_Road_{index:02d}", CUBE_MESH, "MI_EP01_PathSand",
                0.60 + index * 0.90, 0.0, road_right, 4.0,
                (1.75, 4.2, 0.08), "NoCollision"
            )
        )

    for label, forward_arc, right_arc, scale in (
        ("North", 760.0, 0.0, (1.2, 14.2, 0.6)),
        ("South", -760.0, 0.0, (1.2, 14.2, 0.6)),
        ("East", 0.0, 760.0, (14.2, 1.2, 0.6)),
        ("West", 0.0, -760.0, (14.2, 1.2, 0.6)),
    ):
        specs.append(
            visual_spec(
                f"Env01_Town_PlazaEdge_{label}", CUBE_MESH, "MI_EP01_StoneLight",
                2.0, forward_arc, right_arc, 30.0, scale, "NoCollision"
            )
        )

    fountain_specs = (
        ("Base", CYLINDER_MESH, "MI_EP01_StoneLight", 20.0, (4.0, 4.0, 0.4), "BlockAll"),
        ("Basin", CYLINDER_MESH, "MI_EP01_RoofBlue", 45.0, (3.5, 3.5, 0.25), "NoCollision"),
        ("Column", CYLINDER_MESH, "MI_EP01_StoneLight", 175.0, (0.65, 0.65, 2.7), "BlockAll"),
        ("Water", CYLINDER_MESH, "MI_EP01_Water", 62.0, (3.05, 3.05, 0.12), "NoCollision"),
        ("Orb", SPHERE_MESH, "MI_EP01_AccentGold", 335.0, (1.3, 1.3, 1.3), "NoCollision"),
    )
    for part, mesh, material, up, scale, collision in fountain_specs:
        specs.append(
            visual_spec(
                f"Env01_Town_Fountain_{part}", mesh, material,
                2.0, 0.0, 0.0, up, scale, collision
            )
        )

    for side, angle, forward_arc, right_arc, inner_right, outer_right, roof_material, roof_scale in (
        ("Left", 4.0, -250.0, -980.0, -705.0, -1120.0, "MI_EP01_RoofBlue", (4.8, 5.8, 3.0)),
        ("Right", 4.5, -120.0, 1120.0, 845.0, 1260.0, "MI_EP01_RoofCoral", (5.2, 5.0, 2.8)),
    ):
        specs.extend(
            (
                visual_spec(
                    f"Env01_Town_House_{side}_Roof", CONE_MESH, roof_material,
                    angle, forward_arc, right_arc, 610.0, roof_scale, "NoCollision"
                ),
                visual_spec(
                    f"Env01_Town_House_{side}_Door", CUBE_MESH, "MI_EP01_Wood",
                    angle, forward_arc, inner_right, 90.0, (0.9, 0.14, 1.8), "NoCollision"
                ),
                visual_spec(
                    f"Env01_Town_House_{side}_Chimney", CUBE_MESH, "MI_EP01_Rock",
                    angle, forward_arc - 100.0, outer_right, 560.0, (0.6, 0.6, 2.0), "NoCollision"
                ),
            )
        )

    for side, angle, forward_arc, right_arc, inner_right, outer_right, roof_material, body_scale in (
        ("Left", 6.1, -260.0, -1880.0, -1660.0, -2050.0, "MI_EP01_RoofBlue", (3.2, 4.4, 3.8)),
        ("Right", 7.0, 80.0, 1650.0, 1430.0, 1820.0, "MI_EP01_RoofBlue", (4.0, 3.6, 4.3)),
    ):
        specs.extend(
            (
                visual_spec(
                    f"Env01_Town_Cottage_{side}_Body", CHAMFER_CUBE_MESH, "MI_EP01_WallCream",
                    angle, forward_arc, right_arc, 200.0, body_scale, "BlockAll"
                ),
                visual_spec(
                    f"Env01_Town_Cottage_{side}_Roof", CONE_MESH, roof_material,
                    angle, forward_arc, right_arc, 530.0, (4.4, 4.6, 2.6), "NoCollision"
                ),
                visual_spec(
                    f"Env01_Town_Cottage_{side}_Door", CUBE_MESH, "MI_EP01_Wood",
                    angle, forward_arc, inner_right, 80.0, (0.8, 0.14, 1.6), "NoCollision"
                ),
                visual_spec(
                    f"Env01_Town_Cottage_{side}_Chimney", CUBE_MESH, "MI_EP01_Rock",
                    angle, forward_arc - 90.0, outer_right, 500.0, (0.55, 0.55, 2.0), "NoCollision"
                ),
            )
        )

    tree_positions = (
        (1.1, -1750.0), (2.0, 1450.0), (3.5, -2220.0), (4.8, 2050.0),
        (5.9, -2550.0), (7.4, 1880.0), (8.6, -1520.0), (9.8, 2350.0),
    )
    for index, (angle, right_arc) in enumerate(tree_positions):
        foliage = "MI_EP01_FoliageLight" if index % 2 == 0 else "MI_EP01_FoliageDark"
        specs.extend(
            (
                visual_spec(
                    f"Env01_Town_Tree_{index:02d}_Trunk", CYLINDER_MESH, "MI_EP01_Wood",
                    angle, 0.0, right_arc, 180.0, (0.75, 0.75, 3.6), "BlockAll"
                ),
                visual_spec(
                    f"Env01_Town_Tree_{index:02d}_Crown", SPHERE_MESH, foliage,
                    angle, 0.0, right_arc, 440.0, (3.1, 3.1, 3.0), "NoCollision"
                ),
            )
        )

    for index, (angle, right_arc) in enumerate(
        ((3.2, -520.0), (5.5, 520.0), (7.8, -520.0), (10.0, 520.0))
    ):
        specs.extend(
            (
                visual_spec(
                    f"Env01_Town_Lamp_{index:02d}_Pole", CYLINDER_MESH, "MI_EP01_Wood",
                    angle, 0.0, right_arc, 180.0, (0.35, 0.35, 3.6), "BlockAll"
                ),
                visual_spec(
                    f"Env01_Town_Lamp_{index:02d}_Glow", SPHERE_MESH, "MI_EP01_AccentGold",
                    angle, 0.0, right_arc, 385.0, (1.0, 1.0, 1.0), "NoCollision"
                ),
            )
        )

    fence_index = 0
    for angle in (5.0, 6.3, 7.6, 8.9):
        for right_arc in (-750.0, 750.0):
            specs.append(
                visual_spec(
                    f"Env01_Town_Fence_{fence_index:02d}", CUBE_MESH, "MI_EP01_Wood",
                    angle, 0.0, right_arc, 45.0, (1.9, 0.18, 0.9), "BlockAll"
                )
            )
            fence_index += 1

    specs.append(
        visual_spec(
            "Env01_Town_GateTop", CUBE_MESH, "MI_EP01_RoofBlue",
            11.0, 0.0, 0.0, 610.0, (1.2, 11.0, 1.0), "NoCollision"
        )
    )
    specs.extend(
        (
            visual_spec(
                "Env01_Town_Sign_Post", CUBE_MESH, "MI_EP01_Wood",
                3.0, 0.0, 650.0, 120.0, (0.35, 0.35, 2.4), "BlockAll"
            ),
            visual_spec(
                "Env01_Town_Sign_Board", CUBE_MESH, "MI_EP01_AccentGold",
                3.0, 0.0, 650.0, 265.0, (0.35, 2.2, 0.8), "NoCollision"
            ),
            visual_spec(
                "Env01_Town_FlowerBed_Left", CYLINDER_MESH, "MI_EP01_RoofCoral",
                2.0, 0.0, -760.0, 15.0, (4.0, 1.2, 0.3), "NoCollision"
            ),
            visual_spec(
                "Env01_Town_FlowerBed_Right", CYLINDER_MESH, "MI_EP01_FoliageLight",
                2.0, 0.0, 760.0, 15.0, (4.0, 1.2, 0.3), "NoCollision"
            ),
        )
    )

    require(len(specs) == 72, f"Town environment spec count drifted: {len(specs)}")
    return tuple(specs)


def build_field_core_specs() -> tuple[dict[str, object], ...]:
    return (
        visual_spec(
            "FieldPath_0", CUBE_MESH, "MI_EP01_PathSand",
            -4.0, 0.0, 0.0, 5.0, (6.0, 4.2, 0.10), "NoCollision"
        ),
        visual_spec(
            "FieldRock_0", SPHERE_MESH, "MI_EP01_Rock",
            -3.0, 200.0, -900.0, 150.0, (3.0, 4.0, 3.0), "BlockAll"
        ),
        visual_spec(
            "FieldRock_1", SPHERE_MESH, "MI_EP01_Rock",
            -6.0, -200.0, 900.0, 200.0, (4.0, 3.0, 4.0), "BlockAll"
        ),
        visual_spec(
            "FieldMarker_Left", CYLINDER_MESH, "MI_EP01_Wood",
            -11.0, 0.0, -500.0, 250.0, (1.0, 1.0, 5.0), "BlockAll"
        ),
        visual_spec(
            "FieldMarker_Right", CYLINDER_MESH, "MI_EP01_Wood",
            -11.0, 0.0, 500.0, 250.0, (1.0, 1.0, 5.0), "BlockAll"
        ),
    )


def build_field_environment_specs() -> tuple[dict[str, object], ...]:
    specs: list[dict[str, object]] = []
    trail_points = (
        (-5.00, 0.0), (-5.52, 150.0), (-6.04, 320.0), (-6.56, 500.0),
        (-7.08, 400.0), (-7.60, 200.0), (-8.165, 0.0),
        (-12.035, 0.0), (-12.57, -200.0), (-13.06, -400.0),
        (-13.55, -500.0), (-14.04, -320.0), (-14.53, -150.0), (-15.02, 0.0),
    )
    for index, (angle, right_arc) in enumerate(trail_points):
        specs.append(
            visual_spec(
                f"Env01_Field_Trail_{index:02d}", CUBE_MESH, "MI_EP01_PathSand",
                angle, 0.0, right_arc, 4.0,
                (1.30, 4.2, 0.08), "NoCollision"
            )
        )

    specs.append(
        visual_spec(
            "Env01_Field_Pond", CYLINDER_MESH, "MI_EP01_Water",
            -10.1, 0.0, 0.0, 3.0, (6.0, 8.0, 0.06), "NoCollision"
        )
    )

    for index, forward_arc in enumerate((-220.0, -110.0, 0.0, 110.0, 220.0)):
        specs.append(
            visual_spec(
                f"Env01_Field_Bridge_Deck_{index:02d}", CUBE_MESH, "MI_EP01_Wood",
                -10.1, forward_arc, 0.0, 30.0, (1.16, 4.5, 0.22), "BlockAll"
            )
        )
    specs.extend(
        (
            visual_spec(
                "Env01_Field_Bridge_Rail_Left", CUBE_MESH, "MI_EP01_Wood",
                -10.1, 0.0, -235.0, 75.0, (5.5, 0.15, 0.8), "BlockAll"
            ),
            visual_spec(
                "Env01_Field_Bridge_Rail_Right", CUBE_MESH, "MI_EP01_Wood",
                -10.1, 0.0, 235.0, 75.0, (5.5, 0.15, 0.8), "BlockAll"
            ),
        )
    )

    tree_positions = (
        (-3.4, -600.0), (-3.8, -1450.0), (-4.4, -1250.0),
        (-5.2, 850.0), (-5.7, 1400.0), (-6.4, 1180.0),
        (-7.8, -350.0), (-8.5, -1120.0), (-10.2, 800.0), (-10.8, 1420.0),
    )
    for index, (angle, right_arc) in enumerate(tree_positions):
        foliage = "MI_EP01_FoliageDark" if index % 3 == 0 else "MI_EP01_FoliageLight"
        crown_xy = 3.2 + (index % 4) * 0.28
        crown_z = 2.30 + (index % 3) * 0.22
        trunk_scale = 0.78 + (index % 3) * 0.09
        specs.extend(
            (
                visual_spec(
                    f"Env01_Field_Tree_{index:02d}_Trunk", CYLINDER_MESH, "MI_EP01_Wood",
                    angle, 0.0, right_arc, 200.0,
                    (trunk_scale, trunk_scale, 3.7 + (index % 2) * 0.5), "BlockAll"
                ),
                visual_spec(
                    f"Env01_Field_Tree_{index:02d}_Crown", SPHERE_MESH, foliage,
                    angle, 0.0, right_arc, 475.0,
                    (crown_xy, crown_xy * 0.94, crown_z), "NoCollision"
                ),
            )
        )

    rock_positions = (
        (-4.0, -1000.0, 155.0, (6.8, 2.6, 3.1), CHAMFER_CUBE_MESH, "MI_EP01_StoneLight"),
        (-4.8, -1900.0, 205.0, (5.4, 2.2, 4.1), CHAMFER_CUBE_MESH, "MI_EP01_StoneLight"),
        (-5.8, 1300.0, 145.0, (7.2, 2.8, 2.9), CHAMFER_CUBE_MESH, "MI_EP01_StoneLight"),
        (-6.8, 1850.0, 220.0, (5.6, 2.4, 4.4), CHAMFER_CUBE_MESH, "MI_EP01_StoneLight"),
        (-8.2, -800.0, 170.0, (7.5, 2.5, 3.4), CHAMFER_CUBE_MESH, "MI_EP01_StoneLight"),
        (-9.2, 1800.0, 190.0, (6.2, 2.7, 3.8), CHAMFER_CUBE_MESH, "MI_EP01_StoneLight"),
        (-10.4, -1200.0, 85.0, (1.7, 2.0, 1.7), SPHERE_MESH, "MI_EP01_Rock"),
        (-11.2, 1250.0, 100.0, (2.0, 2.6, 2.0), SPHERE_MESH, "MI_EP01_Rock"),
    )
    for index, (angle, right_arc, up, scale, mesh, material) in enumerate(rock_positions):
        specs.append(
            visual_spec(
                f"Env01_Field_Rock_{index:02d}", mesh, material,
                angle, 0.0, right_arc, up, scale, "BlockAll"
            )
        )

    cliff_caps = (
        (-4.0, -1000.0, 316.0, (6.6, 2.4, 0.12)),
        (-4.8, -1900.0, 416.0, (5.2, 2.0, 0.12)),
        (-5.8, 1300.0, 296.0, (7.0, 2.6, 0.12)),
        (-6.8, 1850.0, 446.0, (5.4, 2.2, 0.12)),
        (-8.2, -800.0, 346.0, (7.3, 2.3, 0.12)),
        (-9.2, 1800.0, 386.0, (6.0, 2.5, 0.12)),
    )
    for index, (angle, right_arc, up, scale) in enumerate(cliff_caps):
        specs.append(
            visual_spec(
                f"Env01_Field_CliffCap_{index:02d}", CUBE_MESH, "MI_EP01_GrassDark",
                angle, 0.0, right_arc, up, scale, "NoCollision"
            )
        )

    for index, (angle, right_arc) in enumerate(((-5.2, -850.0), (-10.8, 850.0))):
        material = "MI_EP01_AccentGold" if index % 2 == 0 else "MI_EP01_RoofCoral"
        specs.append(
            visual_spec(
                f"Env01_Field_FlowerPatch_{index:02d}", SPHERE_MESH, material,
                angle, 0.0, right_arc, 24.0, (2.2, 2.2, 0.40), "NoCollision"
            )
        )

    ruin_walls = (
        (-6.6, -2600.0, 125.0, (5.0, 0.6, 2.5)),
        (-7.2, -2600.0, 125.0, (5.0, 0.6, 2.5)),
        (-6.9, -2200.0, 125.0, (0.6, 4.5, 2.5)),
        (-6.9, -3000.0, 125.0, (0.6, 4.5, 2.5)),
        (-6.5, -2600.0, 300.0, (2.0, 0.6, 2.0)),
        (-7.3, -2600.0, 300.0, (2.0, 0.6, 2.0)),
    )
    for index, (angle, right_arc, up, scale) in enumerate(ruin_walls):
        specs.append(
            visual_spec(
                f"Env01_Field_Ruin_Wall_{index:02d}", CUBE_MESH, "MI_EP01_StoneLight",
                angle, 0.0, right_arc, up, scale, "BlockAll"
            )
        )
    specs.extend(
        (
            visual_spec(
                "Env01_Field_Ruin_Pillar_Left", CYLINDER_MESH, "MI_EP01_Rock",
                -7.5, 0.0, -2350.0, 300.0, (1.0, 1.0, 6.0), "BlockAll"
            ),
            visual_spec(
                "Env01_Field_Ruin_Pillar_Right", CYLINDER_MESH, "MI_EP01_Rock",
                -7.5, 0.0, -3250.0, 300.0, (1.0, 1.0, 6.0), "BlockAll"
            ),
            visual_spec(
                "Env01_Field_Sign_Post", CUBE_MESH, "MI_EP01_Wood",
                -8.8, 0.0, 680.0, 120.0, (0.35, 0.35, 2.4), "BlockAll"
            ),
            visual_spec(
                "Env01_Field_Sign_Board", CUBE_MESH, "MI_EP01_AccentGold",
                -8.8, 0.0, 680.0, 265.0, (0.35, 2.3, 0.8), "NoCollision"
            ),
            visual_spec(
                "Env01_Field_PortalGateTop", CUBE_MESH, "MI_EP01_FoliageDark",
                -11.0, 0.0, 0.0, 510.0, (1.2, 11.0, 1.0), "NoCollision"
            ),
            visual_spec(
                "Env01_Field_FarLandmark", CONE_MESH, "MI_EP01_AccentGold",
                -10.5, 0.0, 2800.0, 600.0, (3.0, 3.0, 12.0), "NoCollision"
            ),
        )
    )

    require(len(specs) == 70, f"Field environment spec count drifted: {len(specs)}")
    return tuple(specs)


TOWN_CORE_SPECS = build_town_core_specs()
TOWN_ENVIRONMENT_SPECS = build_town_environment_specs()
FIELD_CORE_SPECS = build_field_core_specs()
FIELD_ENVIRONMENT_SPECS = build_field_environment_specs()


def validate_static_spec_contract(
    core_specs: tuple[dict[str, object], ...],
    environment_specs: tuple[dict[str, object], ...],
    expected_core_count: int,
    expected_environment_count: int,
) -> None:
    require(len(core_specs) == expected_core_count, "Core visual spec count mismatch")
    require(
        len(environment_specs) == expected_environment_count,
        "Environment visual spec count mismatch",
    )
    labels = [str(spec["label"]) for spec in core_specs + environment_specs]
    require(len(labels) == len(set(labels)), "Visual spec labels are duplicated")


def required_subsystems() -> tuple[
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


def current_world_checked(expected_package: str) -> unreal.World:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor_subsystem is not None, "UnrealEditorSubsystem unavailable")
    world = editor_subsystem.get_editor_world()
    require(world is not None, "Editor World unavailable")
    actual_package = str(world.get_outermost().get_name())
    require(
        actual_package == expected_package,
        f"Editor World mismatch: actual={actual_package}, expected={expected_package}",
    )
    return world


def actors_by_label(actor_subsystem: unreal.EditorActorSubsystem) -> dict[str, unreal.Actor]:
    actors = list(actor_subsystem.get_all_level_actors())
    require(all(actor is not None for actor in actors), "Current map contains a null Actor")
    result: dict[str, unreal.Actor] = {}
    for actor in actors:
        label = str(actor.get_actor_label())
        require(label not in result, f"Duplicate Actor label: {label}")
        result[label] = actor
    return result


def classify_mode(
    asset_subsystem: unreal.EditorAssetSubsystem,
    level_subsystem: unreal.LevelEditorSubsystem,
    actor_subsystem: unreal.EditorActorSubsystem,
) -> str:
    material_presence: list[bool] = []
    for path in MATERIAL_PATHS.values():
        registry_present = bool(asset_subsystem.does_asset_exist(path))
        disk_present = os.path.isfile(package_disk_path(path, ".uasset"))
        require(
            registry_present == disk_present,
            f"Material registry/disk presence disagreement: {path}",
        )
        material_presence.append(registry_present)
    material_count = sum(1 for present in material_presence if present)
    require(
        material_count in (0, len(MATERIAL_PATHS)),
        f"Partial Pass01 material state: {material_count}/{len(MATERIAL_PATHS)}",
    )

    map_states: list[tuple[str, int, set[str]]] = []
    for map_path in (TOWN_MAP, FIELD_MAP):
        require(level_subsystem.load_level(map_path), f"Could not load regional map: {map_path}")
        current_world_checked(map_path)
        by_label = actors_by_label(actor_subsystem)
        map_states.append((map_path, len(by_label), set(by_label)))
        del by_label
        gc.collect()
        unreal.collect_garbage()

    town_env_labels = {str(spec["label"]) for spec in TOWN_ENVIRONMENT_SPECS}
    field_env_labels = {str(spec["label"]) for spec in FIELD_ENVIRONMENT_SPECS}
    town_path, town_count, town_labels = map_states[0]
    field_path, field_count, field_labels = map_states[1]
    require(town_path == TOWN_MAP and field_path == FIELD_MAP, "Map state ordering drifted")

    baseline = (
        material_count == 0
        and town_count == 14
        and town_labels == TOWN_BASE_LABELS
        and field_count == 13
        and field_labels == FIELD_BASE_LABELS
    )
    applied = (
        material_count == len(MATERIAL_PATHS)
        and town_count == 86
        and town_labels == TOWN_BASE_LABELS | town_env_labels
        and field_count == 83
        and field_labels == FIELD_BASE_LABELS | field_env_labels
    )
    require(
        baseline or applied,
        "Town/Field are neither the exact verified skeleton nor exact Environment Pass01",
    )
    mode = "apply" if baseline else "already-exact"
    requested_mode = os.environ.get("EVERPLANET_ENV_P01_EXPECTED_MODE", "")
    require(
        requested_mode in ("apply", "already-exact"),
        "EVERPLANET_ENV_P01_EXPECTED_MODE is missing or invalid",
    )
    require(
        mode == requested_mode,
        f"Requested mode does not match exact content state: requested={requested_mode}, actual={mode}",
    )
    info(
        "FIRST_PLANET_ENV_P01_PREFLIGHT="
        f"mode={mode},townActors={town_count},fieldActors={field_count},"
        f"materials={material_count},state=exact"
    )
    return mode


def create_materials(asset_subsystem: unreal.EditorAssetSubsystem) -> None:
    parent = load_asset(FLAT_PARENT_MATERIAL)
    vector_parameter_names = {
        str(name)
        for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(parent)
    }
    scalar_parameter_names = {
        str(name)
        for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(parent)
    }
    require(
        "Base Color" in vector_parameter_names,
        f"Parent vector parameter missing: {FLAT_PARENT_MATERIAL} :: Base Color",
    )
    require(
        "Roughness" in scalar_parameter_names,
        f"Parent scalar parameter missing: {FLAT_PARENT_MATERIAL} :: Roughness",
    )
    info(
        "FIRST_PLANET_ENV_P01_PREFLIGHT_MATERIAL_PARAMETERS="
        "vector=Base Color,scalar=Roughness,status=resolved"
    )

    asset_subsystem.make_directory(MATERIAL_DIRECTORY)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    require(asset_tools is not None, "AssetTools unavailable")

    for name, color, roughness in MATERIAL_DEFINITIONS:
        asset_path = MATERIAL_PATHS[name]
        require(not asset_subsystem.does_asset_exist(asset_path), f"Material already exists: {asset_path}")
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = asset_tools.create_asset(
            name,
            MATERIAL_DIRECTORY,
            unreal.MaterialInstanceConstant.static_class(),
            factory,
        )
        require(instance is not None, f"Material instance creation failed: {asset_path}")
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
        require(
            object_path(instance.get_editor_property("parent"))
            == canonical_object_path(FLAT_PARENT_MATERIAL),
            f"Material parent assignment failed: {asset_path}",
        )

        # UE 5.8's MaterialEditingLibrary setters apply the value but return false
        # unconditionally.  Treat their documented bool as unusable here and prove
        # the postcondition through override flags plus immediate read-back instead.
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance,
            "Base Color",
            unreal.LinearColor(*color),
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            instance,
            "Roughness",
            float(roughness),
        )
        unreal.MaterialEditingLibrary.update_material_instance(instance)

        require(
            bool(
                unreal.MaterialEditingLibrary.is_material_instance_parameter_overridden(
                    instance, "Base Color"
                )
            ),
            f"Base Color override missing after assignment: {asset_path}",
        )
        require(
            bool(
                unreal.MaterialEditingLibrary.is_material_instance_parameter_overridden(
                    instance, "Roughness"
                )
            ),
            f"Roughness override missing after assignment: {asset_path}",
        )

        actual_color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
            instance,
            "Base Color",
        )
        actual_color_values = (
            float(actual_color.r),
            float(actual_color.g),
            float(actual_color.b),
            float(actual_color.a),
        )
        require(
            all(
                abs(actual - expected) <= 1.0e-4
                for actual, expected in zip(actual_color_values, color)
            ),
            f"Base Color read-back mismatch: {asset_path}",
        )
        actual_roughness = float(
            unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
                instance,
                "Roughness",
            )
        )
        require(
            math.isclose(actual_roughness, roughness, abs_tol=1.0e-4),
            f"Roughness read-back mismatch: {asset_path}",
        )
        require(asset_subsystem.save_asset(asset_path, False), f"Material save failed: {asset_path}")
        info(
            "FIRST_PLANET_ENV_P01_CREATE_MATERIAL="
            f"asset={asset_path},parent={FLAT_PARENT_MATERIAL},status=success"
        )
        info(f"FIRST_PLANET_ENV_P01_SAVE=asset={asset_path},type=MaterialInstance,status=success")


def set_actor_transform(actor: unreal.Actor, spec: dict[str, object]) -> None:
    location = unreal.Vector(*spec["location"])
    rotation_values = spec["rotation"]
    rotation = unreal.Rotator(
        pitch=rotation_values[0],
        yaw=rotation_values[1],
        roll=rotation_values[2],
    )
    # Transform mutators can report an operation result rather than the final
    # state (and static registered components reject movement).  The caller
    # temporarily makes the root mesh movable; prove all three postconditions
    # instead of treating the return values as authoritative success flags.
    actor.set_actor_location(location, False, True)
    actor.set_actor_rotation(rotation, True)
    actor.set_actor_scale3d(unreal.Vector(*spec["scale"]))
    require(
        vector_close(actor.get_actor_location(), spec["location"], TRANSFORM_TOLERANCE),
        f"Location read-back mismatch: {spec['label']}",
    )
    require(
        rotator_close(actor.get_actor_rotation(), spec["rotation"]),
        f"Rotation read-back mismatch: {spec['label']}",
    )
    require(
        vector_close(actor.get_actor_scale3d(), spec["scale"], 1.0e-4),
        f"Scale read-back mismatch: {spec['label']}",
    )


def configure_static_actor(
    actor: unreal.Actor,
    spec: dict[str, object],
    mesh_assets: dict[str, unreal.StaticMesh],
    material_assets: dict[str, unreal.MaterialInterface],
) -> None:
    label = str(spec["label"])
    require(class_path(actor) == STATIC_MESH_ACTOR_CLASS, f"Not an exact StaticMeshActor: {label}")
    component = actor.get_editor_property("static_mesh_component")
    require(component is not None, f"StaticMeshComponent missing: {label}")
    mesh_path = str(spec["mesh_path"])
    material_path = str(spec["material_path"])

    # Existing skeleton actors are registered as Static.  Make the root movable
    # while changing its world transform, then restore the exact Static contract.
    # SetStaticMesh also legitimately returns false when the requested mesh is
    # already assigned (FieldPath_0), so use exact read-back for every mutator.
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    require(
        component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE,
        f"Temporary mobility assignment failed: {label}",
    )
    component.set_static_mesh(mesh_assets[mesh_path])
    require(
        object_path(component.get_editor_property("static_mesh"))
        == canonical_object_path(mesh_path),
        f"Mesh assignment read-back mismatch: {label}",
    )
    component.set_material(0, material_assets[material_path])
    require(
        object_path(component.get_material(0)) == canonical_object_path(material_path),
        f"Material assignment read-back mismatch: {label}",
    )
    component.set_collision_profile_name(str(spec["collision_profile"]))
    require(
        str(component.get_collision_profile_name()) == str(spec["collision_profile"]),
        f"Collision assignment read-back mismatch: {label}",
    )
    set_actor_transform(actor, spec)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    require(
        component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC,
        f"Final mobility assignment failed: {label}",
    )


def spawn_static_actor(
    actor_subsystem: unreal.EditorActorSubsystem,
    static_mesh_actor_class: unreal.Class,
    spec: dict[str, object],
    mesh_assets: dict[str, unreal.StaticMesh],
    material_assets: dict[str, unreal.MaterialInterface],
) -> unreal.Actor:
    rotation_values = spec["rotation"]
    actor = actor_subsystem.spawn_actor_from_class(
        static_mesh_actor_class,
        unreal.Vector(*spec["location"]),
        unreal.Rotator(
            pitch=rotation_values[0],
            yaw=rotation_values[1],
            roll=rotation_values[2],
        ),
        False,
    )
    require(actor is not None, f"Static Actor spawn failed: {spec['label']}")
    actor.set_actor_label(str(spec["label"]), mark_dirty=True)
    require(actor.get_actor_label() == spec["label"], f"Actor label assignment failed: {spec['label']}")
    configure_static_actor(actor, spec, mesh_assets, material_assets)
    return actor


def patch_map(
    map_path: str,
    region_name: str,
    baseline_labels: set[str],
    core_specs: tuple[dict[str, object], ...],
    environment_specs: tuple[dict[str, object], ...],
    expected_before: int,
    expected_after: int,
) -> None:
    asset_subsystem, level_subsystem, actor_subsystem, _ = required_subsystems()
    require(level_subsystem.load_level(map_path), f"Map load failed before patch: {map_path}")
    current_world_checked(map_path)
    by_label = actors_by_label(actor_subsystem)
    require(len(by_label) == expected_before, f"{region_name} pre-patch Actor count mismatch")
    require(set(by_label) == baseline_labels, f"{region_name} pre-patch labels mismatch")

    planet = by_label.get("PlanetBody_0")
    require(planet is not None and class_path(planet) == PLANET_BODY_CLASS, f"{region_name} PlanetBody invalid")
    planet_mesh = planet.get_editor_property("planet_mesh")
    require(planet_mesh is not None, f"{region_name} Planet mesh component unavailable")
    planet_mesh.set_material(0, load_asset(MATERIAL_PATHS["MI_EP01_Grass"]))

    mesh_paths = {str(spec["mesh_path"]) for spec in core_specs + environment_specs}
    material_paths = {str(spec["material_path"]) for spec in core_specs + environment_specs}
    mesh_assets = {path: load_asset(path) for path in mesh_paths}
    material_assets = {path: load_asset(path) for path in material_paths}
    static_mesh_actor_class = load_class(STATIC_MESH_ACTOR_CLASS)

    for spec in core_specs:
        actor = by_label.get(str(spec["label"]))
        require(actor is not None, f"Required core visual Actor missing: {spec['label']}")
        configure_static_actor(actor, spec, mesh_assets, material_assets)

    for spec in environment_specs:
        spawn_static_actor(
            actor_subsystem,
            static_mesh_actor_class,
            spec,
            mesh_assets,
            material_assets,
        )

    after = actors_by_label(actor_subsystem)
    expected_labels = baseline_labels | {str(spec["label"]) for spec in environment_specs}
    require(len(after) == expected_after, f"{region_name} post-patch Actor count mismatch")
    require(set(after) == expected_labels, f"{region_name} post-patch labels mismatch")
    require(level_subsystem.save_current_level(), f"{region_name} map save failed")
    info(
        "FIRST_PLANET_ENV_P01_MUTATE_MAP="
        f"map={map_path},region={region_name},before={expected_before},"
        f"after={expected_after},environment={len(environment_specs)},status=success"
    )
    info(f"FIRST_PLANET_ENV_P01_SAVE=asset={map_path},type=Map,status=success")

    del by_label, after, planet, planet_mesh, mesh_assets, material_assets
    gc.collect()
    unreal.collect_garbage()


def audit_materials(asset_subsystem: unreal.EditorAssetSubsystem) -> None:
    expected_parent = canonical_object_path(FLAT_PARENT_MATERIAL)
    issues: list[str] = []
    for name, expected_color, expected_roughness in MATERIAL_DEFINITIONS:
        asset_path = MATERIAL_PATHS[name]
        try:
            require(asset_subsystem.does_asset_exist(asset_path), f"Material missing: {asset_path}")
            instance = load_asset(asset_path)
            require(
                class_path(instance) == "/Script/Engine.MaterialInstanceConstant",
                f"Material class mismatch: {asset_path}",
            )
            actual_parent = object_path(instance.get_editor_property("parent"))
            require(actual_parent == expected_parent, f"Material parent mismatch: {asset_path}")
            require(
                bool(
                    unreal.MaterialEditingLibrary.is_material_instance_parameter_overridden(
                        instance, "Base Color"
                    )
                ),
                f"Material Base Color override missing: {asset_path}",
            )
            require(
                bool(
                    unreal.MaterialEditingLibrary.is_material_instance_parameter_overridden(
                        instance, "Roughness"
                    )
                ),
                f"Material Roughness override missing: {asset_path}",
            )
            actual_color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
                instance,
                "Base Color",
            )
            color_values = (
                float(actual_color.r),
                float(actual_color.g),
                float(actual_color.b),
                float(actual_color.a),
            )
            require(
                all(abs(left - right) <= 1.0e-4 for left, right in zip(color_values, expected_color)),
                f"Material Base Color mismatch: {asset_path}",
            )
            actual_roughness = float(
                unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
                    instance,
                    "Roughness",
                )
            )
            require(
                math.isclose(actual_roughness, expected_roughness, abs_tol=1.0e-4),
                f"Material Roughness mismatch: {asset_path}",
            )
            disk_path = package_disk_path(asset_path, ".uasset")
            require(os.path.isfile(disk_path), f"Material package missing on disk: {disk_path}")
            digest = sha256_file(disk_path)
            info(
                "FIRST_PLANET_ENV_P01_AUDIT_MATERIAL="
                f"asset={asset_path},parent=exact,color=exact,roughness=exact,"
                f"sha256={digest},status=passed"
            )
        except Exception as exc:
            issue = f"material={asset_path},error={type(exc).__name__}: {exc}"
            issues.append(issue)
            info(f"FIRST_PLANET_ENV_P01_AUDIT_MISMATCH={issue}")

    require(not issues, f"Material audit mismatches ({len(issues)}): {' | '.join(issues)}")


def audit_static_actor(actor: unreal.Actor, spec: dict[str, object]) -> None:
    label = str(spec["label"])
    require(class_path(actor) == STATIC_MESH_ACTOR_CLASS, f"Static Actor class mismatch: {label}")
    require(vector_close(actor.get_actor_location(), spec["location"], TRANSFORM_TOLERANCE), f"Location mismatch: {label}")
    require(rotator_close(actor.get_actor_rotation(), spec["rotation"]), f"Rotation mismatch: {label}")
    require(vector_close(actor.get_actor_scale3d(), spec["scale"], 1.0e-4), f"Scale mismatch: {label}")
    component = actor.get_editor_property("static_mesh_component")
    require(component is not None, f"StaticMeshComponent missing during audit: {label}")
    require(
        object_path(component.get_editor_property("static_mesh"))
        == canonical_object_path(str(spec["mesh_path"])),
        f"Static mesh mismatch: {label}",
    )
    require(
        object_path(component.get_material(0))
        == canonical_object_path(str(spec["material_path"])),
        f"Material mismatch: {label}",
    )
    require(
        str(component.get_collision_profile_name()) == str(spec["collision_profile"]),
        f"Collision profile mismatch: {label}",
    )
    require(
        component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC,
        f"Mobility mismatch: {label}",
    )


def actor_count_of_exact_class(actors: list[unreal.Actor], expected_class_path: str) -> int:
    return sum(1 for actor in actors if class_path(actor) == expected_class_path)


def expected_spawn_distance() -> float:
    character_class = unreal.EditorAssetLibrary.load_blueprint_class(PRODUCT_CHARACTER)
    require(character_class is not None, "Product Character generated class unavailable")
    defaults = unreal.get_default_object(character_class)
    require(defaults is not None, "Product Character defaults unavailable")
    capsule = defaults.get_editor_property("capsule_component")
    require(capsule is not None, "Product Character capsule unavailable")
    return PLANET_RADIUS_CM + float(capsule.get_scaled_capsule_half_height()) + SPAWN_CLEARANCE_CM


def audit_actor_surface_transform(
    actor: unreal.Actor,
    angle_degrees: float,
    radial_distance: float,
    label: str,
) -> None:
    angle = math.radians(angle_degrees)
    expected_location = (
        math.sin(angle) * radial_distance,
        0.0,
        math.cos(angle) * radial_distance,
    )
    expected_rotation = (-angle_degrees, 0.0, 0.0)
    require(vector_close(actor.get_actor_location(), expected_location, TRANSFORM_TOLERANCE), f"Core location mismatch: {label}")
    require(rotator_close(actor.get_actor_rotation(), expected_rotation), f"Core rotation mismatch: {label}")
    require(vector_close(actor.get_actor_scale3d(), (1.0, 1.0, 1.0), 1.0e-4), f"Core scale mismatch: {label}")


def audit_map(
    map_path: str,
    region_name: str,
    baseline_labels: set[str],
    core_specs: tuple[dict[str, object], ...],
    environment_specs: tuple[dict[str, object], ...],
    expected_actor_count: int,
    expected_static_count: int,
) -> None:
    _, level_subsystem, actor_subsystem, _ = required_subsystems()
    require(level_subsystem.load_level(map_path), f"Map reload failed: {map_path}")
    world = current_world_checked(map_path)
    by_label = actors_by_label(actor_subsystem)
    actors = list(by_label.values())
    expected_labels = baseline_labels | {str(spec["label"]) for spec in environment_specs}
    require(len(actors) == expected_actor_count, f"{region_name} Actor count mismatch")
    require(set(by_label) == expected_labels, f"{region_name} exact label contract mismatch")
    require(
        actor_count_of_exact_class(actors, STATIC_MESH_ACTOR_CLASS) == expected_static_count,
        f"{region_name} StaticMeshActor count mismatch",
    )
    require(actor_count_of_exact_class(actors, PLANET_BODY_CLASS) == 1, f"{region_name} PlanetBody count mismatch")
    require(actor_count_of_exact_class(actors, REGION_DESCRIPTOR_CLASS) == 1, f"{region_name} descriptor count mismatch")
    require(actor_count_of_exact_class(actors, REGION_PORTAL_CLASS) == 1, f"{region_name} portal count mismatch")
    require(actor_count_of_exact_class(actors, DIRECTIONAL_LIGHT_CLASS) == 2, f"{region_name} light count mismatch")
    require(actor_count_of_exact_class(actors, SKY_LIGHT_CLASS) == 1, f"{region_name} SkyLight count mismatch")
    require(actor_count_of_exact_class(actors, SKY_ATMOSPHERE_CLASS) == 1, f"{region_name} atmosphere count mismatch")

    expected_game_mode = canonical_object_path(REGIONAL_GAME_MODE) + "_C"
    world_settings = world.get_world_settings()
    require(world_settings is not None, f"{region_name} WorldSettings unavailable")
    require(
        object_path(world_settings.get_editor_property("default_game_mode")) == expected_game_mode,
        f"{region_name} GameMode override mismatch",
    )

    planet = by_label["PlanetBody_0"]
    require(math.isclose(float(planet.get_surface_radius()), PLANET_RADIUS_CM, abs_tol=1.0e-4), f"{region_name} radius mismatch")
    planet_mesh = planet.get_editor_property("planet_mesh")
    require(planet_mesh is not None, f"{region_name} planet mesh unavailable")
    require(
        object_path(planet_mesh.get_material(0)) == canonical_object_path(MATERIAL_PATHS["MI_EP01_Grass"]),
        f"{region_name} planet material mismatch",
    )

    for spec in core_specs + environment_specs:
        actor = by_label.get(str(spec["label"]))
        require(actor is not None, f"Visual Actor missing during audit: {spec['label']}")
        audit_static_actor(actor, spec)

    spawn_distance = expected_spawn_distance()
    if region_name == "Town":
        require(actor_count_of_exact_class(actors, REGION_START_CLASS) == 2, "Town regional start count mismatch")
        descriptor = by_label[TOWN_DESCRIPTOR_LABEL]
        require(str(descriptor.get_editor_property("planet_id")) == "planet.first", "Town descriptor PlanetId mismatch")
        require(str(descriptor.get_editor_property("region_id")) == "region.first.town", "Town descriptor RegionId mismatch")
        start_contracts = (
            (TOWN_START_LABEL, "spawn.town.start", 0.0),
            (TOWN_RETURN_LABEL, "spawn.town.from_field", 8.0),
        )
        portal_label = TOWN_PORTAL_LABEL
        portal_id = "portal.town.east_gate"
        link_id = "link.first.town_to_field"
        portal_angle = 12.0
    else:
        require(actor_count_of_exact_class(actors, REGION_START_CLASS) == 1, "Field regional start count mismatch")
        descriptor = by_label[FIELD_DESCRIPTOR_LABEL]
        require(str(descriptor.get_editor_property("planet_id")) == "planet.first", "Field descriptor PlanetId mismatch")
        require(str(descriptor.get_editor_property("region_id")) == "region.first.field", "Field descriptor RegionId mismatch")
        start_contracts = ((FIELD_START_LABEL, "spawn.field.from_town", -8.0),)
        portal_label = FIELD_PORTAL_LABEL
        portal_id = "portal.field.west_gate"
        link_id = "link.first.field_to_town"
        portal_angle = -12.0

    require(object_path(descriptor.get_editor_property("world_catalog")) == canonical_object_path(CATALOG), f"{region_name} descriptor Catalog mismatch")
    for label, anchor_id, angle in start_contracts:
        start = by_label[label]
        require(str(start.get_editor_property("spawn_anchor_id")) == anchor_id, f"Spawn anchor mismatch: {label}")
        audit_actor_surface_transform(start, angle, spawn_distance, label)
    portal = by_label[portal_label]
    require(str(portal.get_editor_property("portal_id")) == portal_id, f"Portal ID mismatch: {portal_label}")
    require(str(portal.get_editor_property("travel_link_id")) == link_id, f"Portal link mismatch: {portal_label}")
    audit_actor_surface_transform(portal, portal_angle, PLANET_RADIUS_CM, portal_label)

    info(f"FIRST_PLANET_ENV_P01_MAP_CHECK_BEGIN=map={map_path}")
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK DONTDISPLAYDIALOG")
    info(f"FIRST_PLANET_ENV_P01_MAP_CHECK_END=map={map_path}")

    map_extension = ".umap"
    disk_path = package_disk_path(map_path, map_extension)
    digest = sha256_file(disk_path)
    info(
        "FIRST_PLANET_ENV_P01_AUDIT_MAP="
        f"map={map_path},region={region_name},actors={expected_actor_count},"
        f"static={expected_static_count},environment={len(environment_specs)},"
        f"sha256={digest},status=passed"
    )

    del by_label, actors, world, world_settings, planet, planet_mesh, descriptor, portal
    gc.collect()
    unreal.collect_garbage()


def main() -> None:
    validate_static_spec_contract(TOWN_CORE_SPECS, TOWN_ENVIRONMENT_SPECS, 5, 72)
    validate_static_spec_contract(FIELD_CORE_SPECS, FIELD_ENVIRONMENT_SPECS, 5, 70)

    for class_path_name in (
        PLANET_BODY_CLASS,
        REGION_DESCRIPTOR_CLASS,
        REGION_START_CLASS,
        REGION_PORTAL_CLASS,
        STATIC_MESH_ACTOR_CLASS,
        DIRECTIONAL_LIGHT_CLASS,
        SKY_LIGHT_CLASS,
        SKY_ATMOSPHERE_CLASS,
    ):
        load_class(class_path_name)
    for asset_path in (
        PRODUCT_CHARACTER,
        PRODUCT_GAME_MODE,
        REGIONAL_GAME_MODE,
        CATALOG,
        PRODUCT_GRAYBOX,
        FLAT_PARENT_MATERIAL,
        CUBE_MESH,
        CYLINDER_MESH,
        SPHERE_MESH,
        CONE_MESH,
        CHAMFER_CUBE_MESH,
    ):
        load_asset(asset_path)
    info("FIRST_PLANET_ENV_P01_PREFLIGHT_API=classes=8,sourceAssets=11,status=resolved")

    town_disk_path = package_disk_path(TOWN_MAP, ".umap")
    field_disk_path = package_disk_path(FIELD_MAP, ".umap")
    require(os.path.isfile(town_disk_path), f"Town map missing on disk: {town_disk_path}")
    require(os.path.isfile(field_disk_path), f"Field map missing on disk: {field_disk_path}")

    protected_before = snapshot_protected_hashes("FIRST_PLANET_ENV_P01_PREFLIGHT_PROTECTED_SHA256")
    asset_subsystem, level_subsystem, actor_subsystem, _ = required_subsystems()
    mode = classify_mode(asset_subsystem, level_subsystem, actor_subsystem)

    if mode == "apply":
        require(sha256_file(town_disk_path) == TOWN_BASELINE_SHA256, "Town baseline SHA-256 mismatch")
        require(sha256_file(field_disk_path) == FIELD_BASELINE_SHA256, "Field baseline SHA-256 mismatch")
        info(
            "FIRST_PLANET_ENV_P01_PREFLIGHT_BASELINE_SHA256="
            f"town={TOWN_BASELINE_SHA256},field={FIELD_BASELINE_SHA256},status=exact"
        )
        create_materials(asset_subsystem)
        patch_map(
            TOWN_MAP,
            "Town",
            TOWN_BASE_LABELS,
            TOWN_CORE_SPECS,
            TOWN_ENVIRONMENT_SPECS,
            14,
            86,
        )
        patch_map(
            FIELD_MAP,
            "Field",
            FIELD_BASE_LABELS,
            FIELD_CORE_SPECS,
            FIELD_ENVIRONMENT_SPECS,
            13,
            83,
        )
        result_mode = "applied"
    else:
        result_mode = "already-exact"

    audit_issues: list[str] = []
    audit_steps = (
        ("materials", lambda: audit_materials(asset_subsystem)),
        (
            "town",
            lambda: audit_map(
                TOWN_MAP,
                "Town",
                TOWN_BASE_LABELS,
                TOWN_CORE_SPECS,
                TOWN_ENVIRONMENT_SPECS,
                86,
                77,
            ),
        ),
        (
            "field",
            lambda: audit_map(
                FIELD_MAP,
                "Field",
                FIELD_BASE_LABELS,
                FIELD_CORE_SPECS,
                FIELD_ENVIRONMENT_SPECS,
                83,
                75,
            ),
        ),
    )
    for scope, audit_step in audit_steps:
        try:
            audit_step()
        except Exception as exc:
            issue = f"scope={scope},error={type(exc).__name__}: {exc}"
            audit_issues.append(issue)
            info(f"FIRST_PLANET_ENV_P01_AUDIT_MISMATCH={issue}")

    try:
        protected_after = snapshot_protected_hashes("FIRST_PLANET_ENV_P01_AUDIT_PROTECTED_SHA256")
        require(protected_before == protected_after, "Protected package SHA-256 changed")
        info("FIRST_PLANET_ENV_P01_AUDIT_PROTECTED_SHA256_UNCHANGED=True assets=7")
    except Exception as exc:
        issue = f"scope=protected,error={type(exc).__name__}: {exc}"
        audit_issues.append(issue)
        info(f"FIRST_PLANET_ENV_P01_AUDIT_MISMATCH={issue}")

    info(f"FIRST_PLANET_ENV_P01_AUDIT_FAILURES=count={len(audit_issues)}")
    require(
        not audit_issues,
        f"Environment Pass01 audit failed in {len(audit_issues)} scope(s): {' | '.join(audit_issues)}",
    )
    info(
        "FIRST_PLANET_ENV_P01_AUDIT="
        "version=1,materials=13,town=86/77/72,field=83/75/70,"
        "catalog=unchanged,gameModes=unchanged,character=unchanged,graybox=unchanged,status=passed"
    )
    info(
        "FIRST_PLANET_ENV_P01_RESULT=VERIFIED "
        f"mode={result_mode},version={PASS_VERSION},materials=13,townActors=86,fieldActors=83"
    )


failure_message: str | None = None
try:
    main()
except Exception as exc:
    failure_message = f"{type(exc).__module__}.{type(exc).__qualname__}: {exc}"

if failure_message is not None:
    unreal.log_error(
        "[EverPlanet First Planet Env P01] "
        f"FIRST_PLANET_ENV_P01_FAILURE=error={failure_message}"
    )
    unreal.log_error(
        "[EverPlanet First Planet Env P01] "
        "FIRST_PLANET_ENV_P01_RESULT=FAILED"
    )
    raise RuntimeError(failure_message)
