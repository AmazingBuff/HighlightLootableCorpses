//
// Created by AmazingBuff on 2026/9/22.
//

#pragma once

#include "render/geometry/render_geometry.h"

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Form-id LRU cache over collected mask geometry, owned by the render thread
// (the Present path inside OverlayDirector::draw).
//
// Threading contract: the LRU core has NO mutex - only the render thread looks
// up, inserts, and erases; the scan task never touches this cache (the scan no
// longer collects geometry at all), so all refcount traffic on the cached
// NiPointers happens on one thread. The ONLY cross-thread structure is the
// cache-owned equip-invalidation inbox (mutex + capped vector): the main-thread
// equip sink posts corpse form ids into it and performs no other cache access
// (never lookup/insert/erase or any LRU member); the render thread applies the
// drained erases via drain_invalidations() at the head of the mask branch each
// frame.
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
//   Equip events reach the same erase through the cache-owned inbox drain, so
//   cached geometry tracks gear changes (taken or added) immediately. Save
//   loads self-heal for free: every ref gets a fresh 3D root on load, so all
//   entries mismatch on the first frame after a load and re-collect.
//
// Self-managed invalidation: the cache owns its equip sink, its inbox and the
// drain. install() registers the sink with the script event source; it is
// idempotent (AddEventSink deduplicates identical sinks), meant to be called
// once at load (Renderer::install, kPostLoadGame) and safe to call repeatedly.
//
// Singleton: exactly one instance exists, so the sink always posts into the
// same instance the renderer draws from - the inbox can never split from the
// LRU.
//
// Lifetime: the cached RenderGeometry NiPointers keep VB/IB (and skin/node)
// alive while cached; eviction drops the last reference in a same-thread
// refcount decrement. The positionless geometry's position_stream buffers are
// device objects owned by the entry: every path that destroys an entry
// (erase, eviction) releases them while the device is alive. Per-frame copies
// of RenderGeometry borrow them and never release. The cache destructor does
// NOT release them - it runs at process-exit teardown ordering where the
// device may already be gone (the same convention as the overlay singletons);
// the OS reclaims what remains. Pointers handed out by lookup stay valid until
// the next cache mutation.
// ---------------------------------------------------------------------------
class RenderGeometryCache
{
public:
    RenderGeometryCache(RenderGeometryCache const&) = delete;
    RenderGeometryCache(RenderGeometryCache const&&) = delete;
    RenderGeometryCache operator=(RenderGeometryCache&) = delete;
    RenderGeometryCache operator=(RenderGeometryCache&&) = delete;

    static RenderGeometryCache& instance();

    // View of a cache hit: the cached geometry list (nullptr on a miss) plus the 3D root
    // captured at collection time, so the caller can validate a hit against the corpse's
    // current 3D without a second hash lookup.
    struct Hit
    {
        std::vector<RenderGeometry> const* geometries;
        RE::NiAVObject* root3d;
    };

    // Registers the cache's equip sink (the invalidation inbox's only producer). Idempotent:
    // AddEventSink deduplicates identical sinks, so the repeated kPostLoadGame install is
    // harmless and leaves exactly one registration.
    void install();

    // Returns the cached geometry list for form_id and refreshes its recency; Hit::geometries
    // is nullptr on a miss.
    Hit lookup(RE::FormID form_id);

    // Stores geometries under form_id together with the ref's 3D root captured at collection
    // time (empty lists are ignored - they are never cached) and evicts the least recently used
    // entry when the cache is at capacity.
    void insert(RE::FormID form_id, RE::NiAVObject* root3d, std::vector<RenderGeometry>&& geometries);

    // Removes one entry without recency side effects; a missing key is a no-op. Render-thread
    // only (called from drain_invalidations and the mask branch's root-mismatch path).
    void erase(RE::FormID form_id);

    // Render-thread only (head of the mask branch): swaps out the equip-invalidation inbox in
    // one short critical section, then erases each drained corpse id that is a cache key (a
    // missing key is a no-op, so no pre-filtering is needed - the cache is the only judge of
    // its own key set).
    void drain_invalidations();

private:
    // Main-thread equip sink: whenever an equippable item is equipped on or taken off an actor,
    // queue that actor's form id for cache invalidation on the render thread. The sink performs
    // NO cache access beyond the inbox push (queue_invalidation) - the LRU core is
    // render-thread-only - and stays silent per event.
    //
    // baseObject is filtered by form type: spells, abilities and shouts fire TESEquipEvent too
    // but have no mesh, so only Armor/Weapon/Light/Ammo pass. Dedup is unnecessary - the render
    // side erases per drained id and erase of a missing key is a no-op.
    class EquipHandler final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            RE::TESEquipEvent const* event,
            [[maybe_unused]] RE::BSTEventSource<RE::TESEquipEvent>* source) override;
    };

    struct Entry
    {
        RE::FormID form_id;
        RE::NiAVObject* root3d;
        std::vector<RenderGeometry> geometries;
    };
private:
    RenderGeometryCache();
    ~RenderGeometryCache();

    // Releases the device objects owned by a dying entry's geometries (the positionless
    // geometry's position_stream buffers; see RenderGeometry::position_buffer). Render-thread
    // only, reached from erase and eviction - both run while the device is alive. Per-frame
    // copies of RenderGeometry borrow these pointers and never release them.
    static void release_position_buffers(std::vector<RenderGeometry> const& geometries);

    // Main-thread posting target of EquipSink: the ONLY shared state between the threads. Posts
    // past the cap are dropped (see Max_Equip_Inbox in the cpp).
    void queue_invalidation(RE::FormID form_id);
private:
    // list + hash map: O(1) lookup and O(1) recency moves; the front of the list is the least
    // recently used entry, the back the most recently used one.
    std::list<Entry> m_entries;
    std::unordered_map<RE::FormID, std::list<Entry>::iterator> m_indices;

    // Equip-invalidation inbox: the ONLY cross-thread state in the cache design. The
    // main-thread equip sink pushes affected corpse form ids, the render thread drains it at
    // the head of the mask branch each frame; both sides only ever touch it under
    // m_invalidations_mutex.
    std::mutex m_invalidations_mutex;
    std::vector<RE::FormID> m_invalidations;

    EquipHandler m_equip_handler;
};

PLUGIN_NAMESPACE_END
