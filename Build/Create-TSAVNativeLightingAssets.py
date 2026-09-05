"""Build TSAV's original procedural gobo/framing light function in an isolated UE editor.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> -DisablePlugins=SuperStage.
Only creates the named TSAV material; never saves a level or changes vendor content.
"""
import unreal

path = "/Game/TSAV/Materials/M_TSAVNativeGobo"
material = unreal.load_asset(path)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_TSAVNativeGobo", "/Game/TSAV/Materials", unreal.Material, unreal.MaterialFactoryNew()
    )
else:
    raise RuntimeError("Native gobo material already exists; inspect it before regenerating.")
material.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
material.set_editor_property("force_compatible_with_light_function_atlas", True)
edit = unreal.MaterialEditingLibrary
uv = edit.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -800, 0)
mask = edit.create_material_expression(material, unreal.MaterialExpressionCustom, 0, 0)
parameters = {"Iris": 1.0, "Frost": 0.0, "GoboIndex": 0.0, "GoboAngle": 0.0,
              "BladeTop": 0.0, "BladeBottom": 0.0, "BladeLeft": 0.0, "BladeRight": 0.0}
inputs = []
for name in ["UV", *parameters]:
    item = unreal.CustomInput()
    item.set_editor_property("input_name", name)
    inputs.append(item)
mask.set_editor_property("inputs", inputs)
mask.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
mask.set_editor_property("description", "Original TSAV procedural optics: iris, framing and four gobo patterns")
mask.set_editor_property("code", r"""
float2 p = (UV - 0.5) * 2.0;
float s = sin(GoboAngle), c = cos(GoboAngle);
float2 q = float2(c*p.x - s*p.y, s*p.x + c*p.y);
float edge = lerp(0.005, 0.22, saturate(Frost));
float radius = length(p);
float mask = (1.0 - smoothstep(max(0.0, Iris-edge), Iris+edge, radius)) * step(0.0001,Iris);
mask *= smoothstep(-1.0+2.0*BladeLeft-edge,-1.0+2.0*BladeLeft+edge,p.x);
mask *= 1.0-smoothstep(1.0-2.0*BladeRight-edge,1.0-2.0*BladeRight+edge,p.x);
mask *= smoothstep(-1.0+2.0*BladeBottom-edge,-1.0+2.0*BladeBottom+edge,p.y);
mask *= 1.0-smoothstep(1.0-2.0*BladeTop-edge,1.0-2.0*BladeTop+edge,p.y);
float pattern=1.0;
if (GoboIndex>0.5 && GoboIndex<1.5) pattern=smoothstep(-edge,edge,sin(q.x*24.0));
else if (GoboIndex<2.5 && GoboIndex>1.5) {
    float a=atan2(q.y,q.x);
    float star=0.45+0.25*cos(a*5.0);
    pattern=1.0-smoothstep(star-edge,star+edge,length(q));
}
else if (GoboIndex<3.5 && GoboIndex>2.5) {
    float2 dots=frac(q*3.0+0.5)-0.5;
    pattern=1.0-smoothstep(0.20-edge,0.20+edge,length(dots));
}
else if (GoboIndex>3.5) pattern=smoothstep(-edge,edge,sin(atan2(q.y,q.x)*4.0+length(q)*20.0));
return saturate(mask*lerp(pattern,1.0,Frost*0.75));
""")
assert edit.connect_material_expressions(uv, "", mask, "UV")
for row, (name, default) in enumerate(parameters.items()):
    param = edit.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 180+row*100)
    param.set_editor_property("parameter_name", name)
    param.set_editor_property("default_value", default)
    assert edit.connect_material_expressions(param, "", mask, name)
assert edit.connect_material_property(mask, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log("TSAV_NATIVE_OPTICS_ASSET_CREATED " + path)
