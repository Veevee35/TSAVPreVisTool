"""Create the bundled TSAV LED materials and NDI builder demonstration level.

Run from Unreal's Python console or with UnrealEditor-Cmd -ExecutePythonScript.
The script is intentionally idempotent for the exact assets it owns.
"""

import traceback

import unreal


MATERIAL_PATH = "/TSAVLEDTools/Materials"
VIDEO_MATERIAL = f"{MATERIAL_PATH}/M_TSAV_LEDCanvasVideo"
FRAME_MATERIAL = f"{MATERIAL_PATH}/M_TSAV_LEDCanvasFrame"
PANEL_PROFILE_PATH = "/TSAVLEDTools/PanelDefinitions"
PANEL_PROFILE = f"{PANEL_PROFILE_PATH}/DA_TSAV_500mm_128px"
LEVEL_PATH = "/Game/TSAV/Levels/LED_Canvas_Configurator"


def _asset_name(asset_path):
    return asset_path.rsplit("/", 1)[-1]


def _create_or_load_material(asset_path):
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(existing)
        return existing

    factory = unreal.MaterialFactoryNew()
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        _asset_name(asset_path),
        asset_path.rsplit("/", 1)[0],
        unreal.Material,
        factory,
    )


def _build_video_material():
    material = _create_or_load_material(VIDEO_MATERIAL)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    texture = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -420, -40
    )
    texture.set_editor_property("parameter_name", "MediaTexture")
    texture.set_editor_property(
        "texture", unreal.EditorAssetLibrary.load_asset("/Engine/EngineResources/DefaultTexture")
    )

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -950, -120
    )

    scale_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -950, 40
    )
    scale_x.set_editor_property("parameter_name", "CanvasScaleX")
    scale_x.set_editor_property("default_value", 1.0)

    scale_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -950, 130
    )
    scale_y.set_editor_property("parameter_name", "CanvasScaleY")
    scale_y.set_editor_property("default_value", 1.0)

    scale = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -690, 60
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(scale_x, "", scale, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(scale_y, "", scale, "B")

    scaled_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -480, -110
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", scaled_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(scale, "", scaled_uv, "B")

    offset_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -950, 250
    )
    offset_x.set_editor_property("parameter_name", "CanvasOffsetX")
    offset_x.set_editor_property("default_value", 0.0)

    offset_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -950, 340
    )
    offset_y.set_editor_property("parameter_name", "CanvasOffsetY")
    offset_y.set_editor_property("default_value", 0.0)

    offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -690, 280
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(offset_x, "", offset, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(offset_y, "", offset, "B")

    mapped_uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -260, -100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(scaled_uv, "", mapped_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(offset, "", mapped_uv, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(mapped_uv, "", texture, "UVs")

    strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -420, 150
    )
    strength.set_editor_property("parameter_name", "EmissiveStrength")
    strength.set_editor_property("default_value", 3.0)

    brightness_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -120, 20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(texture, "RGB", brightness_multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(strength, "", brightness_multiply, "B")

    canvas_visible = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -120, 180
    )
    canvas_visible.set_editor_property("parameter_name", "CanvasVisible")
    canvas_visible.set_editor_property("default_value", 1.0)

    output_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 100, 30
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        brightness_multiply, "", output_multiply, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        canvas_visible, "", output_multiply, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        output_multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def _build_panel_definition():
    definition = unreal.EditorAssetLibrary.load_asset(PANEL_PROFILE)
    if not definition:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TSAVLEDPanelDefinition)
        definition = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            _asset_name(PANEL_PROFILE),
            PANEL_PROFILE.rsplit("/", 1)[0],
            unreal.TSAVLEDPanelDefinition,
            factory,
        )

    definition.set_editor_property("model_name", "TSAV 500 mm / 128 px Cabinet")
    definition.set_editor_property("width_cm", 50.0)
    definition.set_editor_property("height_cm", 50.0)
    definition.set_editor_property("depth_cm", 8.0)
    definition.set_editor_property("bezel_cm", 0.5)
    definition.set_editor_property("resolution_x", 128)
    definition.set_editor_property("resolution_y", 128)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    return definition


def _build_frame_material():
    material = _create_or_load_material(FRAME_MATERIAL)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -300, -20
    )
    color.set_editor_property("constant", unreal.LinearColor(0.006, 0.008, 0.012, 1.0))

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -300, 130
    )
    roughness.set_editor_property("r", 0.62)

    metallic = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -300, 230
    )
    metallic.set_editor_property("r", 0.65)

    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic, "", unreal.MaterialProperty.MP_METALLIC
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def _load_first(paths):
    for path in paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset:
            return asset
    return None


def _set_properties(actor, **properties):
    for name, value in properties.items():
        actor.set_editor_property(name, value)


def _spawn(actor_subsystem, actor_class, label, location, rotation=None):
    actor = actor_subsystem.spawn_actor_from_class(
        actor_class,
        location,
        rotation or unreal.Rotator(),
        transient=False,
    )
    actor.set_actor_label(label)
    return actor


def _build_level(video_material, frame_material, panel_definition):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        if not level_subsystem.load_level(LEVEL_PATH):
            raise RuntimeError(f"Could not load existing level {LEVEL_PATH}")
        existing_actors = actor_subsystem.get_all_level_actors()
        if existing_actors:
            actor_subsystem.destroy_actors(existing_actors)
    elif not level_subsystem.new_level(LEVEL_PATH):
        raise RuntimeError(f"Could not create level {LEVEL_PATH}")

    wall_source = _load_first(
        ["/Game/CenterMainNDI", "/Game/SLMainNDI", "/Game/SRMainNDI"]
    )
    left_source = _load_first(["/Game/SL1NDI", "/Game/SLMainNDI", "/Game/CenterMainNDI"])
    right_source = _load_first(["/Game/SR1NDI", "/Game/SRMainNDI", "/Game/CenterMainNDI"])

    wall = _spawn(
        actor_subsystem,
        unreal.TSAVLEDWall,
        "LED Wall Builder - Center NDI",
        unreal.Vector(0.0, 0.0, 235.0),
    )
    _set_properties(
        wall,
        columns=8,
        rows=4,
        panel_definition=panel_definition,
        use_panel_definition=True,
        panel_gap_cm=0.6,
        wall_depth_cm=12.0,
        border_cm=2.0,
        show_panel_seams=True,
        canvas_resolution=unreal.IntPoint(4096, 2160),
        canvas_position=unreal.IntPoint(512, 256),
        use_canvas_mapping=True,
        media_source=wall_source,
        display_material=video_material,
        frame_material=frame_material,
        emissive_strength=3.0,
        play_in_editor=False,
    )

    left_panel = _spawn(
        actor_subsystem,
        unreal.TSAVLEDPanel,
        "LED Panel - Stage Left NDI",
        unreal.Vector(10.0, -340.0, 100.0),
    )
    _set_properties(
        left_panel,
        width_cm=100.0,
        height_cm=175.0,
        depth_cm=8.0,
        bezel_cm=1.5,
        resolution_x=256,
        resolution_y=448,
        canvas_resolution=unreal.IntPoint(4096, 2160),
        canvas_position=unreal.IntPoint(0, 800),
        use_canvas_mapping=True,
        media_source=left_source,
        display_material=video_material,
        frame_material=frame_material,
        emissive_strength=3.0,
        play_in_editor=False,
    )

    right_panel = _spawn(
        actor_subsystem,
        unreal.TSAVLEDPanel,
        "LED Panel - Stage Right NDI",
        unreal.Vector(10.0, 340.0, 100.0),
    )
    _set_properties(
        right_panel,
        width_cm=100.0,
        height_cm=175.0,
        depth_cm=8.0,
        bezel_cm=1.5,
        resolution_x=256,
        resolution_y=448,
        canvas_resolution=unreal.IntPoint(4096, 2160),
        canvas_position=unreal.IntPoint(3840, 800),
        use_canvas_mapping=True,
        media_source=right_source,
        display_material=video_material,
        frame_material=frame_material,
        emissive_strength=3.0,
        play_in_editor=False,
    )

    floor = _spawn(
        actor_subsystem,
        unreal.StaticMeshActor,
        "Previs Floor",
        unreal.Vector(300.0, 0.0, -10.0),
    )
    floor.static_mesh_component.set_static_mesh(
        unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    )
    floor.static_mesh_component.set_material(0, frame_material)
    floor.set_actor_scale3d(unreal.Vector(12.0, 9.0, 0.2))

    key_light = _spawn(
        actor_subsystem,
        unreal.DirectionalLight,
        "Key Light",
        unreal.Vector(300.0, 0.0, 500.0),
        unreal.Rotator(-35.0, -35.0, 0.0),
    )
    key_light.light_component.set_editor_property("intensity", 4.0)

    sky_light = _spawn(
        actor_subsystem,
        unreal.SkyLight,
        "Ambient Sky Light",
        unreal.Vector(0.0, 0.0, 450.0),
    )
    sky_light.light_component.set_editor_property("intensity", 0.7)

    camera = _spawn(
        actor_subsystem,
        unreal.CameraActor,
        "LED Wall Preview Camera",
        unreal.Vector(950.0, 0.0, 245.0),
        unreal.Rotator(0.0, 180.0, 0.0),
    )
    camera.camera_component.set_editor_property("field_of_view", 52.0)

    if not level_subsystem.save_current_level():
        raise RuntimeError(f"Could not save level {LEVEL_PATH}")

    source_summary = {
        "wall": wall_source.get_path_name() if wall_source else "None",
        "left": left_source.get_path_name() if left_source else "None",
        "right": right_source.get_path_name() if right_source else "None",
    }
    wall_resolution = wall.get_wall_resolution_pixels()
    panel_links = wall.get_editor_property("panel_links")
    unreal.log(
        "CODEX_LED_LEVEL_SUCCESS "
        f"level={LEVEL_PATH} actors={len(actor_subsystem.get_all_level_actors())} "
        f"wall_resolution={wall_resolution.x}x{wall_resolution.y} "
        f"canvas_position=512,256 canvas_resolution=4096x2160 "
        f"panel_links={len(panel_links)} canvas_valid={wall.is_canvas_mapping_valid()} "
        f"sources={source_summary}"
    )


def main():
    unreal.log("CODEX_LED_LEVEL_START")
    unreal.EditorAssetLibrary.make_directory(MATERIAL_PATH)
    unreal.EditorAssetLibrary.make_directory(PANEL_PROFILE_PATH)
    unreal.EditorAssetLibrary.make_directory(LEVEL_PATH.rsplit("/", 1)[0])
    video_material = _build_video_material()
    frame_material = _build_frame_material()
    panel_definition = _build_panel_definition()
    _build_level(video_material, frame_material, panel_definition)


try:
    main()
except Exception:
    unreal.log_error("CODEX_LED_LEVEL_FAILED\n" + traceback.format_exc())
    raise
