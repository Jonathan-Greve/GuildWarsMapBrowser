#pragma once

void draw_file_info_editor_panel(std::vector<std::vector<std::string>>& csv_data);

enum class ModelTypes
{
    // NPCs
    npc_friendly, // For NPCs that are always friendly (to the best of your knowledge)
    npc_enemy,// For NPCs that are always enemies (to the best of your knowledge)

    // Props
    prop_building, // Structures like houses, shops, wells, ruins, barns, and temples.
    prop_foliage, // Trees, bushes, flowers, grass, vines, and other plant life.
    prop_furniture, // Chairs, tables, bookcases, beds, benches and other decorations.
    prop_nature, // Rocks, boulders, cliffs, water bodies, and other natural formations
    prop_barrier, // Gates, walls, hedges, and other objects used to block or define boundaries.
    prop_miscellaneous, // Items that don't fit into other prop categories

    // Weapons
    item_weapon_axe,
    item_weapon_sword,
    item_weapon_daggers,
    item_weapon_hammer,
    item_weapon_scythe,
    item_weapon_spear,
    item_weapon_bow,
    item_weapon_wand,
    item_weapon_staff,
    item_weapon_focus,
    item_weapon_shield,

    // Armor
    item_armor_helm,
    item_armor_chest,
    item_armor_hands,
    item_armor_legs,
    item_armor_feet,


    // other item types
    item_consumable, // alcohol, sweets
    item_upgrade_component, // runes, insignias
    item_trophy, // the items accepted by collectors
    item_crafting, // Used for crafting armor and weapons: iron ingots, ectos etc.
    item_key, // keys for openings chests, e.g. Lockpick
    item_miniature, // Miniature like those from birthday gifts
    item_polymock, // Polymock pieces
    item_quest, // Items used during quests
    item_storybook, // Hero's Handbook etc. 
    item_miscellaneous, // When you don't know or it doesn't fit in the above.

    // Other
    miscellaneous, // When it doesn't fit an of the categories above or you don't know.

    unknown // Used only internally
};

inline std::string ModelTypeToString(ModelTypes type)
{
    switch (type)
    {
    case ModelTypes::npc_friendly: return "npc_friendly";
    case ModelTypes::npc_enemy: return "npc_enemy";
    case ModelTypes::prop_building: return "prop_building";
    case ModelTypes::prop_foliage: return "prop_foliage";
    case ModelTypes::prop_furniture: return "prop_furniture";
    case ModelTypes::prop_nature: return "prop_nature";
    case ModelTypes::prop_barrier: return "prop_barrier";
    case ModelTypes::prop_miscellaneous: return "prop_miscellaneous";
    case ModelTypes::item_weapon_axe: return "item_weapon_axe";
    case ModelTypes::item_weapon_sword: return "item_weapon_sword";
    case ModelTypes::item_weapon_daggers: return "item_weapon_daggers";
    case ModelTypes::item_weapon_hammer: return "item_weapon_hammer";
    case ModelTypes::item_weapon_scythe: return "item_weapon_scythe";
    case ModelTypes::item_weapon_spear: return "item_weapon_spear";
    case ModelTypes::item_weapon_bow: return "item_weapon_bow";
    case ModelTypes::item_weapon_wand: return "item_weapon_wand";
    case ModelTypes::item_weapon_staff: return "item_weapon_staff";
    case ModelTypes::item_weapon_focus: return "item_weapon_focus";
    case ModelTypes::item_weapon_shield: return "item_weapon_shield";
    case ModelTypes::item_armor_helm: return "item_armor_helm";
    case ModelTypes::item_armor_chest: return "item_armor_chest";
    case ModelTypes::item_armor_hands: return "item_armor_hands";
    case ModelTypes::item_armor_legs: return "item_armor_legs";
    case ModelTypes::item_armor_feet: return "item_armor_feet";
    case ModelTypes::item_consumable: return "item_consumable";
    case ModelTypes::item_upgrade_component: return "item_upgrade_component";
    case ModelTypes::item_trophy: return "item_trophy";
    case ModelTypes::item_crafting: return "item_crafting";
    case ModelTypes::item_key: return "item_key";
    case ModelTypes::item_miniature: return "item_miniature";
    case ModelTypes::item_polymock: return "item_polymock";
    case ModelTypes::item_quest: return "item_quest";
    case ModelTypes::item_storybook: return "item_storybook";
    case ModelTypes::item_miscellaneous: return "item_miscellaneous";
    case ModelTypes::miscellaneous: return "miscellaneous";
    default: return "unknown";
    }
}

inline ModelTypes StringToModelType(const std::string& typeString)
{
    if (typeString == "npc_friendly") return ModelTypes::npc_friendly;
    if (typeString == "npc_enemy") return ModelTypes::npc_enemy;
    if (typeString == "prop_building") return ModelTypes::prop_building;
    if (typeString == "prop_foliage") return ModelTypes::prop_foliage;
    if (typeString == "prop_furniture") return ModelTypes::prop_furniture;
    if (typeString == "prop_nature") return ModelTypes::prop_nature;
    if (typeString == "prop_barrier") return ModelTypes::prop_barrier;
    if (typeString == "prop_miscellaneous") return ModelTypes::prop_miscellaneous;
    if (typeString == "item_weapon_axe") return ModelTypes::item_weapon_axe;
    if (typeString == "item_weapon_sword") return ModelTypes::item_weapon_sword;
    if (typeString == "item_weapon_daggers") return ModelTypes::item_weapon_daggers;
    if (typeString == "item_weapon_hammer") return ModelTypes::item_weapon_hammer;
    if (typeString == "item_weapon_scythe") return ModelTypes::item_weapon_scythe;
    if (typeString == "item_weapon_spear") return ModelTypes::item_weapon_spear;
    if (typeString == "item_weapon_bow") return ModelTypes::item_weapon_bow;
    if (typeString == "item_weapon_wand") return ModelTypes::item_weapon_wand;
    if (typeString == "item_weapon_staff") return ModelTypes::item_weapon_staff;
    if (typeString == "item_weapon_focus") return ModelTypes::item_weapon_focus;
    if (typeString == "item_weapon_shield") return ModelTypes::item_weapon_shield;
    if (typeString == "item_armor_helm") return ModelTypes::item_armor_helm;
    if (typeString == "item_armor_chest") return ModelTypes::item_armor_chest;
    if (typeString == "item_armor_hands") return ModelTypes::item_armor_hands;
    if (typeString == "item_armor_legs") return ModelTypes::item_armor_legs;
    if (typeString == "item_armor_feet") return ModelTypes::item_armor_feet;
    if (typeString == "item_consumable") return ModelTypes::item_consumable;
    if (typeString == "item_upgrade_component") return ModelTypes::item_upgrade_component;
    if (typeString == "item_trophy") return ModelTypes::item_trophy;
    if (typeString == "item_crafting") return ModelTypes::item_crafting;
    if (typeString == "item_key") return ModelTypes::item_key;
    if (typeString == "item_miniature") return ModelTypes::item_miniature;
    if (typeString == "item_polymock") return ModelTypes::item_polymock;
    if (typeString == "item_quest") return ModelTypes::item_quest;
    if (typeString == "item_storybook") return ModelTypes::item_storybook;
    if (typeString == "item_miscellaneous") return ModelTypes::item_miscellaneous;
    if (typeString == "miscellaneous") return ModelTypes::miscellaneous;
    // If no match is found, return unknown
    return ModelTypes::unknown;
}