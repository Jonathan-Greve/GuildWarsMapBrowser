import bpy
import json
import time
import os
import numpy as np
from mathutils import Vector, Matrix

import bpy

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
        # Set the background color to gray or any desired color
        nodes['Background'].inputs['Color'].default_value = (0.6, 0.6, 0.6, 1)
        nodes['Background'].inputs['Strength'].default_value = 0.7


def create_water_surface(water_level=0.0, terrain_min_x_max_y=(-50000, 50000), terrain_max_x_min_y=(50000, -50000),
                         map_collection=None):
    print('Creating water')

    min_x, max_y = terrain_min_x_max_y
    max_x, min_y = terrain_max_x_min_y

    # Define the vertices for the water plane
    water_vertices = [
        (min_x, min_y, water_level),  # Bottom left
        (max_x, min_y, water_level),  # Bottom right
        (max_x, max_y, water_level),  # Top right
        (min_x, max_y, water_level)  # Top left
    ]
    water_faces = [(0, 1, 2, 3)]

    # Create a new mesh for the water surface
    water_mesh = bpy.data.meshes.new("WaterMesh")
    water_mesh.from_pydata(water_vertices, [], water_faces)
    water_mesh.update()

    # Create a new object for the water surface
    water_obj = bpy.data.objects.new("WaterSurface", water_mesh)
    water_obj.location = (0, 0, water_level)  # Adjust as needed

    # Enable Eevee settings for reflection
    bpy.context.scene.render.engine = 'BLENDER_EEVEE'
    bpy.context.scene.eevee.use_ssr = True
    bpy.context.scene.eevee.use_ssr_refraction = True
    bpy.context.scene.eevee.ssr_quality = 1
    bpy.context.scene.eevee.ssr_thickness = 0.1

    # Create a new material for the water
    water_mat = bpy.data.materials.new(name="WaterMaterial")
    water_mat.use_nodes = True
    nodes = water_mat.node_tree.nodes
    nodes.clear()

    # Create Principled BSDF node for water appearance with reflections
    bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    bsdf.inputs['Base Color'].default_value = (0.0, 0.344, 0.55, 1)  # Blue color
    bsdf.inputs['Roughness'].default_value = 0.05  # Sharp reflections
    bsdf.inputs['IOR'].default_value = 1.333  # Index of Refraction
    bsdf.inputs['Alpha'].default_value = 0.8  # Transparency

    print(f'blender version: {bpy.app.version}')
    if (4, 0, 0) >= bpy.app.version:
        bsdf.inputs['Transmission Weight'].default_value = 0.95
        bsdf.inputs['Emission Color'].default_value = (0.002, 0.004, 0.022, 1)
    else:
        bsdf.inputs['Transmission'].default_value = 0.95
        bsdf.inputs['Emission'].default_value = (0.002, 0.004, 0.022, 1)

    # Add Output node and link nodes
    output_node = nodes.new('ShaderNodeOutputMaterial')
    links = water_mat.node_tree.links
    links.new(bsdf.outputs['BSDF'], output_node.inputs['Surface'])

    # Set material settings for transparency and reflection
    water_mat.blend_method = 'HASHED'
    water_mat.shadow_method = 'HASHED'

    # Assign material to the water object
    water_obj.data.materials.append(water_mat)

    # Link the water object to the specified collection
    if map_collection:
        map_collection.objects.link(water_obj)
    else:
        bpy.context.collection.objects.link(water_obj)


# Function to create a rotation matrix from two vectors
def rotation_from_vectors(right_vec, forward_vec, up_vec):
    # Creating a 4x4 rotation matrix from right, up and forward vectors
    rot_mat = Matrix([right_vec, forward_vec, up_vec]).to_4x4()
    return rot_mat


def clamp_output(node, nodes, links):
    clampNode = nodes.new('ShaderNodeClamp')
    clampNode.inputs[1].default_value = 0
    clampNode.inputs[2].default_value = 1
    links.new(node.outputs[0], clampNode.inputs[0])
    return clampNode


def create_material_for_new_models(name, images, uv_map_names, blend_flags, texture_types):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()  # Clear default nodes (it comes with a ShaderNodeBsdfPrincipled and output as default, but we'll just create them ourselves)

    bsdf = nodes.new('ShaderNodeBsdfPrincipled')

    # Set all options to 0, except Roughness to 1
    for input in bsdf.inputs:
        try:
            # Check if the input is 'Roughness' and set it to 1.0
            if input.name == "Roughness" or input.name == "Alpha":
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

    done = False  # We only want to set the first non-normal texture. This is because I havent figured out the pixel shaders yet for these "new" models
    for index, image in enumerate(images):
        texture_type = texture_types[index]
        blend_flag = blend_flags[index]

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

            if blend_flag != 8:
                texImage.image.alpha_mode = 'NONE'
            links.new(texImage.outputs['Alpha'], bsdf.inputs['Alpha'])

            done = True

    output = nodes.new('ShaderNodeOutputMaterial')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Set material settings
    if 8 in blend_flags:
        mat.blend_method = 'BLEND'
        mat.shadow_method = 'CLIP'
        mat.alpha_threshold = 0.0
    else:
        mat.blend_method = 'OPAQUE'
        mat.shadow_method = 'OPAQUE'

    return mat


def create_material_for_old_models(name, images, uv_map_names, blend_flags, texture_types):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()  # Clear default nodes

    # Lighting is computed in the vertex shader in GW, we use this as a stand-in for lighting
    # since GWs models shaders doesn't match very well with Blenders shader systems.
    lighting_node = nodes.new('ShaderNodeCombineXYZ')
    lighting_node.inputs['X'].default_value = 2.0
    lighting_node.inputs['Y'].default_value = 2.0
    lighting_node.inputs['Z'].default_value = 2.0

    prev_texture_node = lighting_node

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

        if blend_flag in [1, 2, 3, 4, 5, 8]:
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
            # we must duplicate the image, otherwise any changed we make to it's properties will apply to the
            # existing image. So we duplicate so we can set this textures alpha mode to None without affecting the existing texture.
            texImageNoAlpha.image = image.copy()
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


def create_map_from_json(context, directory, filename):
    disable_render_preview_background()
    set_clipping_values()
    set_metric_space()

    filepath = os.path.join(directory, filename)
    print('create_map_from_json called')
    gwmb_collection = ensure_collection(context, "GWMB Maps")

    base_name = os.path.basename(filepath).split('.')[0]
    map_hash = base_name.split('_')[1]
    obj_name = "terrain_{}".format(map_hash)

    if map_hash in bpy.data.collections:
        print(f"Object {obj_name} already exists. Skipping.")
        return {'FINISHED'}

    with open(filepath, 'r') as f:
        data = json.load(f)

    if 'terrain' not in data:
        print(f"The key 'terrain' is not in the JSON data. Available keys: {list(data.keys())}")
        return {'FINISHED'}

    terrain_data = data['terrain']

    # Create all images from the json data
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

    map_filehash = data['filehash']
    terrain_tex_atlas_name = "gwmb_terrain_texture_atlas_{}".format(map_filehash)
    if terrain_tex_atlas_name not in bpy.data.images:
        terrain_tex_atlas_image_path = os.path.join(directory, "atlas_{}.png".format(map_filehash))
        terrain_tex_atlas = bpy.data.images.load(terrain_tex_atlas_image_path)
        terrain_tex_atlas.alpha_mode = 'NONE'
        terrain_tex_atlas.name = terrain_tex_atlas_name
    else:
        terrain_tex_atlas = bpy.data.images[terrain_tex_atlas_name]

    map_collection = ensure_collection(context, map_hash, parent_collection=gwmb_collection)

    vertices_data = terrain_data.get('vertices', [])
    indices = terrain_data.get('indices', [])
    texture_indices = [v['texture_index'] for v in vertices_data]

    terrain_width = terrain_data.get('width')
    terrain_height = terrain_data.get('height')

    # Assuming each texture in the atlas occupies 1/8th of the width and height
    atlas_grid_size = 8

    # Function to calculate UV coordinates based on texture index
    def calculate_uv_coordinates(texture_index, corner):
        grid_x = texture_index % atlas_grid_size
        grid_y = texture_index // atlas_grid_size

        # Adjusting the offsets for each corner
        # Corner: 0 = bottom-left, 1 = bottom-right, 2 = top-right, 3 = top-left
        offsets = [(0, 0), (1, 0), (1, 1), (0, 1)]
        u = (grid_x + offsets[corner][0]) / atlas_grid_size
        v = 1 - ((grid_y + offsets[corner][1]) / atlas_grid_size)
        return (u, v)

    # Duplicate vertices at borders
    new_vertices = []
    new_faces = []
    new_uvs = []

    for y in range(terrain_height - 1):
        for x in range(terrain_width - 1):
            # Top-left vertex of each tile
            top_left_vertex_index = y * terrain_width + x
            texture_index = texture_indices[top_left_vertex_index]

            # Create vertices for the current tile
            tile_vertex_indices = []
            for dy in range(2):
                for dx in range(2):
                    idx = (y + dy) * terrain_width + (x + dx)
                    new_vertices.append(swap_axes(vertices_data[idx]['pos']))
                    tile_vertex_indices.append(len(new_vertices) - 1)

            # Create a face for the current tile
            new_faces.append(
                (tile_vertex_indices[0], tile_vertex_indices[1], tile_vertex_indices[3], tile_vertex_indices[2]))

            # Calculate and assign UVs
            for corner in range(4):
                uv = calculate_uv_coordinates(texture_index, corner)
                new_uvs.append(uv)

    # Create new mesh
    terrain_mesh_name = "{}_terrain".format(map_hash)
    mesh = bpy.data.meshes.new(name=terrain_mesh_name)
    mesh.from_pydata(new_vertices, [], new_faces)
    mesh.update()

    # Create UV layer and assign UVs
    uv_layer = mesh.uv_layers.new(name="terrain_uv")
    for i, loop in enumerate(mesh.loops):
        uv_layer.data[i].uv = new_uvs[i]

    # Create object and link to the collection
    obj = bpy.data.objects.new(obj_name, mesh)
    map_collection.objects.link(obj)

    # Assign material using the texture atlas
    material = bpy.data.materials.new(name="TerrainMaterial")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()

    bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    # Set all options to 0, except Roughness to 1
    for input in bsdf.inputs:
        try:
            # Check if the input is 'Roughness' and set it to 1.0
            if input.name == "Roughness" or input.name == "Alpha":
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

    texture_node = nodes.new(type='ShaderNodeTexImage')
    texture_node.image = terrain_tex_atlas
    texture_node.interpolation = 'Closest'

    uv_map_node = nodes.new(type='ShaderNodeUVMap')
    uv_map_node.uv_map = "terrain_uv"

    # Connect nodes
    material.node_tree.links.new(texture_node.inputs['Vector'], uv_map_node.outputs['UV'])
    material.node_tree.links.new(bsdf.inputs['Base Color'], texture_node.outputs['Color'])

    output_node = nodes.new('ShaderNodeOutputMaterial')
    material.node_tree.links.new(output_node.inputs['Surface'], bsdf.outputs['BSDF'])
    material.blend_method = 'OPAQUE'

    # Assign material to the object
    obj.data.materials.append(material)

    top_left_point = (data['min_x'], data['max_z'])
    bottom_right_point = (data['max_x'], data['min_z'])

    # Add water:
    create_water_surface(0.0, top_left_point, bottom_right_point, map_collection)

    # Now that we have added the terrain we will transform the models
    # (translate, rotate and scale) that should already be imported at this point
    map_models_data = data['models']
    parent_collection_name = "GWMB Models"

    # Check if the parent collection exists
    if parent_collection_name in bpy.data.collections:
        parent_collection = bpy.data.collections[parent_collection_name]

        for model_data in map_models_data:
            # Convert the hash to a string with uppercase hexadecimal characters
            model_hash = "0x{:X}".format(model_data['model_hash'])  # Adjust formatting as needed

            # Search for the collection with the matching hash within the parent collection's children
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

                # Calculate rotation matrix only once for all objects in the collection
                right_vector = Vector((model_right['x'], model_right['z'], model_right['y']))
                look_vector = Vector((model_look['x'], model_look['z'], model_look['y']))
                up_vector = Vector((model_up['x'], model_up['z'], model_up['y']))
                rotation_matrix = rotation_from_vectors(right_vector, look_vector, up_vector)

                for obj in model_collection.objects:
                    if obj.name not in map_collection.objects:
                        # Instead of manipulating the original objects, create linked duplicates
                        # The linked duplicate will share the same mesh data as the original object
                        linked_duplicate = obj.copy()
                        linked_duplicate.data = obj.data.copy()
                        linked_duplicate.animation_data_clear()

                        location = Vector((world_pos['x'], world_pos['z'], world_pos['y']))
                        scale_matrix = Matrix.Scale(scale, 4)
                        translation_matrix = Matrix.Translation(location)
                        linked_duplicate.matrix_world = translation_matrix @ rotation_matrix.to_4x4() @ scale_matrix

                        # Link the duplicate to the scene collection (or any specific collection you want)
                        map_collection.objects.link(linked_duplicate)

            else:
                print(f"No collection with the hash {model_hash} found within '{parent_collection_name}'.")

    else:
        print(f"The parent collection '{parent_collection_name}' does not exist in the scene.")

    return {'FINISHED'}


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
                                                      texture_blend_flags, texture_types)
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

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        # Check if the directory exists
        if not os.path.isdir(self.directory):
            self.report({'ERROR'}, "The selected path is not a valid directory")
            return {'CANCELLED'}

        # List all json files in the directory
        file_list = [f for f in os.listdir(self.directory) if f.lower().endswith('.json')]

        map_filename = None
        i = 0
        for filename in file_list:
            print(f'progress: {i}/{len(file_list)}')
            i = i + 1

            if filename.lower().startswith("model_"):
                result = create_mesh_from_json(context, self.directory, filename)
                if 'FINISHED' not in result:
                    self.report({'ERROR'}, f"Failed to import model from file: {filename}")
                    continue

            elif filename.lower().startswith("map_"):
                map_filename = filename

        # Now process the map JSON file to transform all the models (translate, rotate, scale) as well as add terrain
        if not (map_filename == None):
            result = create_map_from_json(context, self.directory, map_filename)
            if 'FINISHED' not in result:
                self.report({'ERROR'}, f"Failed to import map from file: {filename}")

        return {'FINISHED'}


def menu_func_import(self, context):
    self.layout.operator("import_mesh.gwmb_map_folder", text="Import GWMB Map Folder")


def register():
    bpy.utils.register_class(IMPORT_OT_GWMBMap)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(IMPORT_OT_GWMBMap)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
    bpy.ops.import_mesh.gwmb_map_folder('INVOKE_DEFAULT')