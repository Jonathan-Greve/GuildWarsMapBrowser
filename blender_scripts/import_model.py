import bpy
import json
import os


def create_image_from_rgba(name, width, height, pixels):
    flattened_pixels = [channel for px in pixels for channel in (px['x'], px['y'], px['z'], px['w'])]
    image = bpy.data.images.new(name, width=width, height=height)
    image.pixels = flattened_pixels
    return image


def create_multi_texture_material(name, images, uv_map_names):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    bsdf = nodes["Principled BSDF"]
    last_color = bsdf.inputs['Base Color']

    for index, image in enumerate(images):
        texImage = nodes.new('ShaderNodeTexImage')
        mixRGB = nodes.new('ShaderNodeMixRGB')
        uvMap = nodes.new('ShaderNodeUVMap')  # UV Map node

        texImage.image = image
        uvMap.uv_map = uv_map_names[index]  # Set UV map name

        mixRGB.blend_type = 'MULTIPLY'

        links.new(uvMap.outputs[0], texImage.inputs[0])  # Connect UV map to texture
        links.new(mixRGB.inputs[1], texImage.outputs['Color'])
        links.new(mixRGB.inputs[2], last_color)
        last_color = mixRGB.outputs['Color']

    links.new(bsdf.inputs['Base Color'], last_color)
    links.new(nodes['Material Output'].inputs['Surface'], bsdf.outputs['BSDF'])

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

    for index, image in enumerate(images):
        print(blend_flags)
        blend_flag = blend_flags[index]

        # Create Image Texture node
        texImage = nodes.new('ShaderNodeTexImage')
        texImage.image = image
        texImage.label = "gwmb_texture"

        # Create UV Map node
        uvMap = nodes.new('ShaderNodeUVMap')
        uvMap.uv_map = uv_map_names[index]

        links.new(uvMap.outputs[0], texImage.inputs[0])

        # Accumulate alpha
        accumulated_alpha += texImage.outputs['Alpha'].default_value

        # Depending on the blend_flag, perform the blending
        if blend_flag == 0:
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

        elif blend_flag == 3:
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

        print(prev_texture_node)

    # Create the Principled BSDF and Output nodes
    print(prev_texture_node, prev_texture_node.outputs.keys())
    print(prev_texture_node, prev_texture_node.outputs.keys())

    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    if 'Color' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Color'], bsdf.inputs['Base Color'])
    elif 'Vector' in prev_texture_node.outputs:
        links.new(prev_texture_node.outputs['Vector'], bsdf.inputs['Base Color'])

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

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

    texture_types = [tex['texture_type'] for tex in data.get('textures', [])]

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
        else:
            material = create_multi_texture_material("Material_submodel_{}".format(idx), submodel_images, uv_map_names)

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
    bl_options = {'REGISTER', 'UNDO'}

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        return create_mesh_from_json(context, self.filepath)


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