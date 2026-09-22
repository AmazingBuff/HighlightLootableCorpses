//
// Created by AmazingBuff on 2026/9/22.
//

#pragma once

#include "render/geometry/render_geometry.h"

#include <list>
#include <unordered_map>

PLUGIN_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Form-id LRU cache over collected mask geometry, owned by the render thread
// (the Present path inside OverlayDirector::draw).
//
// Threading contract: NO mutex. Only the render thread looks up, inserts, and
// evicts; the scan task never touches this cache (the scan no longer collects
// geometry at all), so all refcount traffic on the cached NiPointers happens on
// one thread.
//
// Why the cache exists: mask-geometry collection moved back to the render
// thread so a corpse that becomes visible after a fast camera turn is collected
// and highlighted on that very frame instead of waiting up to one scan interval
// for the scan-side pass. A miss collects that corpse on the spot and caches
// the result; a hit reuses the cached list without re-traversing the scene
// graph, so the steady state pays no traversal or calibration cost.
//
// Semantics:
// - capacity is exactly Max_Corpse_Count; at capacity, insert evicts the least
//   recently used entry;
// - lookup refreshes recency on a hit; insert refreshes it too, so both paths
//   keep an entry hot;
// - empty collections are NOT cached: a corpse whose geometry cannot be
//   collected this frame is retried on the next one (the caller skips the
//   insert); frustum-culled corpses never touch the cache at all - recency
//   tracks drawn corpses, keeping the nearest on-screen set hot;
// - a stale entry (the corpse's 3D composition changed since collection) is
//   accepted until eviction, the same staleness class as the previous
//   scan-side collection, now bounded by 32-corpse recency churn.
//
// Lifetime: the cached RenderGeometry NiPointers keep VB/IB (and skin/node)
// alive while cached; eviction drops the last reference in a same-thread
// refcount decrement. Pointers handed out by lookup stay valid until the next
// cache mutation.
// ---------------------------------------------------------------------------
class RenderGeometryCache
{
public:
    // Returns the cached geometry list for form_id and refreshes its recency; nullptr on a miss.
    std::vector<RenderGeometry> const* lookup(RE::FormID form_id);

    // Stores geometries under form_id (empty lists are ignored - they are never cached) and
    // evicts the least recently used entry when the cache is at capacity.
    void insert(RE::FormID form_id, std::vector<RenderGeometry>&& geometries);

private:
    struct Entry
    {
        RE::FormID form_id;
        std::vector<RenderGeometry> geometries;
    };

    // list + hash map: O(1) lookup and O(1) recency moves; the front of the list is the least
    // recently used entry, the back the most recently used one.
    std::list<Entry> m_entries;
    std::unordered_map<RE::FormID, std::list<Entry>::iterator> m_index;
};

PLUGIN_NAMESPACE_END
