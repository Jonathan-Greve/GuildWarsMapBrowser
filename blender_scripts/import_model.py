import bpy
import json


def create_image_from_rgba(name, width, height, pixels):
    flattened_pixels = [channel for px in pixels for channel in (px['x'], px['y'], px['z'], px['w'])]
    image = bpy.data.images.new(name, width=width, height=height)
    image.pixels = flattened_pixels
    return image


def create_multi_texture_material(name, images):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    bsdf = nodes["Principled BSDF"]
    last_color = bsdf.inputs['Base Color']

    for image in images:
        texImage = nodes.new('ShaderNodeTexImage')
        mixRGB = nodes.new('ShaderNodeMixRGB')

        texImage.image = image
        mixRGB.blend_type = 'MULTIPLY'

        links.new(mixRGB.inputs[1], texImage.outputs['Color'])
        links.new(mixRGB.inputs[2], last_color)
        last_color = mixRGB.outputs['Color']

    links.new(bsdf.inputs['Base Color'], last_color)
    links.new(nodes['Material Output'].inputs['Surface'], bsdf.outputs['BSDF'])

    return mat


def create_mesh_from_json(context, filepath):
    with open(filepath, 'r') as f:
        data = json.load(f)

    images = [create_image_from_rgba("JSON_Texture_{}".format(i), tex['width'], tex['height'], tex['rgba_pixels']) for
              i, tex in enumerate(data.get('textures', []))]
    material = create_multi_texture_material("JSON_Material", images)

    for submodel in data.get('submodels', []):
        vertices_data = submodel.get('vertices', [])
        indices = submodel.get('indices', [])

        vertices = [tuple(v['pos'].values()) for v in vertices_data]

        # Extract all UV coords
        uvs_list = []
        for vertex in vertices_data:
            uvs = vertex['texture_uv_coords']
            for uv_coord in uvs:
                uvs_list.append((uv_coord['x'], uv_coord['y']))

        faces = [tuple(indices[i:i + 3]) for i in range(0, len(indices), 3)]

        mesh = bpy.data.meshes.new(name="JSON_Submodel")
        mesh.from_pydata(vertices, [], faces)

        # Add UV maps
        uv_layer = mesh.uv_layers.new()
        for i, loop in enumerate(mesh.loops):
            uv_layer.data[i].uv = uvs_list[loop.vertex_index]

        mesh.update()

        obj = bpy.data.objects.new("JSON_Submodel_Object", mesh)
        obj.data.materials.append(material)

        context.collection.objects.link(obj)
        context.view_layer.objects.active = obj
        obj.select_set(True)

    return {'FINISHED'}


class IMPORT_OT_JSONMesh(bpy.types.Operator):
    bl_idname = "import_mesh.json"
    bl_label = "Import JSON Mesh"
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
