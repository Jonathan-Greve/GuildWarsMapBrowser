import bpy
import json
import os


def clamp_output(node, nodes, links):
    clampNode = nodes.new('ShaderNodeClamp')
    clampNode.inputs[1].default_value = 0
    clampNode.inputs[2].default_value = 1
    links.new(node.outputs[0], clampNode.inputs[0])
    return clampNode


def create_image_from_rgba(name, width, height, pixels):
    flattened_pixels = [channel for px in pixels for channel in (px['x'], px['y'], px['z'], px['w'])]
    image = bpy.data.images.new(name, width=width, height=height)
    image.pixels = flattened_pixels
    return image


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

    # Create initial alpha node and set its value to 1
    initial_alpha_node = nodes.new('ShaderNodeValue')
    initial_alpha_node.outputs[0].default_value = 1.0
    prev_alpha_node = initial_alpha_node

    prev_blend_flag = -1
    prev_texture_type = -1

    for index, image in enumerate(images):
        blend_flag = blend_flags[index]
        texture_type = texture_types[index]

        # Create Image Texture node
        texImage = nodes.new('ShaderNodeTexImage')
        texImage.image = image
        texImage.label = "gwmb_texture"

        # Create UV Map node
        uvMap = nodes.new('ShaderNodeUVMap')
        uvMap.uv_map = uv_map_names[index]

        links.new(uvMap.outputs[0], texImage.inputs[0])

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
                links.new(alphaMultiplyNode.outputs[0], alphaAddNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaAddNode.inputs[1])
                prev_alpha_node = clamp_output(alphaAddNode, nodes, links)

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
                prev_texture_node = addNode  # clamp_output(addNode, nodes, links)

                # For Alpha
                alphaMultiplyNode = nodes.new('ShaderNodeMath')
                alphaMultiplyNode.operation = 'MULTIPLY'
                links.new(prev_alpha_node.outputs[0], alphaMultiplyNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[1])

                alphaAddNode = nodes.new('ShaderNodeMath')
                alphaAddNode.operation = 'ADD'
                links.new(alphaMultiplyNode.outputs[0], alphaAddNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaAddNode.inputs[1])
                prev_alpha_node = clamp_output(alphaAddNode, nodes, links)

        elif blend_flag == 4 and index > 0:
            # For Color
            mixRGBNode = nodes.new('ShaderNodeMixRGB')
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
            links.new(texImage.outputs['Alpha'], mixAlphaNode.inputs[0])
            links.new(prev_alpha_node.outputs[0], mixAlphaNode.inputs[1])
            links.new(texImage.outputs['Alpha'], mixAlphaNode.inputs[2])
            prev_alpha_node = clamp_output(mixAlphaNode, nodes, links)
        else:
            # For Color
            multiplyNode = nodes.new('ShaderNodeVectorMath')
            multiplyNode.operation = 'MULTIPLY'
            links.new(prev_texture_node.outputs[0], multiplyNode.inputs[0])
            links.new(texImage.outputs['Color'], multiplyNode.inputs[1])
            prev_texture_node = multiplyNode

            # For Alpha
            alphaMultiplyNode = nodes.new('ShaderNodeMath')
            alphaMultiplyNode.operation = 'MULTIPLY'
            links.new(prev_alpha_node.outputs[0], alphaMultiplyNode.inputs[0])
            links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[1])
            prev_alpha_node = alphaMultiplyNode

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

            # Clamp the Alpha output
            prev_alpha_node = clamp_output(prev_alpha_node, nodes, links)

        prev_blend_flag = blend_flag
        prev_texture_type = texture_type

    # Create the Principled BSDF and Output nodes
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')

    # Set all options to 0, except Roughness to 1
    for input in bsdf.inputs:
        if input.name == "Roughness":
            input.default_value = 1.0
        else:
            try:
                # Assuming all other inputs can safely be set to a single float value or an RGBA tuple.
                input.default_value = 0.0 if isinstance(input.default_value, float) else (0.0, 0.0, 0.0, 0.0)
            except Exception as e:
                print(f"Error setting default value for {input.name}: {e}")


    if 'Color' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Color'], bsdf.inputs['Base Color'])
    elif 'Vector' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Vector'], bsdf.inputs['Base Color'])
    elif 'Result' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Result'], bsdf.inputs['Base Color'])

    links.new(prev_alpha_node.outputs[0], bsdf.inputs['Alpha'])

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Set material settings
    # Set Blend Mode to 'Alpha Clip'
    mat.blend_method = 'CLIP'

    # Set Shadow Mode to 'Alpha Clip'
    mat.shadow_method = 'CLIP'

    return mat


def swap_axes(vec):
    # Swap X and Y axis
    return (vec['x'], vec['z'], vec['y'])


def create_mesh_from_json(context, filepath):
    with open(filepath, 'r') as f:
        data = json.load(f)

    base_name = os.path.basename(filepath).split('.')[0]  # Something like "model_0x43A22_gwmb"
    model_hash = base_name.split('_')[1]  # E.g., "0x43A22"

    # Create all images from the json data
    all_images = [create_image_from_rgba("gwmb_texture_{}".format(tex['file_hash']), tex['width'], tex['height'],
                                         tex['rgba_pixels']) for tex in data.get('textures', [])]

    all_texture_types = [tex['texture_type'] for tex in data.get('textures', [])]

    for idx, submodel in enumerate(data.get('submodels', [])):
        pixel_shader_type = submodel['pixel_shader_type']
        vertices_data = submodel.get('vertices', [])
        indices = submodel.get('indices', [])
        vertices = [swap_axes(v['pos']) for v in vertices_data]

        texture_blend_flags = submodel.get('texture_blend_flags', [])

        # Normals
        normals = [swap_axes(v['normal']) if v['has_normal'] else (0, 0, 1) for v in vertices_data]

        # Faces
        faces = [tuple(indices[i:i + 3]) for i in range(0, len(indices), 3)]

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
            uv_layer_name = "UV_{}".format(uv_index)
            uv_map_names.append(uv_layer_name)
            uv_layer = mesh.uv_layers.new(name=uv_layer_name)
            uvs = []
            for vertex in vertices_data:
                uvs.append(vertex['texture_uv_coords'][tex_index])
            for i, loop in enumerate(mesh.loops):
                uv_layer.data[i].uv = (uvs[loop.vertex_index]['x'], uvs[loop.vertex_index]['y'])

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

        obj = bpy.data.objects.new("{}_{}".format(model_hash, idx), mesh)
        obj.data.materials.append(material)

        context.collection.objects.link(obj)
        context.view_layer.objects.active = obj
        obj.select_set(True)

    return {'FINISHED'}


class IMPORT_OT_JSONMesh(bpy.types.Operator):
    bl_idname = "import_mesh.json"
    bl_label = "Import JSON model file"
    bl_description = "Import a Guild Wars Map Browser model file (.json)"
    bl_options = {'REGISTER', 'UNDO'}

    # Use a CollectionProperty to store multiple file paths
    files: bpy.props.CollectionProperty(type=bpy.types.OperatorFileListElement)
    directory: bpy.props.StringProperty(subtype='DIR_PATH')

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        # Iterate over the collection of file paths
        for file_elem in self.files:
            filepath = os.path.join(self.directory, file_elem.name)
            result = create_mesh_from_json(context, filepath)
            if 'FINISHED' not in result:
                break  # If something went wrong, exit the loop

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