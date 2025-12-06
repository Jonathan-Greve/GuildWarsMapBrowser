bl_info = {
    "name": "Guild Wars Map Browser Model Importer",
    "author": "Jonathan Bjorn Greve",
    "version": (2, 0, 0),
    "blender": (5, 0, 0),
    "location": "File > Import > GW Map Browser Model File (.json)",
    "description": "Import a 3D model from a JSON file provided by Guild Wars Map Browser.",
    "warning": "",
    "wiki_url": "github.com/Jonathan-Greve/GuildWarsMapBrowser",
    "category": "Import-Export",
}

import bpy

from . import import_model


def menu_func_import(self, context):
    self.layout.operator(import_model.IMPORT_OT_JSONMesh.bl_idname, text="GW Map Browser Model File (.json)")


def register():
    bpy.utils.register_class(import_model.IMPORT_OT_JSONMesh)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(import_model.IMPORT_OT_JSONMesh)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
