#pragma once

#include "filter/loot_filter.h"
#include "render/geometry/render_geometry.h"

PLUGIN_NAMESPACE_BEGIN

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

        // Mask-path render geometries, collected by CorpseScan::search() on the SKSE main-thread scan
        // task whenever the plugin is enabled (icon mode included, so switching display modes never
        // shows a gap; empty only while disabled - the corpse list itself is never gated). The render
        // thread culls on anchor/radius, refreshes the style color per frame from distance + config +
        // pulse, and consumes these geometries positionally. Kept last so the common (empty while
        // disabled) copy for icon/menu consumers costs only the vector header.
        std::vector<RenderGeometry> render_geometries;
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
