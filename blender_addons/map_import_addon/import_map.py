import bpy
import json
import time
import os
import traceback
from mathutils import Vector, Matrix

# Blender 5.0 compatible version - V2 with new terrain format (4 UV layers)

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


def setup_eevee_settings():
    """Setup EEVEE settings compatible with Blender 4.2+ (EEVEE Next)"""
    version = get_blender_version()
    if version >= (4, 2, 0):
        try:
            bpy.context.scene.render.engine = 'BLENDER_EEVEE_NEXT'
        except:
            try:
                bpy.context.scene.render.engine = 'BLENDER_EEVEE'
            except:
                pass
    else:
        bpy.context.scene.render.engine = 'BLENDER_EEVEE'
        try:
            bpy.context.scene.eevee.use_ssr = True
            bpy.context.scene.eevee.use_ssr_refraction = True
            bpy.context.scene.eevee.ssr_quality = 1
            bpy.context.scene.eevee.ssr_thickness = 0.1
        except AttributeError:
            pass


def create_water_surface(water_level=0.0, terrain_min_x_max_y=(-50000, 50000), terrain_max_x_min_y=(50000, -50000),
                         map_collection=None):
    log('Creating water')
    min_x, max_y = terrain_min_x_max_y
    max_x, min_y = terrain_max_x_min_y

    water_vertices = [
        (min_x, min_y, water_level),
        (max_x, min_y, water_level),
        (max_x, max_y, water_level),
        (min_x, max_y, water_level)
    ]
    water_faces = [(0, 1, 2, 3)]

    water_mesh = bpy.data.meshes.new("WaterMesh")
    water_mesh.from_pydata(water_vertices, [], water_faces)
    water_mesh.update()

    water_obj = bpy.data.objects.new("WaterSurface", water_mesh)
    water_obj.location = (0, 0, water_level)

    setup_eevee_settings()

    water_mat = bpy.data.materials.new(name="WaterMaterial")
    water_mat.use_nodes = True
    nodes = water_mat.node_tree.nodes
    nodes.clear()

    bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    bsdf.inputs['Base Color'].default_value = (0.0, 0.344, 0.55, 1)
    bsdf.inputs['Roughness'].default_value = 0.05
    bsdf.inputs['IOR'].default_value = 1.333
    bsdf.inputs['Alpha'].default_value = 0.8

    version = get_blender_version()
    if version >= (4, 0, 0):
        try:
            bsdf.inputs['Transmission Weight'].default_value = 0.95
        except KeyError:
            try:
                bsdf.inputs['Transmission'].default_value = 0.95
            except KeyError:
                pass

    output_node = nodes.new('ShaderNodeOutputMaterial')
    links = water_mat.node_tree.links
    links.new(bsdf.outputs['BSDF'], output_node.inputs['Surface'])

    set_material_blend_method(water_mat, 'HASHED')
    set_material_shadow_method(water_mat, 'HASHED')

    water_obj.data.materials.append(water_mat)

    if map_collection:
        map_collection.objects.link(water_obj)
    else:
        bpy.context.collection.objects.link(water_obj)


def rotation_from_vectors(right_vec, forward_vec, up_vec):
    rot_mat = Matrix([right_vec, forward_vec, up_vec]).to_4x4()
    return rot_mat


def swap_axes(vec):
    return (vec['x'], vec['z'], vec['y'])


def swap_axes_normal(vec):
    """Swap axes for normal with fallback for None values"""
    if vec is None:
        return (0.0, 1.0, 0.0)  # Default up normal
    x = vec.get('x') if vec.get('x') is not None else 0.0
    y = vec.get('y') if vec.get('y') is not None else 1.0
    z = vec.get('z') if vec.get('z') is not None else 0.0
    return (x, z, y)  # Swap Y and Z for Blender


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


def create_map_from_json(context, directory, filename):
    """
    Import map with new terrain format (V2):
    - Vertices already have 4 UV coordinates pre-computed
    - Indices are pre-computed
    - No need to calculate UVs from texture_index
    """
    log("=== create_map_from_json START (V2 format) ===")
    disable_render_preview_background()
    set_clipping_values()
    set_metric_space()

    filepath = os.path.join(directory, filename)
    log(f'Loading map from: {filepath}')
    gwmb_collection = ensure_collection(context, "GWMB Maps")

    base_name = os.path.basename(filepath).split('.')[0]
    map_hash = base_name.split('_')[1]
    obj_name = "terrain_{}".format(map_hash)

    if map_hash in bpy.data.collections:
        log(f"Object {obj_name} already exists. Skipping.")
        return {'FINISHED'}

    with open(filepath, 'r') as f:
        data = json.load(f)

    if 'terrain' not in data:
        log(f"The key 'terrain' is not in the JSON data. Available keys: {list(data.keys())}")
        return {'FINISHED'}

    terrain_data = data['terrain']
    log(f"Terrain data loaded. Width: {terrain_data.get('width')}, Height: {terrain_data.get('height')}")

    # Load terrain atlas
    map_filehash = data['filehash']
    terrain_tex_atlas_name = "gwmb_terrain_texture_atlas_{}".format(map_filehash)
    if terrain_tex_atlas_name not in bpy.data.images:
        terrain_tex_atlas_image_path = os.path.join(directory, "atlas_{}.png".format(map_filehash))
        log(f"Loading terrain atlas: {terrain_tex_atlas_image_path}")
        terrain_tex_atlas = bpy.data.images.load(terrain_tex_atlas_image_path)
        # Use STRAIGHT alpha mode so transparent slot 0 works for blending
        terrain_tex_atlas.alpha_mode = 'STRAIGHT'
        terrain_tex_atlas.name = terrain_tex_atlas_name
        log(f"Atlas loaded with alpha_mode: STRAIGHT")
    else:
        terrain_tex_atlas = bpy.data.images[terrain_tex_atlas_name]

    map_collection = ensure_collection(context, map_hash, parent_collection=gwmb_collection)

    # Get terrain data - new format has pre-computed vertices and indices
    vertices_data = terrain_data.get('vertices', [])
    indices = terrain_data.get('indices', [])

    log(f"Reading {len(vertices_data)} vertices and {len(indices)} indices")

    # Check if this is the new format (has uv_coord0) or old format (has texture_index)
    is_new_format = len(vertices_data) > 0 and 'uv_coord0' in vertices_data[0]
    log(f"Detected format: {'NEW (pre-computed UVs)' if is_new_format else 'OLD (texture_index)'}")

    if is_new_format:
        # NEW FORMAT: Vertices already have 4 UV coords, indices are pre-computed
        # Convert vertices to Blender format (swap Y and Z)
        blender_vertices = [swap_axes(v['pos']) for v in vertices_data]
        blender_normals = [swap_axes_normal(v.get('normal')) for v in vertices_data]

        # Extract all 4 UV layers
        uv0_coords = [(v['uv_coord0']['x'], 1.0 - v['uv_coord0']['y']) for v in vertices_data]
        uv1_coords = [(v['uv_coord1']['x'], 1.0 - v['uv_coord1']['y']) for v in vertices_data]
        uv2_coords = [(v['uv_coord2']['x'], 1.0 - v['uv_coord2']['y']) for v in vertices_data]
        uv3_coords = [(v['uv_coord3']['x'], 1.0 - v['uv_coord3']['y']) for v in vertices_data]

        # Convert indices to faces (triangles)
        faces = []
        for i in range(0, len(indices), 3):
            faces.append((indices[i], indices[i+1], indices[i+2]))

        log(f"Creating mesh with {len(blender_vertices)} vertices and {len(faces)} faces")

        # Create mesh
        terrain_mesh_name = "{}_terrain".format(map_hash)
        mesh = bpy.data.meshes.new(name=terrain_mesh_name)
        mesh.from_pydata(blender_vertices, [], faces)

        # Set custom normals
        set_custom_normals(mesh, blender_normals)

        # Create 4 UV layers
        uv_layer_names = ["UV_Atlas", "UV_Blend1", "UV_Blend2", "UV_Shadow"]
        uv_data_lists = [uv0_coords, uv1_coords, uv2_coords, uv3_coords]

        for layer_idx, (layer_name, uv_data) in enumerate(zip(uv_layer_names, uv_data_lists)):
            uv_layer = mesh.uv_layers.new(name=layer_name)
            for i, loop in enumerate(mesh.loops):
                uv_layer.data[i].uv = uv_data[loop.vertex_index]
            log(f"Created UV layer: {layer_name}")

        mesh.update()

    else:
        # OLD FORMAT: Fall back to old logic with texture_index
        log("Using OLD format import (texture_index based)")
        texture_indices = [v['texture_index'] for v in vertices_data]
        terrain_width = terrain_data.get('width')
        terrain_height = terrain_data.get('height')
        atlas_grid_size = 8

        def calculate_uv_coordinates(texture_index, corner):
            grid_x = texture_index % atlas_grid_size
            grid_y = texture_index // atlas_grid_size
            offsets = [(0, 0), (1, 0), (1, 1), (0, 1)]
            u = (grid_x + offsets[corner][0]) / atlas_grid_size
            v = 1 - ((grid_y + offsets[corner][1]) / atlas_grid_size)
            return (u, v)

        new_vertices = []
        new_faces = []
        new_uvs = []

        for y in range(terrain_height - 1):
            for x in range(terrain_width - 1):
                top_left_vertex_index = y * terrain_width + x
                texture_index = texture_indices[top_left_vertex_index]

                tile_vertex_indices = []
                for dy in range(2):
                    for dx in range(2):
                        idx = (y + dy) * terrain_width + (x + dx)
                        new_vertices.append(swap_axes(vertices_data[idx]['pos']))
                        tile_vertex_indices.append(len(new_vertices) - 1)

                new_faces.append(
                    (tile_vertex_indices[0], tile_vertex_indices[1], tile_vertex_indices[3], tile_vertex_indices[2]))

                for corner in range(4):
                    uv = calculate_uv_coordinates(texture_index, corner)
                    new_uvs.append(uv)

        terrain_mesh_name = "{}_terrain".format(map_hash)
        mesh = bpy.data.meshes.new(name=terrain_mesh_name)
        mesh.from_pydata(new_vertices, [], new_faces)
        mesh.update()

        uv_layer = mesh.uv_layers.new(name="UV_Atlas")
        for i, loop in enumerate(mesh.loops):
            uv_layer.data[i].uv = new_uvs[i]

    # Create object
    obj = bpy.data.objects.new(obj_name, mesh)
    map_collection.objects.link(obj)

    # Create material with alpha blending for terrain layers
    material = bpy.data.materials.new(name="TerrainMaterial_{}".format(map_hash))
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    # Principled BSDF
    bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    bsdf.location = (600, 0)
    bsdf.inputs['Roughness'].default_value = 1.0

    # Output node
    output_node = nodes.new('ShaderNodeOutputMaterial')
    output_node.location = (800, 0)
    links.new(bsdf.outputs['BSDF'], output_node.inputs['Surface'])

    if is_new_format:
        # NEW FORMAT: 3-layer alpha blending like the HLSL shader
        # Use Closest interpolation to avoid bleeding from transparent slot 0

        # Layer 0 (base) - UV_Atlas
        uv0_node = nodes.new(type='ShaderNodeUVMap')
        uv0_node.uv_map = "UV_Atlas"
        uv0_node.location = (-1000, 300)

        tex0_node = nodes.new(type='ShaderNodeTexImage')
        tex0_node.image = terrain_tex_atlas
        tex0_node.interpolation = 'Linear'  # Linear matches C++ shader; UVs are inset by border
        tex0_node.location = (-800, 300)
        links.new(uv0_node.outputs['UV'], tex0_node.inputs['Vector'])

        # Layer 1 - UV_Blend1
        uv1_node = nodes.new(type='ShaderNodeUVMap')
        uv1_node.uv_map = "UV_Blend1"
        uv1_node.location = (-1000, 0)

        tex1_node = nodes.new(type='ShaderNodeTexImage')
        tex1_node.image = terrain_tex_atlas
        tex1_node.interpolation = 'Linear'
        tex1_node.location = (-800, 0)
        links.new(uv1_node.outputs['UV'], tex1_node.inputs['Vector'])

        # Layer 2 - UV_Blend2
        uv2_node = nodes.new(type='ShaderNodeUVMap')
        uv2_node.uv_map = "UV_Blend2"
        uv2_node.location = (-1000, -300)

        tex2_node = nodes.new(type='ShaderNodeTexImage')
        tex2_node.image = terrain_tex_atlas
        tex2_node.interpolation = 'Linear'
        tex2_node.location = (-800, -300)
        links.new(uv2_node.outputs['UV'], tex2_node.inputs['Vector'])

        # HLSL-style blending with fallback:
        # When totalAlpha > threshold: result = sum(color * alpha) / totalAlpha
        # When totalAlpha <= threshold: result = sum(color) / 3 (unweighted average)

        # Calculate weighted sum: color0*alpha0 + color1*alpha1 + color2*alpha2
        mult0 = nodes.new(type='ShaderNodeVectorMath')
        mult0.operation = 'SCALE'
        mult0.location = (-550, 250)
        links.new(tex0_node.outputs['Color'], mult0.inputs[0])
        links.new(tex0_node.outputs['Alpha'], mult0.inputs['Scale'])

        mult1 = nodes.new(type='ShaderNodeVectorMath')
        mult1.operation = 'SCALE'
        mult1.location = (-550, 0)
        links.new(tex1_node.outputs['Color'], mult1.inputs[0])
        links.new(tex1_node.outputs['Alpha'], mult1.inputs['Scale'])

        mult2 = nodes.new(type='ShaderNodeVectorMath')
        mult2.operation = 'SCALE'
        mult2.location = (-550, -250)
        links.new(tex2_node.outputs['Color'], mult2.inputs[0])
        links.new(tex2_node.outputs['Alpha'], mult2.inputs['Scale'])

        # Sum weighted colors
        add_w01 = nodes.new(type='ShaderNodeVectorMath')
        add_w01.operation = 'ADD'
        add_w01.location = (-350, 125)
        links.new(mult0.outputs['Vector'], add_w01.inputs[0])
        links.new(mult1.outputs['Vector'], add_w01.inputs[1])

        add_w012 = nodes.new(type='ShaderNodeVectorMath')
        add_w012.operation = 'ADD'
        add_w012.location = (-200, 50)
        links.new(add_w01.outputs['Vector'], add_w012.inputs[0])
        links.new(mult2.outputs['Vector'], add_w012.inputs[1])

        # Sum alphas
        add_a01 = nodes.new(type='ShaderNodeMath')
        add_a01.operation = 'ADD'
        add_a01.location = (-350, -100)
        links.new(tex0_node.outputs['Alpha'], add_a01.inputs[0])
        links.new(tex1_node.outputs['Alpha'], add_a01.inputs[1])

        add_a012 = nodes.new(type='ShaderNodeMath')
        add_a012.operation = 'ADD'
        add_a012.location = (-200, -150)
        links.new(add_a01.outputs[0], add_a012.inputs[0])
        links.new(tex2_node.outputs['Alpha'], add_a012.inputs[1])

        # Weighted result: weighted_sum / max(totalAlpha, 0.001)
        safe_alpha = nodes.new(type='ShaderNodeMath')
        safe_alpha.operation = 'MAXIMUM'
        safe_alpha.location = (-50, -150)
        safe_alpha.inputs[1].default_value = 0.001
        links.new(add_a012.outputs[0], safe_alpha.inputs[0])

        inv_alpha = nodes.new(type='ShaderNodeMath')
        inv_alpha.operation = 'DIVIDE'
        inv_alpha.location = (50, -150)
        inv_alpha.inputs[0].default_value = 1.0
        links.new(safe_alpha.outputs[0], inv_alpha.inputs[1])

        weighted_result = nodes.new(type='ShaderNodeVectorMath')
        weighted_result.operation = 'SCALE'
        weighted_result.location = (100, 50)
        links.new(add_w012.outputs['Vector'], weighted_result.inputs[0])
        links.new(inv_alpha.outputs[0], weighted_result.inputs['Scale'])

        # Fallback: Use layer 0's color directly (layer 0 always has valid texture, never neutral)
        # This ensures we never get black when all alphas are low
        fallback_result = tex0_node.outputs['Color']

        # Choose between weighted result and layer0 fallback based on totalAlpha
        # If totalAlpha > 0.5, use weighted; otherwise use layer 0's color
        alpha_threshold = nodes.new(type='ShaderNodeMath')
        alpha_threshold.operation = 'GREATER_THAN'
        alpha_threshold.location = (150, -200)
        alpha_threshold.inputs[1].default_value = 0.5
        links.new(add_a012.outputs[0], alpha_threshold.inputs[0])

        final_mix, final_fac, final_a, final_b, final_out = create_mix_rgb_node(nodes, 'MIX')
        final_mix.location = (300, 0)
        links.new(fallback_result, final_a)
        links.new(weighted_result.outputs['Vector'], final_b)
        links.new(alpha_threshold.outputs[0], final_fac)

        # Connect final blended color to BSDF
        links.new(final_out, bsdf.inputs['Base Color'])

        # Set alpha to 1 - terrain is always opaque
        bsdf.inputs['Alpha'].default_value = 1.0

        log("Created 3-layer terrain material with alpha threshold fallback")
    else:
        # OLD FORMAT: Single texture lookup
        uv_map_node = nodes.new(type='ShaderNodeUVMap')
        uv_map_node.uv_map = "UV_Atlas"
        uv_map_node.location = (-400, 0)

        texture_node = nodes.new(type='ShaderNodeTexImage')
        texture_node.image = terrain_tex_atlas
        texture_node.interpolation = 'Linear'
        texture_node.location = (-200, 0)

        links.new(uv_map_node.outputs['UV'], texture_node.inputs['Vector'])
        links.new(texture_node.outputs['Color'], bsdf.inputs['Base Color'])

    set_material_blend_method(material, 'OPAQUE')
    obj.data.materials.append(material)

    # Create water surface
    top_left_point = (data['min_x'], data['max_z'])
    bottom_right_point = (data['max_x'], data['min_z'])
    create_water_surface(0.0, top_left_point, bottom_right_point, map_collection)

    # Place models
    map_models_data = data.get('models', [])
    parent_collection_name = "GWMB Models"

    if parent_collection_name in bpy.data.collections:
        parent_collection = bpy.data.collections[parent_collection_name]

        for model_data in map_models_data:
            model_hash = "0x{:X}".format(model_data['model_hash'])

            model_collection = None
            for coll in parent_collection.children:
                if coll.name == model_hash:
                    model_collection = coll
                    break

            if model_collection:
                world_pos = model_data['world_pos']
                model_right = model_data['model_right']
                model_look = model_data['model_look']
                model_up = model_data['model_up']
                scale = model_data['scale']

                right_vector = Vector((model_right['x'], model_right['z'], model_right['y']))
                look_vector = Vector((model_look['x'], model_look['z'], model_look['y']))
                up_vector = Vector((model_up['x'], model_up['z'], model_up['y']))
                rotation_matrix = rotation_from_vectors(right_vector, look_vector, up_vector)

                for obj in model_collection.objects:
                    if obj.name not in map_collection.objects:
                        linked_duplicate = obj.copy()
                        linked_duplicate.data = obj.data.copy()
                        linked_duplicate.animation_data_clear()

                        location = Vector((world_pos['x'], world_pos['z'], world_pos['y']))
                        scale_matrix = Matrix.Scale(scale, 4)
                        translation_matrix = Matrix.Translation(location)
                        linked_duplicate.matrix_world = translation_matrix @ rotation_matrix.to_4x4() @ scale_matrix

                        map_collection.objects.link(linked_duplicate)
    else:
        log(f"The parent collection '{parent_collection_name}' does not exist in the scene.")

    log("=== create_map_from_json DONE ===")
    return {'FINISHED'}


def create_mesh_from_json(context, directory, filename):
    """Import a model from JSON file"""
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


class IMPORT_OT_GWMBMap(bpy.types.Operator):
    bl_idname = "import_mesh.gwmb_map_folder"
    bl_label = "Import GWMB Map Folder"
    bl_description = "Import all GWMB model and map files from a folder"
    bl_options = {'REGISTER', 'UNDO'}

    directory: bpy.props.StringProperty(
        subtype='DIR_PATH',
        default="",
        description="Directory used for importing the GWMB files"
    )

    filter_folder: bpy.props.BoolProperty(
        default=True,
        options={'HIDDEN'}
    )

    def invoke(self, context, event):
        log("=== INVOKE called ===")
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        log("=== EXECUTE called ===")
        log(f"Directory: '{self.directory}'")

        if not self.directory:
            self.report({'ERROR'}, "No directory selected")
            return {'CANCELLED'}

        if not os.path.isdir(self.directory):
            self.report({'ERROR'}, f"Not a valid directory: {self.directory}")
            return {'CANCELLED'}

        try:
            file_list = [f for f in os.listdir(self.directory) if f.lower().endswith('.json')]
            log(f"Found {len(file_list)} JSON files in directory")

            if len(file_list) == 0:
                self.report({'WARNING'}, "No JSON files found in directory")
                return {'CANCELLED'}

            map_filename = None
            for i, filename in enumerate(file_list):
                log(f'Progress: {i+1}/{len(file_list)} - {filename}')

                if filename.lower().startswith("model_"):
                    try:
                        create_mesh_from_json(context, self.directory, filename)
                    except Exception as e:
                        log(f"ERROR importing model {filename}: {e}")
                        log(traceback.format_exc())

                elif filename.lower().startswith("map_"):
                    map_filename = filename

            if map_filename:
                log(f"Processing map file: {map_filename}")
                try:
                    create_map_from_json(context, self.directory, map_filename)
                except Exception as e:
                    log(f"ERROR importing map {map_filename}: {e}")
                    log(traceback.format_exc())

            log("=== EXECUTE DONE ===")
            return {'FINISHED'}

        except Exception as e:
            log(f"FATAL ERROR: {e}")
            log(traceback.format_exc())
            self.report({'ERROR'}, f"Error: {e}")
            return {'CANCELLED'}


def menu_func_import(self, context):
    self.layout.operator("import_mesh.gwmb_map_folder", text="Import GWMB Map Folder")


def register():
    log("Registering GWMB Map Importer (V2)")
    bpy.utils.register_class(IMPORT_OT_GWMBMap)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    log("Unregistering GWMB Map Importer")
    bpy.utils.unregister_class(IMPORT_OT_GWMBMap)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
    bpy.ops.import_mesh.gwmb_map_folder('INVOKE_DEFAULT')
