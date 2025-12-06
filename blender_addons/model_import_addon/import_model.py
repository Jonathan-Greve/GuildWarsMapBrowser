import bpy
import json
import os
import traceback

# Blender 5.0 compatible version - matches map_import_addon exactly

def get_blender_version():
    return bpy.app.version


def log(msg):
    """Print to console with prefix for easy filtering"""
    print(f"[GWMB] {msg}")


def set_metric_space():
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.length_unit = 'METERS'
    bpy.context.scene.unit_settings.scale_length = 0.02


def set_clipping_values():
    """ Sets clipping start and end for all cameras and 3D viewports. """
    min_clip = 10
    max_clip = 300000
    for workspace in bpy.data.workspaces:
        for screen in workspace.screens:
            for area in screen.areas:
                if area.type == 'VIEW_3D':
                    for space in area.spaces:
                        if space.type == 'VIEW_3D':
                            space.clip_start = min_clip
                            space.clip_end = max_clip


def disable_render_preview_background():
    world = bpy.context.scene.world
    if world is not None:
        world.use_nodes = True
        nodes = world.node_tree.nodes
        if 'Background' in nodes:
            nodes['Background'].inputs['Color'].default_value = (0.6, 0.6, 0.6, 1)
            nodes['Background'].inputs['Strength'].default_value = 0.7


def create_mix_rgb_node(nodes, blend_type='MIX'):
    """
    Create a color mixing node compatible with Blender 3.4+ (ShaderNodeMix)
    and earlier versions (ShaderNodeMixRGB).
    """
    version = get_blender_version()

    if version >= (3, 4, 0):
        mix_node = nodes.new('ShaderNodeMix')
        mix_node.data_type = 'RGBA'
        mix_node.clamp_result = True

        blend_type_map = {
            'MIX': 'MIX', 'MULTIPLY': 'MULTIPLY', 'ADD': 'ADD',
            'SUBTRACT': 'SUBTRACT', 'SCREEN': 'SCREEN', 'DIVIDE': 'DIVIDE',
            'DIFFERENCE': 'DIFFERENCE', 'DARKEN': 'DARKEN', 'LIGHTEN': 'LIGHTEN',
            'OVERLAY': 'OVERLAY', 'DODGE': 'DODGE', 'BURN': 'BURN',
            'HUE': 'HUE', 'SATURATION': 'SATURATION', 'VALUE': 'VALUE',
            'COLOR': 'COLOR', 'SOFT_LIGHT': 'SOFT_LIGHT', 'LINEAR_LIGHT': 'LINEAR_LIGHT',
        }
        mix_node.blend_type = blend_type_map.get(blend_type, 'MIX')
        return (mix_node, mix_node.inputs[0], mix_node.inputs[6], mix_node.inputs[7], mix_node.outputs[2])
    else:
        mix_node = nodes.new('ShaderNodeMixRGB')
        mix_node.blend_type = blend_type
        mix_node.use_clamp = True
        return (mix_node, mix_node.inputs['Fac'], mix_node.inputs[1], mix_node.inputs[2], mix_node.outputs['Color'])


def set_material_blend_method(mat, method):
    """Set material blend method compatible with Blender 4.2+"""
    version = get_blender_version()
    if version >= (4, 2, 0):
        method_map = {'OPAQUE': 'DITHERED', 'BLEND': 'BLENDED', 'HASHED': 'DITHERED', 'CLIP': 'DITHERED'}
        mat.surface_render_method = method_map.get(method, 'DITHERED')
    else:
        mat.blend_method = method


def set_material_shadow_method(mat, method):
    """Set material shadow method compatible with different Blender versions"""
    version = get_blender_version()
    if version >= (4, 2, 0):
        try:
            mat.shadow_method = method
        except AttributeError:
            pass
    else:
        mat.shadow_method = method


def set_material_transparent_back(mat, value):
    """Set transparent back setting compatible with Blender 4.2+"""
    version = get_blender_version()
    if version >= (4, 2, 0):
        try:
            mat.use_transparency_overlap = value
        except AttributeError:
            pass
    else:
        try:
            mat.show_transparent_back = value
        except AttributeError:
            pass


def create_material_for_old_models(name, images, uv_map_names, blend_flags, texture_types):
    """
    Complex material creation for old models (pixel_shader_type == 4, 6)
    Ported from Blender 3/4 addon with Blender 5.0 compatibility
    Tracks output sockets directly instead of nodes for proper ShaderNodeMix handling
    """
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    # Lighting stand-in
    lighting_node = nodes.new('ShaderNodeCombineXYZ')
    lighting_node.inputs['X'].default_value = 2.0
    lighting_node.inputs['Y'].default_value = 2.0
    lighting_node.inputs['Z'].default_value = 2.0

    # Track output sockets directly, not nodes
    prev_color_output = lighting_node.outputs['Vector']

    const_node_1 = nodes.new('ShaderNodeValue')
    const_node_1.outputs[0].default_value = 1.0

    # Initial alpha for blending
    initial_alpha_node = nodes.new('ShaderNodeValue')
    initial_alpha_node.outputs[0].default_value = 2.0
    prev_alpha_output = initial_alpha_node.outputs[0]

    # Alpha for result
    initial_alpha_val_node = nodes.new('ShaderNodeValue')
    initial_alpha_val_node.outputs[0].default_value = 0
    prev_alpha_val_output = initial_alpha_val_node.outputs[0]

    prev_blend_flag = -1
    prev_texture_type = -1
    is_opaque = True

    for index, image in enumerate(images):
        blend_flag = blend_flags[index] if index < len(blend_flags) else 0
        texture_type = texture_types[index] if index < len(texture_types) else 0

        if blend_flag in [1, 2, 3, 4, 5, 8]:
            is_opaque = False

        texImage = nodes.new('ShaderNodeTexImage')
        texImage.image = image
        texImage.label = "gwmb_texture"

        texImageNoAlpha = None

        uvMap = nodes.new('ShaderNodeUVMap')
        uvMap.uv_map = uv_map_names[index] if index < len(uv_map_names) else "UV_0"
        links.new(uvMap.outputs[0], texImage.inputs[0])

        # Update alpha_val node
        modify_alpha_output = None
        if blend_flag in [3, 6, 7]:
            subNode = nodes.new('ShaderNodeMath')
            subNode.operation = 'SUBTRACT'
            links.new(const_node_1.outputs[0], subNode.inputs[0])
            links.new(texImage.outputs['Alpha'], subNode.inputs[1])
            modify_alpha_output = subNode.outputs[0]
        elif blend_flag == 0:
            texImageNoAlpha = nodes.new('ShaderNodeTexImage')
            texImageNoAlpha.image = image.copy()
            texImageNoAlpha.label = "gwmb_texture"
            texImageNoAlpha.image.alpha_mode = 'NONE'
            links.new(uvMap.outputs[0], texImageNoAlpha.inputs[0])
            modify_alpha_output = const_node_1.outputs[0]
        else:
            modify_alpha_output = texImage.outputs['Alpha']

        # 1 - a
        subNode = nodes.new('ShaderNodeMath')
        subNode.operation = 'SUBTRACT'
        links.new(const_node_1.outputs[0], subNode.inputs[0])
        links.new(prev_alpha_val_output, subNode.inputs[1])

        # alpha * (1 - a)
        multiplyNode = nodes.new('ShaderNodeMath')
        multiplyNode.operation = 'MULTIPLY'
        links.new(modify_alpha_output, multiplyNode.inputs[0])
        links.new(subNode.outputs[0], multiplyNode.inputs[1])

        addNode = nodes.new('ShaderNodeMath')
        addNode.operation = 'ADD'
        links.new(multiplyNode.outputs[0], addNode.inputs[0])
        links.new(prev_alpha_val_output, addNode.inputs[1])
        prev_alpha_val_output = addNode.outputs[0]

        if blend_flag in [3, 5]:
            if prev_texture_type == 1:
                multiplyNode = nodes.new('ShaderNodeVectorMath')
                multiplyNode.operation = 'MULTIPLY'
                links.new(texImage.outputs['Alpha'], multiplyNode.inputs[0])
                links.new(prev_color_output, multiplyNode.inputs[1])

                addNode = nodes.new('ShaderNodeVectorMath')
                addNode.operation = 'ADD'
                links.new(multiplyNode.outputs['Vector'], addNode.inputs[0])
                links.new(texImage.outputs['Color'], addNode.inputs[1])
                prev_color_output = addNode.outputs['Vector']

                alphaMultiplyNode = nodes.new('ShaderNodeMath')
                alphaMultiplyNode.operation = 'MULTIPLY'
                links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[0])
                links.new(prev_alpha_output, alphaMultiplyNode.inputs[1])

                alphaAddNode = nodes.new('ShaderNodeMath')
                alphaAddNode.operation = 'ADD'
                alphaAddNode.use_clamp = True
                links.new(alphaMultiplyNode.outputs[0], alphaAddNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaAddNode.inputs[1])
                prev_alpha_output = alphaAddNode.outputs[0]
            else:
                multiplyNode = nodes.new('ShaderNodeVectorMath')
                multiplyNode.operation = 'MULTIPLY'
                links.new(prev_alpha_output, multiplyNode.inputs[0])
                links.new(texImage.outputs['Color'], multiplyNode.inputs[1])

                addNode = nodes.new('ShaderNodeVectorMath')
                addNode.operation = 'ADD'
                links.new(multiplyNode.outputs['Vector'], addNode.inputs[0])
                links.new(prev_color_output, addNode.inputs[1])
                prev_color_output = addNode.outputs['Vector']

                alphaMultiplyNode = nodes.new('ShaderNodeMath')
                alphaMultiplyNode.operation = 'MULTIPLY'
                links.new(prev_alpha_output, alphaMultiplyNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[1])

                alphaAddNode = nodes.new('ShaderNodeMath')
                alphaAddNode.operation = 'ADD'
                alphaAddNode.use_clamp = True
                links.new(alphaMultiplyNode.outputs[0], alphaAddNode.inputs[0])
                links.new(texImage.outputs['Alpha'], alphaAddNode.inputs[1])
                prev_alpha_output = alphaAddNode.outputs[0]

        elif blend_flag == 4 and index > 0:
            # Use ShaderNodeMix for Blender 5.0
            mix_node, mix_fac, mix_a, mix_b, mix_out = create_mix_rgb_node(nodes, 'MIX')
            links.new(texImage.outputs['Alpha'], mix_fac)
            links.new(prev_color_output, mix_a)
            links.new(texImage.outputs['Color'], mix_b)
            prev_color_output = mix_out

            mix_alpha_node, mix_alpha_fac, mix_alpha_a, mix_alpha_b, mix_alpha_out = create_mix_rgb_node(nodes, 'MIX')
            links.new(texImage.outputs['Alpha'], mix_alpha_fac)
            links.new(prev_alpha_output, mix_alpha_a)
            links.new(texImage.outputs['Alpha'], mix_alpha_b)
            prev_alpha_output = mix_alpha_out

        else:
            if not ((blend_flag == 7 and prev_blend_flag == 8) or blend_flag == 6 or blend_flag == 0):
                multiplyByTwoNode = nodes.new('ShaderNodeVectorMath')
                multiplyByTwoNode.operation = 'MULTIPLY'
                multiplyByTwoNode.inputs[1].default_value = (2, 2, 2)
                links.new(prev_color_output, multiplyByTwoNode.inputs[0])
                prev_color_output = multiplyByTwoNode.outputs['Vector']

                alphaMultiplyByTwoNode = nodes.new('ShaderNodeMath')
                alphaMultiplyByTwoNode.operation = 'MULTIPLY'
                alphaMultiplyByTwoNode.inputs[1].default_value = 2
                links.new(prev_alpha_output, alphaMultiplyByTwoNode.inputs[0])
                prev_alpha_output = alphaMultiplyByTwoNode.outputs[0]

            mix_node, mix_fac, mix_a, mix_b, mix_out = create_mix_rgb_node(nodes, 'MULTIPLY')
            mix_fac.default_value = 1.0
            links.new(prev_color_output, mix_a)
            if texImageNoAlpha is None:
                links.new(texImage.outputs['Color'], mix_b)
            else:
                links.new(texImageNoAlpha.outputs['Color'], mix_b)
            prev_color_output = mix_out

            alphaMultiplyNode = nodes.new('ShaderNodeMath')
            alphaMultiplyNode.operation = 'MULTIPLY'
            alphaMultiplyNode.use_clamp = True
            links.new(prev_alpha_output, alphaMultiplyNode.inputs[0])
            links.new(texImage.outputs['Alpha'], alphaMultiplyNode.inputs[1])
            prev_alpha_output = alphaMultiplyNode.outputs[0]

        prev_blend_flag = blend_flag
        prev_texture_type = texture_type

    # Create BSDF and Output
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value = 1.0

    links.new(prev_color_output, bsdf.inputs['Base Color'])
    links.new(prev_alpha_val_output, bsdf.inputs['Alpha'])

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Material settings
    set_material_blend_method(mat, 'BLEND')
    if 8 in blend_flags or 4 in blend_flags:
        set_material_transparent_back(mat, True)
    else:
        set_material_transparent_back(mat, False)
        if is_opaque:
            set_material_blend_method(mat, 'OPAQUE')

    set_material_shadow_method(mat, 'CLIP')
    return mat


def create_material_for_new_models(name, images, uv_map_names, blend_flags, texture_types):
    """
    Simple material creation for new models (pixel_shader_type == 7)
    Only uses first non-normal texture
    """
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value = 1.0

    done = False
    for index, image in enumerate(images):
        texture_type = texture_types[index] if index < len(texture_types) else 0
        blend_flag = blend_flags[index] if index < len(blend_flags) else 0

        texImage = nodes.new('ShaderNodeTexImage')
        uvMap = nodes.new('ShaderNodeUVMap')

        texImage.image = image
        uvMap.uv_map = uv_map_names[index] if index < len(uv_map_names) else "UV_0"
        links.new(uvMap.outputs[0], texImage.inputs[0])

        if texture_type == 2:
            # Normal map
            links.new(texImage.outputs['Color'], bsdf.inputs['Normal'])
            continue

        if not done:
            links.new(texImage.outputs['Color'], bsdf.inputs['Base Color'])
            if blend_flag != 8:
                texImage.image.alpha_mode = 'NONE'
            links.new(texImage.outputs['Alpha'], bsdf.inputs['Alpha'])
            done = True

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    if 8 in blend_flags:
        set_material_blend_method(mat, 'BLEND')
        set_material_shadow_method(mat, 'CLIP')
    else:
        set_material_blend_method(mat, 'OPAQUE')
        set_material_shadow_method(mat, 'OPAQUE')

    return mat


def swap_axes(vec):
    return (vec['x'], vec['z'], vec['y'])


def ensure_collection(context, collection_name, parent_collection=None):
    if collection_name not in bpy.data.collections:
        new_collection = bpy.data.collections.new(collection_name)
        if parent_collection:
            parent_collection.children.link(new_collection)
        else:
            context.scene.collection.children.link(new_collection)
    return bpy.data.collections[collection_name]


def set_custom_normals(mesh, normals):
    """Set custom normals compatible with Blender 4.1+"""
    mesh.normals_split_custom_set_from_vertices(normals)


def create_mesh_from_json(context, directory, filename):
    """Import a model from JSON file - identical to map importer version"""
    log(f"Loading model: {filename}")
    filepath = os.path.join(directory, filename)
    base_name = os.path.basename(filepath).split('.')[0]
    model_hash = base_name.split('_')[1]

    if model_hash in bpy.data.collections:
        log(f"Collection for model hash: {model_hash} already exists. Nothing to do.")
        return {'FINISHED'}

    gwmb_collection = ensure_collection(context, "GWMB Models")

    with open(filepath, 'r') as f:
        data = json.load(f)

    all_texture_types = []
    all_images = []
    for tex in data.get('textures', []):
        image_name = "gwmb_texture_{}".format(tex['file_hash'])
        if image_name not in bpy.data.images:
            image_path = os.path.join(directory, "{}.png".format(tex['file_hash']))
            image = bpy.data.images.load(image_path)
            image.name = image_name
            all_images.append(image)
        else:
            image = bpy.data.images[image_name]
            all_images.append(image)
        all_texture_types.append(tex['texture_type'])

    model_collection = ensure_collection(context, model_hash, parent_collection=gwmb_collection)

    layer_collection = bpy.context.view_layer.layer_collection.children[gwmb_collection.name].children[
        model_collection.name]
    bpy.context.view_layer.active_layer_collection = layer_collection

    for idx, submodel in enumerate(data.get('submodels', [])):
        pixel_shader_type = submodel['pixel_shader_type']
        vertices_data = submodel.get('vertices', [])
        indices = submodel.get('indices', [])

        vertices = [swap_axes(v['pos']) for v in vertices_data]
        texture_blend_flags = submodel.get('texture_blend_flags', [])
        normals = [swap_axes(v['normal']) if v.get('has_normal', False) else (0, 0, 1) for v in vertices_data]
        faces = [tuple(reversed(indices[i:i + 3])) for i in range(0, len(indices), 3)]

        mesh = bpy.data.meshes.new(name="{}_submodel_{}".format(model_hash, idx))
        mesh.from_pydata(vertices, [], faces)
        set_custom_normals(mesh, normals)

        texture_indices = submodel.get('texture_indices', [])
        uv_map_indices = submodel.get('texture_uv_map_index', [])

        submodel_images = [all_images[i] for i in texture_indices]
        texture_types = [all_texture_types[i] for i in texture_indices]

        uv_map_names = []
        for uv_index, tex_index in enumerate(uv_map_indices):
            uv_layer_name = f"UV_{uv_index}"
            uv_map_names.append(uv_layer_name)
            uv_layer = mesh.uv_layers.new(name=uv_layer_name)
            uvs = []
            for vertex in vertices_data:
                uvs.append(
                    (vertex['texture_uv_coords'][tex_index]['x'], 1 - vertex['texture_uv_coords'][tex_index]['y']))

            for i, loop in enumerate(mesh.loops):
                uv_layer.data[i].uv = uvs[loop.vertex_index]

        # Create material based on pixel_shader_type
        # Type 4, 6: Old model shaders - complex multi-texture blending
        # Type 7: New model shader - simple first texture only
        material = None
        if pixel_shader_type in [4, 6]:
            # Old model - complex blending
            material = create_material_for_old_models(
                "Material_submodel_{}".format(idx),
                submodel_images,
                uv_map_names,
                texture_blend_flags,
                texture_types
            )
        elif pixel_shader_type == 7:
            # New model - simple first texture
            material = create_material_for_new_models(
                "Material_submodel_{}".format(idx),
                submodel_images,
                uv_map_names,
                texture_blend_flags,
                texture_types
            )
        else:
            # Fallback - use old model shader for unknown types
            log(f"Unknown pixel_shader_type {pixel_shader_type}, using old model shader")
            material = create_material_for_old_models(
                "Material_submodel_{}".format(idx),
                submodel_images,
                uv_map_names,
                texture_blend_flags,
                texture_types
            )

        mesh.update()

        obj_name = "{}_{}".format(model_hash, idx)
        obj = bpy.data.objects.new(obj_name, mesh)
        obj.data.materials.append(material)
        model_collection.objects.link(obj)

    return {'FINISHED'}


class IMPORT_OT_JSONMesh(bpy.types.Operator):
    bl_idname = "import_mesh.json"
    bl_label = "Import JSON model file"
    bl_description = "Import a Guild Wars Map Browser model file (.json)"
    bl_options = {'REGISTER', 'UNDO'}

    files: bpy.props.CollectionProperty(type=bpy.types.OperatorFileListElement)
    directory: bpy.props.StringProperty(subtype='DIR_PATH')

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        disable_render_preview_background()
        set_clipping_values()
        set_metric_space()

        for i, file_elem in enumerate(self.files):
            log(f'Progress: {i+1}/{len(self.files)} - {file_elem.name}')

            if file_elem.name.lower().startswith("model_"):
                try:
                    result = create_mesh_from_json(context, self.directory, file_elem.name)
                    if 'FINISHED' not in result:
                        self.report({'ERROR'}, f"Failed to import model from file: {file_elem.name}")
                except Exception as e:
                    log(f"ERROR importing model {file_elem.name}: {e}")
                    log(traceback.format_exc())
                    self.report({'ERROR'}, f"Error importing {file_elem.name}: {e}")

        return {'FINISHED'}


def menu_func_import(self, context):
    self.layout.operator(IMPORT_OT_JSONMesh.bl_idname, text="GW Map Browser Model File (.json)")


def register():
    log("Registering GWMB Model Importer")
    bpy.utils.register_class(IMPORT_OT_JSONMesh)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    log("Unregistering GWMB Model Importer")
    bpy.utils.unregister_class(IMPORT_OT_JSONMesh)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
    bpy.ops.import_mesh.json('INVOKE_DEFAULT')
