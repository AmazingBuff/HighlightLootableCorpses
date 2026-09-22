//
// Created by AmazingBuff on 2026/9/22.
//

#include "render_geometry_cache.h"

PLUGIN_NAMESPACE_BEGIN

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
    // Invalidation path: an equip event (drained from the inbox) or a root-pointer mismatch on a
    // cache hit drops the entry so the corpse re-collects with fresh geometry this frame; dropping
    // the list node releases the cached NiPointers' last reference, on this thread.
    std::unordered_map<RE::FormID, std::list<Entry>::iterator>::const_iterator const it = m_index.find(form_id);
    if (it == m_index.end())
        return;

    m_entries.erase(it->second);
    m_index.erase(it);
}

PLUGIN_NAMESPACE_END
