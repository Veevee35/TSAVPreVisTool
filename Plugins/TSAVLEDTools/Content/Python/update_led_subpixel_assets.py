"""Import TSAV RGB subpixel masks and rebuild the LED canvas material.

This script only owns the two subpixel textures and M_TSAV_LEDCanvasVideo. It
does not modify levels or actors, so it is safe to rerun when the masks change.
"""

import os
import traceback

import unreal


MATERIAL_PATH = "/TSAVLEDTools/Materials"
VIDEO_MATERIAL = f"{MATERIAL_PATH}/M_TSAV_LEDCanvasVideo"
SUBPIXEL_PATH = "/TSAVLEDTools/Subpixels"
RECTANGLE_TEXTURE = f"{SUBPIXEL_PATH}/T_TSAV_Subpixel_RectangleRGB"
ROUND_TEXTURE = f"{SUBPIXEL_PATH}/T_TSAV_Subpixel_RoundRGB"


def _plugin_source_path(filename):
    return os.path.normpath(
        os.path.join(
            unreal.Paths.project_plugins_dir(),
            "TSAVLEDTools",
            "Content",
            "Source",
            "Subpixels",
            filename,
        )
    )


def _import_texture(filename, asset_path):
    source = _plugin_source_path(filename)
    if not os.path.isfile(source):
        raise RuntimeError(f"Missing subpixel source texture: {source}")

    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = asset_path.rsplit("/", 1)[0]
    task.destination_name = asset_path.rsplit("/", 1)[-1]
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.async_ = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        raise RuntimeError(f"Texture import failed: {source}")

    texture.set_editor_property("srgb", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def _create_or_load_material(asset_path):
    material = unreal.EditorAssetLibrary.load_asset(asset_path)
    if material:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        return material

    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_path.rsplit("/", 1)[-1],
        asset_path.rsplit("/", 1)[0],
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )


def _scalar(material, name, default, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def _append(material, a, b, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, x, y
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", node, "B")
    return node


def _binary(material, node_type, a, b, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(material, node_type, x, y)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", node, "B")
    return node


def build_assets():
    unreal.EditorAssetLibrary.make_directory(MATERIAL_PATH)
    unreal.EditorAssetLibrary.make_directory(SUBPIXEL_PATH)
    rectangle = _import_texture("Rectangle Subpixel.png", RECTANGLE_TEXTURE)
    round_rgb = _import_texture("Round Subpixel.png", ROUND_TEXTURE)

    material = _create_or_load_material(VIDEO_MATERIAL)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1700, -180
    )
    resolution_x = _scalar(material, "SurfaceResolutionX", 128.0, -1700, -40)
    resolution_y = _scalar(material, "SurfaceResolutionY", 128.0, -1700, 45)
    surface_resolution = _append(material, resolution_x, resolution_y, -1450, 0)

    # Local UV multiplied by native surface resolution. Its fractional portion
    # repeats the supplied RGB layout exactly once for every physical LED pixel.
    pixel_uv = _binary(
        material,
        unreal.MaterialExpressionMultiply,
        texcoord,
        surface_resolution,
        -1210,
        -145,
    )

    pixel_floor = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionFloor, -970, -215
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(pixel_uv, "", pixel_floor, "Input")
    half_pixel = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -970, -80
    )
    half_pixel.set_editor_property("r", 0.5)
    pixel_center = _binary(
        material,
        unreal.MaterialExpressionAdd,
        pixel_floor,
        half_pixel,
        -730,
        -190,
    )
    centered_local_uv = _binary(
        material,
        unreal.MaterialExpressionDivide,
        pixel_center,
        surface_resolution,
        -500,
        -190,
    )

    canvas_scale_x = _scalar(material, "CanvasScaleX", 1.0, -970, 80)
    canvas_scale_y = _scalar(material, "CanvasScaleY", 1.0, -970, 165)
    canvas_scale = _append(material, canvas_scale_x, canvas_scale_y, -730, 120)
    scaled_uv = _binary(
        material,
        unreal.MaterialExpressionMultiply,
        centered_local_uv,
        canvas_scale,
        -260,
        -180,
    )

    canvas_offset_x = _scalar(material, "CanvasOffsetX", 0.0, -730, 260)
    canvas_offset_y = _scalar(material, "CanvasOffsetY", 0.0, -730, 345)
    canvas_offset = _append(material, canvas_offset_x, canvas_offset_y, -500, 300)
    mapped_uv = _binary(
        material,
        unreal.MaterialExpressionAdd,
        scaled_uv,
        canvas_offset,
        -20,
        -165,
    )

    media = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 220, -205
    )
    media.set_editor_property("parameter_name", "MediaTexture")
    media.set_editor_property(
        "texture", unreal.EditorAssetLibrary.load_asset("/Engine/EngineResources/DefaultTexture")
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(mapped_uv, "", media, "UVs")

    subpixels = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -730, 500
    )
    subpixels.set_editor_property("parameter_name", "SubpixelTexture")
    subpixels.set_editor_property("texture", rectangle)
    unreal.MaterialEditingLibrary.connect_material_expressions(pixel_uv, "", subpixels, "UVs")

    masked_video = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 220, 80
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(media, "RGB", masked_video, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(subpixels, "RGB", masked_video, "B")

    subpixel_strength = _scalar(material, "SubpixelStrength", 0.0, -260, 400)
    layout_blend = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, 480, -15
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(media, "RGB", layout_blend, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(masked_video, "", layout_blend, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        subpixel_strength, "", layout_blend, "Alpha"
    )

    emissive_strength = _scalar(material, "EmissiveStrength", 3.0, 220, 280)
    bright_video = _binary(
        material,
        unreal.MaterialExpressionMultiply,
        layout_blend,
        emissive_strength,
        720,
        15,
    )
    canvas_visible = _scalar(material, "CanvasVisible", 1.0, 480, 315)
    output = _binary(
        material,
        unreal.MaterialExpressionMultiply,
        bright_video,
        canvas_visible,
        950,
        15,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        output, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log(
        "CODEX_LED_SUBPIXEL_SUCCESS "
        f"material={VIDEO_MATERIAL} rectangle={RECTANGLE_TEXTURE} round={ROUND_TEXTURE}"
    )
    return material, rectangle, round_rgb


def main():
    unreal.log("CODEX_LED_SUBPIXEL_START")
    build_assets()


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("CODEX_LED_SUBPIXEL_FAILED\n" + traceback.format_exc())
        raise
