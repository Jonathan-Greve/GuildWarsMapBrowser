bl_info = {
    "name": "Guild Wars Map Browser Map Importer",
    "author": "Jonathan Bjørn Greve",
    "version": (1, 0),
    "blender": (3, 6, 5),
    "location": "File > Import > GW Map Browser Map Folder",
    "description": "Import a Guild Wars Map from a JSON file provided by Guild Wars Map Browser.",
    "warning": "This might take a long time.",
    "wiki_url": "github.com/Jonathan-Greve/GuildWarsMapBrowser",
    "category": "Import-Export",
}

import bpy

from . import import_map


def menu_func_import(self, context):
    self.layout.operator(import_map.IMPORT_OT_GWMBMap.bl_idname, text="GW Map Browser Map Folder")


def register():
    bpy.utils.register_class(import_map.IMPORT_OT_GWMBMap)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(import_map.IMPORT_OT_GWMBMap)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
