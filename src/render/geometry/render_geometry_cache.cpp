//
// Created by AmazingBuff on 2026/9/22.
//

#include "render_geometry_cache.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Capacity cap of the equip-invalidation inbox: posts beyond it are dropped. A dropped
    // invalidation self-heals - the stale entry still falls to the root check or eviction - so
    // the main-thread sink can never make the render thread allocate unboundedly.
    constexpr size_t Max_Equip_Inbox = 64;
}

RenderGeometryCache& RenderGeometryCache::instance()
{
    static RenderGeometryCache s_instance;
    return s_instance;
}

void RenderGeometryCache::install()
{
    // Equip invalidation must exist before gameplay: registering at Renderer::install's
    // kPostLoadGame keeps the whole render lifecycle in one place and events before the first
    // save load are irrelevant (no corpses to invalidate yet). AddEventSink deduplicates
    // identical sinks, so the repeated message on every load is harmless.
    RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&m_equip_sink);
    logger::info("Registered equip-event sink for render-geometry cache invalidation"sv);
}

RenderGeometryCache::Hit RenderGeometryCache::lookup(RE::FormID form_id)
{
    std::unordered_map<RE::FormID, std::list<Entry>::iterator>::const_iterator const it = m_index.find(form_id);
    if (it == m_index.end())
        return { nullptr, nullptr };

    // Hit: splice the entry to the back (most recently used) and hand out its list and root.
    m_entries.splice(m_entries.end(), m_entries, it->second);
    return { &it->second->geometries, it->second->root3d };
}

void RenderGeometryCache::insert(RE::FormID form_id, RE::NiAVObject* root3d, std::vector<RenderGeometry>&& geometries)
{
    // Callers insert only right after a lookup miss on the same key, so the key cannot be present
    // here. Empty results are rejected: a corpse whose geometry cannot be collected this frame is
    // retried next frame instead of pinning an empty entry in the cache.
    if (geometries.empty())
        return;

    m_entries.push_back(Entry{ form_id, root3d, std::move(geometries) });
    m_index.emplace(form_id, std::prev(m_entries.end()));

    if (m_entries.size() > Max_Corpse_Count)
    {
        // Evict the least recently used entry (list front); dropping it releases the cached
        // NiPointers' last reference, on this thread.
        m_index.erase(m_entries.front().form_id);
        m_entries.pop_front();
    }
}

void RenderGeometryCache::erase(RE::FormID form_id)
{
    // Invalidation path: a drained inbox id or a root-pointer mismatch on a cache hit drops the
    // entry so the corpse re-collects with fresh geometry this frame; dropping the list node
    // releases the cached NiPointers' last reference, on this thread.
    std::unordered_map<RE::FormID, std::list<Entry>::iterator>::const_iterator const it = m_index.find(form_id);
    if (it == m_index.end())
        return;

    m_entries.erase(it->second);
    m_index.erase(it);
}

void RenderGeometryCache::drain_invalidations()
{
    // Swap out the queued invalidations in one short critical section, then apply the erases
    // outside the lock. Ids that are not cache keys are simply dropped - erase of a missing key
    // is a no-op, so no pre-filtering is needed.
    std::vector<RE::FormID> drained;
    {
        std::lock_guard<std::mutex> const lock(m_invalidations_mutex);
        drained.swap(m_invalidations);
    }

    for (RE::FormID const invalidated : drained)
        erase(invalidated);
}

RE::BSEventNotifyControl RenderGeometryCache::EquipSink::ProcessEvent(
    RE::TESEquipEvent const* event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESEquipEvent>* source)
{
    if (event && event->actor)
    {
        RE::TESForm const* const base = RE::TESForm::LookupByID(event->baseObject);
        if (base)
        {
            RE::FormType const type = base->GetFormType();
            if (type == RE::FormType::Armor || type == RE::FormType::Weapon ||
                type == RE::FormType::Light || type == RE::FormType::Ammo)
            {
                RenderGeometryCache::instance().queue_invalidation(event->actor->GetFormID());
            }
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

void RenderGeometryCache::queue_invalidation(RE::FormID form_id)
{
    std::lock_guard<std::mutex> const lock(m_invalidations_mutex);
    if (m_invalidations.size() < Max_Equip_Inbox)
        m_invalidations.push_back(form_id);
}

RenderGeometryCache::RenderGeometryCache() = default;

RenderGeometryCache::~RenderGeometryCache() = default;

PLUGIN_NAMESPACE_END
