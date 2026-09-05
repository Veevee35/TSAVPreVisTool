"""Create the original TSAV instanced laser preview material without saving any map."""
import unreal

path = "/Game/TSAV/Materials/M_TSAVLaserPreview"
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.log("TSAV_LASER_MATERIAL_ALREADY_EXISTS")
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_TSAVLaserPreview", "/Game/TSAV/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    edit = unreal.MaterialEditingLibrary
    channels = []
    for index in range(3):
        node = edit.create_material_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -800, index*150)
        node.set_editor_property("data_index", index)
        node.set_editor_property("const_default_value", 1.0)
        channels.append(node)
    rg = edit.create_material_expression(material, unreal.MaterialExpressionAppendVector, -550, 0)
    rgb = edit.create_material_expression(material, unreal.MaterialExpressionAppendVector, -350, 0)
    assert edit.connect_material_expressions(channels[0], "", rg, "A")
    assert edit.connect_material_expressions(channels[1], "", rg, "B")
    assert edit.connect_material_expressions(rg, "", rgb, "A")
    assert edit.connect_material_expressions(channels[2], "", rgb, "B")
    interpolate = edit.create_material_expression(material, unreal.MaterialExpressionVertexInterpolator, -150, 0)
    assert edit.connect_material_expressions(rgb, "", interpolate, "VS")
    brightness = edit.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -150, 200)
    brightness.set_editor_property("parameter_name", "Brightness")
    brightness.set_editor_property("default_value", 1.0)
    multiply = edit.create_material_expression(material, unreal.MaterialExpressionMultiply, 100, 0)
    assert edit.connect_material_expressions(interpolate, "", multiply, "A")
    assert edit.connect_material_expressions(brightness, "", multiply, "B")
    assert edit.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log("TSAV_LASER_MATERIAL_CREATED")
