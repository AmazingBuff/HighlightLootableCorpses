#pragma once

#include "filter/loot_filter.h"

PLUGIN_NAMESPACE_BEGIN

// Detection-only corpse scan: it loot-filters the references in range and publishes a
// distance-sorted snapshot. Mask geometry is not collected here - the render thread resolves
// each snapshot corpse through its form-id LRU cache and collects on a cache miss
// (RenderGeometryCache).
class CorpseScan
{
public:
    CorpseScan(CorpseScan const&) = delete;
    CorpseScan(CorpseScan const&&) = delete;
    CorpseScan operator=(CorpseScan&) = delete;
    CorpseScan operator=(CorpseScan&&) = delete;

    static CorpseScan& instance();

    struct CorpseInfo
    {
        RE::FormID form_id;              // FormID of the Actor / ash pile
        RE::NiPoint3 anchor;             // world-coordinate anchor (used for projection and the frustum culls on both threads)
        RE::NiPoint3 bound_min;          // minimum corner of the 3D world bounding box (AABB); icon anchor
        RE::NiPoint3 bound_max;          // maximum corner of the 3D world bounding box (AABB); icon anchor
        float radius;                    // world bounding-sphere radius (used by the frustum culls on both threads)
        float distance;                  // distance to the player (fade alpha on the render thread, menu nearest)

        // Loot filtering (computed during the scan by LootFilter::evaluate): a bit mask of the matched value categories and the highest single-item value
        RE::stl::enumeration<LootFilter::Category> loot_categories;
        int32_t best_item_value;
    };

    void search();
    [[nodiscard]] std::vector<CorpseInfo> snapshot();
private:
    CorpseScan();
    ~CorpseScan();
private:
    // Confirmed lootable corpses (written by the main thread, read by the render thread through a snapshot)
    std::mutex m_mutex;
    std::vector<CorpseInfo> m_corpses;
    std::unordered_set<RE::FormID> m_logged_corpses;
};

PLUGIN_NAMESPACE_END
