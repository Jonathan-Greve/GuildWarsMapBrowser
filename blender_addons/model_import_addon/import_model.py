import bpy
import json
import os
import numpy as np


def create_material_for_new_models(name, images, uv_map_names, texture_types):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()  # Clear default nodes (it comes with a ShaderNodeBsdfPrincipled and output as default, but we'll just create them ourselves)

    bsdf = nodes.new('ShaderNodeBsdfPrincipled')

    # Set all options to 0, except Roughness to 1
    for input in bsdf.inputs:
        if input.name in ["Roughness", "Alpha"]:
            input.default_value = 1.0
        else:
            # The default_value might be a single float or a tuple depending on the socket type
            if isinstance(input.default_value, float):
                input.default_value = 0.0
            elif isinstance(input.default_value, tuple):
                input.default_value = (0.0, 0.0, 0.0, 1.0)

    done = False  # We only want to set the first non-normal texture. This is because I havent figured out the pixel shaders yet for these "new" models
    for index, image in enumerate(images):
        texture_type = texture_types[index]

        texImage = nodes.new('ShaderNodeTexImage')
        uvMap = nodes.new('ShaderNodeUVMap')

        texImage.image = image
        uvMap.uv_map = uv_map_names[index]
        links.new(uvMap.outputs[0], texImage.inputs[0])
        if texture_type == 2:
            # Normal map
            links.new(texImage.outputs['Color'], bsdf.inputs['Normal'])
            continue

        if not done:
            links.new(texImage.outputs['Color'], bsdf.inputs['Base Color'])
            done = True

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Set material settings
    # Set Blend Mode to 'Alpha Clip'
    mat.blend_method = 'CLIP'

    # Set Shadow Mode to 'Alpha Clip'
    mat.shadow_method = 'CLIP'

    return mat


def create_material_for_old_models(name, images, uv_map_names, blend_flags, texture_types):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()  # Clear default nodes

    # Alpha accumulation variable
    accumulated_alpha = 0

    # Create RGB node
    rgb_node = nodes.new('ShaderNodeRGB')
    rgb_node.outputs[0].default_value = (1, 1, 1, 1)

    prev_texture_node = rgb_node

    const_node_1 = nodes.new('ShaderNodeValue')
    const_node_1.outputs[0].default_value = 1.0

    # Create initial alpha node and set its value to 2
    # This is only uses for blending not for setting the alpha of the result
    initial_alpha_node = nodes.new('ShaderNodeValue')
    initial_alpha_node.outputs[0].default_value = 2.0
    prev_alpha_node = initial_alpha_node

    # This will be used for setting the alpha of the result
    initial_alpha_val_node = nodes.new('ShaderNodeValue')
    initial_alpha_val_node.outputs[0].default_value = 0
    prev_alpha_val_node = initial_alpha_val_node

    prev_blend_flag = -1
    prev_texture_type = -1

    is_opaque = True

    for index, image in enumerate(images):
        blend_flag = blend_flags[index]
        texture_type = texture_types[index]

        if blend_flag in [1,2,3,4,5,8]:
            print('false')
            is_opaque = False

        # Create Image Texture node
        texImage = nodes.new('ShaderNodeTexImage')
        texImage.image = image
        texImage.label = "gwmb_texture"

        texImageNoAlpha = None

        # Create UV Map node
        uvMap = nodes.new('ShaderNodeUVMap')
        uvMap.uv_map = uv_map_names[index]

        links.new(uvMap.outputs[0], texImage.inputs[0])

        # Update alpha_val node
        modify_alpha_node = None
        if blend_flag in [3, 6, 7]:
            subNode = nodes.new('ShaderNodeMath')
            subNode.operation = 'SUBTRACT'
            links.new(const_node_1.outputs[0], subNode.inputs[0])
            links.new(texImage.outputs['Alpha'], subNode.inputs[1])
            modify_alpha_node = subNode.outputs[0]
        elif blend_flag == 0:
            # Need to create new texture for this to work in Blender
            texImageNoAlpha = nodes.new('ShaderNodeTexImage')
            texImageNoAlpha.image = image
            texImageNoAlpha.label = "gwmb_texture"
            texImageNoAlpha.image.alpha_mode = 'NONE'

            modify_alpha_node = const_node_1.outputs[0]
        else:
            modify_alpha_node = texImage.outputs['Alpha']

        # 1 - a
        subNode = nodes.new('ShaderNodeMath')
        subNode.operation = 'SUBTRACT'
        links.new(const_node_1.outputs[0], subNode.inputs[0])
        links.new(prev_alpha_val_node.outputs[0], subNode.inputs[1])

        # alpha * (1 - a)
        multiplyNode = nodes.new('ShaderNodeMath')
        multiplyNode.operation = 'MULTIPLY'
        links.new(modify_alpha_node, multiplyNode.inputs[0])
        links.new(subNode.outputs[0], multiplyNode.inputs[1])

        addNode = nodes.new('ShaderNodeMath')
        addNode.operation = 'ADD'
        links.new(multiplyNode.outputs[0], addNode.inputs[0])
        links.new(prev_alpha_val_node.outputs[0], addNode.inputs[1])

        prev_alpha_val_node = addNode

        if blend_flag in [3, 5]:
            if prev_texture_type == 1:
                # For Color
                multiplyNode = nodes.new('ShaderNodeVectorMath')
                multiplyNode.operation = 'MULTIPLY'
                links.new(texImage.outputs['Alpha'], multiplyNode.inputs[0])
                if 'Color' in prev_texture_node.outputs:
                    links.new(prev_texture_node.outputs['Color'], multiplyNode.inputs[1])
                elif 'Vector' in prev_texture_node.outputs:
                    links.new(prev_texture_node.outputs['Vector'], multiplyNode.inputs[1])
                elif 'Result' in prev_texture_node.outputs:
                    links.new(prev_texture_node.outputs['Result'], multiplyNode.inputs[1])

                addNode = nodes.new('ShaderNodeVectorMath')
                addNode.operation = 'ADD'
                links.new(multiplyNode.outputs[0], addNode.inputs[0])
                links.new(texImage.outputs['Color'], addNode.inputs[1])
                prev_texture_node = addNode  # clamp_output(addNode, nodes, links)

                # For Alpha
                alphaMultiplyNode = nodes.new('ShaderNodeMath')
                alphaMultiplyNode.operation = 'MULTIPLY'
                links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[0])
                links.new(prev_alpha_node.outputs[0], alphaMultiplyNode.inputs[1])

                alphaAddNode = nodes.new('ShaderNodeMath')
                alphaAddNode.operation = 'ADD'
                alphaAddNode.use_clamp = True
                links.new(alphaMultiplyNode.outputs[0], alphaAddNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaAddNode.inputs[1])
                prev_alpha_node = alphaAddNode

            else:
                # For Color
                multiplyNode = nodes.new('ShaderNodeVectorMath')
                multiplyNode.operation = 'MULTIPLY'
                links.new(prev_alpha_node.outputs[0], multiplyNode.inputs[0])
                links.new(texImage.outputs['Color'], multiplyNode.inputs[1])

                addNode = nodes.new('ShaderNodeVectorMath')
                addNode.operation = 'ADD'
                links.new(multiplyNode.outputs[0], addNode.inputs[0])
                if 'Color' in prev_texture_node.outputs:
                    links.new(prev_texture_node.outputs['Color'], addNode.inputs[1])
                elif 'Vector' in prev_texture_node.outputs:
                    links.new(prev_texture_node.outputs['Vector'], addNode.inputs[1])
                prev_texture_node = addNode

                # For Alpha
                alphaMultiplyNode = nodes.new('ShaderNodeMath')
                alphaMultiplyNode.operation = 'MULTIPLY'
                links.new(prev_alpha_node.outputs[0], alphaMultiplyNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[1])

                alphaAddNode = nodes.new('ShaderNodeMath')
                alphaAddNode.operation = 'ADD'
                alphaAddNode.use_clamp = True
                links.new(alphaMultiplyNode.outputs[0], alphaAddNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaAddNode.inputs[1])
                prev_alpha_node = alphaAddNode

        elif blend_flag == 4 and index > 0:
            # For Color
            mixRGBNode = nodes.new('ShaderNodeMixRGB')
            mixRGBNode.inputs['Fac'].default_value = 1.0
            mixRGBNode.use_clamp = True
            links.new(texImage.outputs['Alpha'], mixRGBNode.inputs[0])
            if 'Color' in prev_texture_node.outputs:
                links.new(prev_texture_node.outputs['Color'], mixRGBNode.inputs[1])
            elif 'Vector' in prev_texture_node.outputs:
                links.new(prev_texture_node.outputs['Vector'], mixRGBNode.inputs[1])
            elif 'Result' in prev_texture_node.outputs:
                links.new(prev_texture_node.outputs['Result'], mixRGBNode.inputs[1])

            links.new(texImage.outputs['Color'], mixRGBNode.inputs[2])
            prev_texture_node = mixRGBNode  # clamp_output(mixRGBNode, nodes, links)

            # For Alpha
            mixAlphaNode = nodes.new('ShaderNodeMixRGB')
            mixAlphaNode.inputs['Fac'].default_value = 1.0
            mixAlphaNode.use_clamp = True
            links.new(texImage.outputs['Alpha'], mixAlphaNode.inputs[0])
            links.new(prev_alpha_node.outputs[0], mixAlphaNode.inputs[1])
            links.new(texImage.outputs['Alpha'], mixAlphaNode.inputs[2])
            prev_alpha_node = mixAlphaNode
        else:
            if not ((blend_flag == 7 and prev_blend_flag == 8) or blend_flag == 6 or blend_flag == 0):
                # Multiply Color by 2
                multiplyByTwoNode = nodes.new('ShaderNodeVectorMath')
                multiplyByTwoNode.operation = 'MULTIPLY'
                multiplyByTwoNode.inputs[1].default_value = (2, 2, 2)
                links.new(prev_texture_node.outputs[0], multiplyByTwoNode.inputs[0])
                prev_texture_node = multiplyByTwoNode

                # Multiply Alpha by 2
                alphaMultiplyByTwoNode = nodes.new('ShaderNodeMath')
                alphaMultiplyByTwoNode.operation = 'MULTIPLY'
                alphaMultiplyByTwoNode.inputs[1].default_value = 2
                links.new(prev_alpha_node.outputs[0], alphaMultiplyByTwoNode.inputs[0])
                prev_alpha_node = alphaMultiplyByTwoNode

            # For Color
            mixRGBNode = nodes.new('ShaderNodeMixRGB')
            mixRGBNode.blend_type = 'MULTIPLY'
            mixRGBNode.inputs['Fac'].default_value = 1.0
            mixRGBNode.use_clamp = True
            links.new(prev_texture_node.outputs[0], mixRGBNode.inputs[1])
            if texImageNoAlpha is None:
                links.new(texImage.outputs['Color'], mixRGBNode.inputs[2])
            else:
                links.new(texImageNoAlpha.outputs['Color'], mixRGBNode.inputs[2])
            prev_texture_node = mixRGBNode

            # For Alpha
            alphaMultiplyNode = nodes.new('ShaderNodeMath')
            alphaMultiplyNode.operation = 'MULTIPLY'
            links.new(prev_alpha_node.outputs[0], alphaMultiplyNode.inputs[0])
            links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[1])
            prev_alpha_node = alphaMultiplyNode
            prev_alpha_node.use_clamp = True

        prev_blend_flag = blend_flag
        prev_texture_type = texture_type

    # Create the Principled BSDF and Output nodes
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')

    # Set all options to 0, except Roughness to 1
    for input in bsdf.inputs:
        try:
            # Check if the input is 'Roughness' and set it to 1.0
            if input.name == "Roughness":
                input.default_value = 1.0
            # Otherwise, set other inputs based on their type
            else:
                # For color inputs which expect a 4-tuple RGBA
                if input.type == 'RGBA':
                    input.default_value = (0.0, 0.0, 0.0, 1.0)  # Assuming you want full opacity
                # For vector inputs which expect a 3-tuple XYZ or RGB
                elif input.type == 'VECTOR':
                    input.default_value = (0.0, 0.0, 0.0)
                # For float inputs which expect a single value
                elif input.type == 'VALUE':
                    input.default_value = 0.0
                # Handle other input types as necessary
                else:
                    print(f"Unhandled input type {input.type} for {input.name}")
        except Exception as e:
            print(f"Error setting default value for {input.name}: {e}")

    if 'Color' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Color'], bsdf.inputs['Base Color'])
    elif 'Vector' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Vector'], bsdf.inputs['Base Color'])
    elif 'Result' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Result'], bsdf.inputs['Base Color'])

    links.new(prev_alpha_val_node.outputs[0], bsdf.inputs['Alpha'])

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Set material settings
    mat.blend_method = 'BLEND'
    if 8 in blend_flags or 4 in blend_flags:
        mat.show_transparent_back = True
    else:
        mat.show_transparent_back = False
        if is_opaque:
            mat.blend_method = 'OPAQUE'

    # Set Shadow Mode to 'Alpha Clip'
    mat.shadow_method = 'CLIP'

    return mat


def swap_axes(vec):
    # Swap X and Y axis
    return (vec['x'], vec['z'], vec['y'])


def scale_axes(vec, scale):
    # Swap X and Y axis
    return (vec[0] * scale, vec[1] * scale, vec[2] * scale)


def ensure_collection(context, collection_name, parent_collection=None):
    if collection_name not in bpy.data.collections:
        new_collection = bpy.data.collections.new(collection_name)
        if parent_collection:
            parent_collection.children.link(new_collection)
        else:
            context.scene.collection.children.link(new_collection)
    return bpy.data.collections[collection_name]


def create_mesh_from_json(context, directory, filename):
    filepath = os.path.join(directory, filename)
    # Derive base_name and model_hash without opening the file
    base_name = os.path.basename(filepath).split('.')[0]  # Something like "model_0x43A22_gwmb"
    model_hash = base_name.split('_')[1]  # E.g., "0x43A22"

    # Check if the collection for the model hash already exists
    if model_hash in bpy.data.collections:
        print(f"Collection for model hash: {model_hash} already exists. Nothing to do.")
        return {'FINISHED'}

    # If the collection does not exist, ensure base collection exists and proceed
    gwmb_collection = ensure_collection(context, "GWMB Models")

    # Since the collection does not exist, we now proceed to open the JSON file
    with open(filepath, 'r') as f:
        data = json.load(f)

    existing_images = set(bpy.data.images.keys())

    # Create all images from the json data
    all_texture_types = []
    all_images = []
    for tex in data.get('textures', []):
        image_name = "gwmb_texture_{}".format(tex['file_hash'])
        if image_name not in bpy.data.images:
            # Construct the file path using os.path.join for portability
            image_path = os.path.join(directory, "{}.png".format(tex['file_hash']))
            # Load the image and assign its name to match the 'image_name'
            image = bpy.data.images.load(image_path)
            image.name = image_name
            all_images.append(image)
        else:
            # The image already exists in bpy.data.images
            image = bpy.data.images[image_name]
            all_images.append(image)
        all_texture_types.append(tex['texture_type'])

    # Ensure a collection for the model hash exists under the GWMB_Models collection
    model_collection = ensure_collection(context, model_hash, parent_collection=gwmb_collection)

    # Set the model's hash collection as the active collection
    layer_collection = bpy.context.view_layer.layer_collection.children[gwmb_collection.name].children[
        model_collection.name]
    bpy.context.view_layer.active_layer_collection = layer_collection

    for idx, submodel in enumerate(data.get('submodels', [])):
        pixel_shader_type = submodel['pixel_shader_type']
        vertices_data = submodel.get('vertices', [])
        indices = submodel.get('indices', [])

        # Swap axis to match Blenders coordinate system.
        # Also scale the vertices to be inches rather than meters. 1 Inch = 0.0254m. (Guild Wars uses GW Inches)
        vertices = [swap_axes(v['pos']) for v in vertices_data]

        texture_blend_flags = submodel.get('texture_blend_flags', [])

        # Normals
        normals = [swap_axes(v['normal']) if v['has_normal'] else (0, 0, 1) for v in vertices_data]

        # Faces
        faces = [tuple(reversed(indices[i:i + 3])) for i in range(0, len(indices), 3)]

        mesh = bpy.data.meshes.new(name="{}_submodel_{}".format(model_hash, idx))
        mesh.from_pydata(vertices, [], faces)

        # Set Normals
        mesh.normals_split_custom_set_from_vertices(normals)

        # Create material for the submodel
        texture_indices = submodel.get('texture_indices', [])
        uv_map_indices = submodel.get('texture_uv_map_index', [])

        # Use only the textures specified in the texture_indices for this submodel
        submodel_images = [all_images[i] for i in texture_indices]
        # We also get the correct texture types for the selected textures above
        texture_types = [all_texture_types[i] for i in texture_indices]

        uv_map_names = []
        for uv_index, tex_index in enumerate(uv_map_indices):
            uv_layer_name = f"UV_{uv_index}"
            uv_map_names.append(uv_layer_name)
            uv_layer = mesh.uv_layers.new(name=uv_layer_name)
            uvs = []
            for vertex in vertices_data:
                # In DirectX 11 (DX11), used by the Guild Wars Map Browser, the UV coordinate system originates at the top left with (0,0), meaning the V coordinate increases downwards.
                # In Blender, however, the UV coordinate system originates at the bottom left with (0,0), so the V coordinate increases upwards.
                # Therefore, to correctly map DX11 UVs to Blender's UV system, we subtract the V value from 1, effectively flipping the texture on the vertical axis.
                uvs.append(
                    (vertex['texture_uv_coords'][tex_index]['x'], 1 - vertex['texture_uv_coords'][tex_index]['y']))

            for i, loop in enumerate(mesh.loops):
                uv_layer.data[i].uv = uvs[loop.vertex_index]

        material = None
        if pixel_shader_type == 6:
            # Old model
            material = create_material_for_old_models("Material_submodel_{}".format(idx), submodel_images, uv_map_names,
                                                      texture_blend_flags, texture_types)
        elif pixel_shader_type == 7:
            # New model
            material = create_material_for_new_models("Material_submodel_{}".format(idx), submodel_images, uv_map_names,
                                                      texture_types)
        else:
            raise "Unknown pixel_shader_type"

        mesh.update()

        obj_name = "{}_{}".format(model_hash, idx)
        obj = bpy.data.objects.new(obj_name, mesh)
        obj.data.materials.append(material)

        # Link the object to the model's collection directly
        model_collection.objects.link(obj)

        # Make sure the object is also in the scene collection for visibility
        context.view_layer.objects.active = obj
        obj.select_set(True)

    return {'FINISHED'}


class IMPORT_OT_JSONMesh(bpy.types.Operator):
    bl_idname = "import_mesh.json"
    bl_label = "Import JSON model file"
    bl_description = "Import a Guild Wars Map Browser model file (.json)"
    bl_options = {'REGISTER', 'UNDO'}

#    directory: bpy.props.StringProperty(
#        subtype='DIR_PATH',
#        default="",
#        description="Directory used for importing the GWMB model"
#    )

    # Use a CollectionProperty to store multiple file paths
    files: bpy.props.CollectionProperty(type=bpy.types.OperatorFileListElement)
    directory: bpy.props.StringProperty(subtype='DIR_PATH')

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        i = 0
        for file_elem in self.files:
            print(f'progress: {i}/{len(self.files)}')
            i = i + 1

            if file_elem.name.lower().startswith("model_"):
                result = create_mesh_from_json(context, self.directory, file_elem.name)
                if 'FINISHED' not in result:
                    self.report({'ERROR'}, f"Failed to import model from file: {filename}")
                    continue

        return {'FINISHED'}


def menu_func_import(self, context):
    self.layout.operator(IMPORT_OT_JSONMesh.bl_idname, text="JSON Mesh Import Operator")


def register():
    bpy.utils.register_class(IMPORT_OT_JSONMesh)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(IMPORT_OT_JSONMesh)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
    bpy.ops.import_mesh.json('INVOKE_DEFAULT')