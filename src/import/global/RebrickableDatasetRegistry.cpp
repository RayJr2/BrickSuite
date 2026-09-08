#include "RebrickableDatasetRegistry.h"

const QVector<RebrickableDatasetDescriptor>& RebrickableDatasetRegistry::datasets()
{
    static const QVector<RebrickableDatasetDescriptor> registry = {
        {RebrickableDatasetId::Themes, "Themes", "themes.csv", {"id", "name", "parent_id"}, {}, 1, true, true},
        {RebrickableDatasetId::Colors, "Colors", "colors.csv", {"id", "name", "rgb", "is_trans"}, {}, 2, true, false},
        {RebrickableDatasetId::PartCategories, "Part Categories", "part_categories.csv", {"id", "name"}, {}, 3, true, false},
        {RebrickableDatasetId::Parts, "Parts", "parts.csv", {"part_num", "name", "part_cat_id", "part_material"}, {RebrickableDatasetId::PartCategories}, 4, true, false},
        {RebrickableDatasetId::PartRelationships, "Part Relationships", "part_relationships.csv", {"rel_type", "child_part_num", "parent_part_num"}, {RebrickableDatasetId::Parts}, 5, true, false},
        {RebrickableDatasetId::Sets, "Sets", "sets.csv", {"set_num", "name", "year", "theme_id", "num_parts", "img_url"}, {}, 6, true, true},
        {RebrickableDatasetId::Minifigs, "Minifigs", "minifigs.csv", {"fig_num", "name", "num_parts", "img_url"}, {}, 7, true, true},
        {RebrickableDatasetId::Elements, "Elements", "elements.csv", {"element_id", "part_num", "color_id", "design_id"}, {RebrickableDatasetId::Parts, RebrickableDatasetId::Colors}, 8, true, false},
        {RebrickableDatasetId::Inventories, "Inventories", "inventories.csv", {"id", "version", "set_num"}, {RebrickableDatasetId::Sets}, 9, true, false},
        {RebrickableDatasetId::InventoryParts, "Inventory Parts", "inventory_parts.csv", {"inventory_id", "part_num", "color_id", "quantity", "is_spare", "img_url"}, {RebrickableDatasetId::Inventories, RebrickableDatasetId::Parts, RebrickableDatasetId::Colors}, 10, true, false},
        {RebrickableDatasetId::InventoryMinifigs, "Inventory Minifigs", "inventory_minifigs.csv", {"inventory_id", "fig_num", "quantity"}, {RebrickableDatasetId::Inventories, RebrickableDatasetId::Minifigs}, 11, true, false},
        {RebrickableDatasetId::InventorySets, "Inventory Sets", "inventory_sets.csv", {"inventory_id", "set_num", "quantity"}, {RebrickableDatasetId::Inventories, RebrickableDatasetId::Sets}, 12, true, false}
    };
    return registry;
}

const RebrickableDatasetDescriptor*
RebrickableDatasetRegistry::descriptor(RebrickableDatasetId id)
{
    for (const auto& dataset : datasets())
        if (dataset.id == id)
            return &dataset;
    return nullptr;
}
