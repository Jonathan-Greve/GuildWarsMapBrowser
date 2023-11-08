import bpy
import json
import time
import os
import numpy as np
from mathutils import Vector, Matrix

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


def create_material_for_new_models(name, images, uv_map_names, texture_types):
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

        # Update alpha_val node
        modify_alpha_node = None
        if blend_flag in [3, 6, 7]:
            subNode = nodes.new('ShaderNodeMath')
            subNode.operation = 'SUBTRACT'
            links.new(const_node_1.outputs[0], subNode.inputs[0])
            links.new(texImage.outputs['Alpha'], subNode.inputs[1])
            modify_alpha_node = subNode.outputs[0]
        elif blend_flag == 0:
            #texImage.image.alpha_mode = 'NONE'
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
            links.new(texImage.outputs['Color'], mixRGBNode.inputs[2])
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


def create_map_from_json(context, filepath):
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

    map_collection = ensure_collection(context, map_hash, parent_collection=gwmb_collection)

    vertices_data = terrain_data.get('vertices', [])
    indices = terrain_data.get('indices', [])
    vertices = [swap_axes(v['pos']) for v in vertices_data]

    # Normals
    normals = [swap_axes(v['normal']) for v in vertices_data]

    # Faces
    faces = [tuple(indices[i:i + 3]) for i in range(0, len(indices), 3)]

    mesh = bpy.data.meshes.new(name="{}_terrain".format(map_hash))
    mesh.from_pydata(vertices, [], faces)

    # Set Normals
    mesh.normals_split_custom_set_from_vertices(normals)

    uv_layer = mesh.uv_layers.new(name="terrain_uv")
    uvs = []
    for vertex in vertices_data:
        uvs.append(vertex['uv_coord'])
    for i, loop in enumerate(mesh.loops):
        uv_layer.data[i].uv = (uvs[loop.vertex_index]['x'], uvs[loop.vertex_index]['y'])

    mesh.update()

    obj = bpy.data.objects.new(obj_name, mesh)

    # Link the object to the model's collection directly
    map_collection.objects.link(obj)

    # Make sure the object is also in the scene collection for visibility
    context.view_layer.objects.active = obj
    obj.select_set(True)

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
                uvs.append((vertex['texture_uv_coords'][tex_index]['x'], 1 - vertex['texture_uv_coords'][tex_index]['y']))

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

        map_full_path = None
        i = 0
        for filename in file_list:
            print(f'progress: {i}/{len(file_list)}')
            i = i + 1
            full_path = os.path.join(self.directory, filename)

            if filename.lower().startswith("model_"):
                result = create_mesh_from_json(context, self.directory, filename)
                if 'FINISHED' not in result:
                    self.report({'ERROR'}, f"Failed to import model from file: {filename}")
                    continue

            elif filename.lower().startswith("map_"):
                map_full_path = full_path

        # Now process the map JSON file to transform all the models (translate, rotate, scale) as well as add terrain
        if not (map_full_path == None):
            result = create_map_from_json(context, map_full_path)
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