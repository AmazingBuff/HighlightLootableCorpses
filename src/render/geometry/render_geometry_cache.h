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
// erases; the scan task never touches this cache (the scan no longer collects
// geometry at all), so all refcount traffic on the cached NiPointers happens on
// one thread. The main-thread equip sink does not touch this cache either: it
// only posts corpse form ids into OverlayDirector's mutex-guarded inbox, and
// the render thread drains that inbox each frame and erases the drained keys
// here - the inbox is the ONLY cross-thread structure in the invalidation
// design.
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
// - invalidation: every entry stores the 3D root (ref->GetCurrent3D()) captured
//   at collection time, and the render branch validates it on each hit; a
//   mismatch erases the entry and the corpse re-collects on the miss path.
//   Equip events reach the same erase through the inbox drain, so cached
//   geometry tracks gear changes (taken or added) immediately. Save loads
//   self-heal for free: every ref gets a fresh 3D root on load, so all entries
//   mismatch on the first frame after a load and re-collect.
//
// Lifetime: the cached RenderGeometry NiPointers keep VB/IB (and skin/node)
// alive while cached; eviction drops the last reference in a same-thread
// refcount decrement. Pointers handed out by lookup stay valid until the next
// cache mutation.
// ---------------------------------------------------------------------------
class RenderGeometryCache
{
public:
    // View of a cache hit: the cached geometry list (nullptr on a miss) plus the 3D root
    // captured at collection time, so the caller can validate a hit against the corpse's
    // current 3D without a second hash lookup.
    struct Hit
    {
        std::vector<RenderGeometry> const* geometries;
        RE::NiAVObject* root3d;
    };

    // Returns the cached geometry list for form_id and refreshes its recency; Hit::geometries
    // is nullptr on a miss.
    Hit lookup(RE::FormID form_id);

    // Stores geometries under form_id together with the ref's 3D root captured at collection
    // time (empty lists are ignored - they are never cached) and evicts the least recently used
    // entry when the cache is at capacity.
    void insert(RE::FormID form_id, RE::NiAVObject* root3d, std::vector<RenderGeometry>&& geometries);

    // Removes one entry without recency side effects; a missing key is a no-op. Render-thread
    // only (called from the mask branch's inbox drain and root-mismatch path).
    void erase(RE::FormID form_id);

private:
    struct Entry
    {
        RE::FormID form_id;
        RE::NiAVObject* root3d;
        std::vector<RenderGeometry> geometries;
    };

    // list + hash map: O(1) lookup and O(1) recency moves; the front of the list is the least
    // recently used entry, the back the most recently used one.
    std::list<Entry> m_entries;
    std::unordered_map<RE::FormID, std::list<Entry>::iterator> m_index;
};

PLUGIN_NAMESPACE_END
