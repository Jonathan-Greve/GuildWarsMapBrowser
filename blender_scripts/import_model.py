import bpy
import json

def create_mesh_from_json(context, filepath):
    # Load JSON data from the file
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    # Loop through each submodel to create meshes
    for submodel in data.get('submodels', []):
        vertices_data = submodel.get('vertices', [])
        indices = submodel.get('indices', [])

        # Extract vertex positions from the vertices data
        vertices = [tuple(v['pos'].values()) for v in vertices_data]

        # Group the indices into faces (assuming triangles for now)
        faces = [tuple(indices[i:i+3]) for i in range(0, len(indices), 3)]

        # Create a new mesh
        mesh = bpy.data.meshes.new(name="JSON_Submodel")
        mesh.from_pydata(vertices, [], faces)
            
        # Update the mesh with the new data
        mesh.update()

        # Create an object with the mesh
        obj = bpy.data.objects.new("JSON_Submodel_Object", mesh)

        # Link the object to the scene
        context.collection.objects.link(obj)

        # Make the new object the active object (optional)
        context.view_layer.objects.active = obj
        obj.select_set(True)
    
    return {'FINISHED'}

class IMPORT_OT_JSONMesh(bpy.types.Operator):
    bl_idname = "import_mesh.json"
    bl_label = "Import JSON Mesh"
    bl_options = {'REGISTER', 'UNDO'}
    
    # Define this to tell Blender to open a file dialog
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
    bpy.ops.import_mesh.json('INVOKE_DEFAULT')  # Test call to invoke the operator immediately
