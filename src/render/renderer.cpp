//
// Created by AmazingBuff on 2026/9/13.
//

#include "renderer.h"

#include "render_util.h"
#include "ui_render_hook.h"

#include "config/config.h"
#include "icon/icon_layout.h"
#include "icon/icon_overlay.h"
#include "mask/mask_overlay.h"
#include "render/dx11/common_states.h"
#include "render/geometry/render_geometry_cache.h"
#include "render/shader_manager.h"
#include "search/corpse_finder.h"
#include "ui/pulse_timer.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr float Hold_Fraction = 0.10f;

    float pulse_alpha(float progress)
    {
        return progress <= Hold_Fraction ? 1.0f : std::clamp(1.0f - (progress - Hold_Fraction) / (1.0f - Hold_Fraction), 0.0f, 1.0f);
    }

    float corpse_alpha(Config const& cfg, float distance, float alpha)
    {
        float const fade_range = std::max(cfg.max_distance - cfg.fade_start_distance, 1.0f);
        float const f = distance <= cfg.fade_start_distance ? 1.0f : 1.0f - std::clamp((distance - cfg.fade_start_distance) / fade_range, 0.0f, 1.0f);
        float const fade = std::pow(f, cfg.fade_power);
        return std::clamp(cfg.min_opacity + fade * alpha * (1.0f - cfg.min_opacity), cfg.min_opacity, 1.0f);
    }

    RE::BSGraphics::ViewData const* update_view_data(RE::NiCamera const* camera)
    {
        RE::BSGraphics::ViewData const* view_data = nullptr;
        if (RE::BSGraphics::State* state = RE::BSGraphics::State::GetSingleton())
        {
            RE::BSGraphics::State::RUNTIME_DATA& state_rt = state->GetRuntimeData();
            for (RE::BSGraphics::CameraStateData const& cam_data : state_rt.cameraDataCacheA)
            {
                if (cam_data.referenceCamera == camera)
                {
                    view_data = std::addressof(cam_data.GetCameraStateRuntimeData().camViewData);
                    break;
                }
            }
            if (!view_data && !state_rt.cameraDataCacheA.empty())
                view_data = std::addressof(state_rt.cameraDataCacheA.front().GetCameraStateRuntimeData().camViewData);
        }
        return view_data;
    }

    bool project_icon_tip(RE::NiCamera* camera, RE::BSGraphics::ViewData const* view_data,
        DirectX::XMFLOAT3 const& point, float width, float height, DirectX::XMFLOAT2& tip)
    {
        float px = 0.0f, py = 0.0f, depth = 0.0f;
        if (project(camera, point, width, height, px, py, depth))
            // The depth gate rejects points behind the near plane / past the far plane: the
            // projection math keeps producing coordinates there, but they mirror through the
            // camera plane and would place ghost icons that swing wildly as the view rotates.
            return depth > 0.0f && depth <= 1.0f && Icon::icon_screen_tip(px, py, tip);
        if (!view_data)
            return false;
        Matrix const& matrix = view_data->viewProjMatrixUnjittered._11 != 0.0f ? view_data->viewProjMatrixUnjittered : view_data->viewProjMat;
        DirectX::XMFLOAT4 clip{};
        DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(point.x, point.y, point.z, 1.0f), matrix));
        return Icon::icon_clip_tip(clip, width, height, tip);
    }

    bool render_target_dimensions(REX::W32::ID3D11RenderTargetView* target, uint32_t& width, uint32_t& height)
    {
        if (!target)
            return false;

        REX::W32::D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
        target->GetDesc(&view_desc);

        uint32_t mip_slice = 0;
        switch (view_desc.viewDimension)
        {
        case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2D:
            mip_slice = view_desc.texture2D.mipSlice;
            break;
        case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
            mip_slice = view_desc.texture2DArray.mipSlice;
            break;
        case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2DMS:
        case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
            break;
        default:
            return false;
        }

        REX::W32::ID3D11Resource* resource = nullptr;
        target->GetResource(&resource);
        if (!resource)
            return false;

        REX::W32::ID3D11Texture2D* texture = nullptr;
        REX::W32::HRESULT const query_hr = resource->QueryInterface(REX::W32::IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
        resource->Release();
        if (!REX::W32::SUCCESS(query_hr) || !texture)
            return false;

        REX::W32::D3D11_TEXTURE2D_DESC texture_desc{};
        texture->GetDesc(&texture_desc);
        texture->Release();

        if (mip_slice >= texture_desc.mipLevels || mip_slice >= 32)
            return false;

        width = std::max(1u, texture_desc.width >> mip_slice);
        height = std::max(1u, texture_desc.height >> mip_slice);
        return true;
    }

    class OverlayDirector
    {
    public:
        static OverlayDirector& instance()
        {
            static OverlayDirector s_instance;
            return s_instance;
        }

        void on_pre_ui_draw()
        {
            schedule_scan();

            RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer)
                return;

            RE::BSGraphics::RendererData& rt = renderer->GetRuntimeData();
            REX::W32::ID3D11Device* device = rt.forwarder;
            REX::W32::ID3D11DeviceContext* context = rt.context;
            if (!device || !context)
                return;

            REX::W32::ID3D11RenderTargetView* const output_target = rt.renderTargets[RE::RENDER_TARGETS::kFRAMEBUFFER].RTV;
            uint32_t width = 0;
            uint32_t height = 0;
            if (!render_target_dimensions(output_target, width, height))
                return;

            RE::BSGraphics::State* bs_state = RE::BSGraphics::State::GetSingleton();
            uint32_t const frame = bs_state ? bs_state->GetFrameCount() : 0;

            if (frame == 0 || frame == m_last_drawn_frame)
                return;

            m_last_drawn_frame = frame;
            draw(device, context, output_target, width, height);
        }

    private:
        OverlayDirector() :
            m_scan_in_flight(false),
            m_last_drawn_frame(std::numeric_limits<uint32_t>::max()),
            m_ready(false) {}

        void schedule_scan()
        {
            std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
            if (now - m_last_scan >= std::chrono::milliseconds(Setting::instance().get_config().scan_interval_ms) &&
                !m_scan_in_flight.exchange(true)) {
                m_last_scan = now;
                SKSE::GetTaskInterface()->AddTask([this]
                {
                    // search() always runs: the corpse list has consumers besides the overlay
                    // (the MCP menu reads the snapshot even with rendering disabled). It is
                    // detection-only now - loot filtering, bounds and distance - and no longer
                    // touches the scene graph for geometry. Mask-geometry collection happens on
                    // the render thread inside draw(), behind the form-id LRU cache: a cache
                    // miss collects that corpse's geometries on the spot, so a corpse that
                    // becomes visible after a camera turn highlights the same frame instead of
                    // waiting up to one scan interval.
                    CorpseScan::instance().search();

                    m_scan_in_flight.store(false);
                });
            }
        }

        void draw(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
            REX::W32::ID3D11RenderTargetView* output_target, uint32_t width, uint32_t height)
        {
            if (!init(device))
                return;

            std::vector<CorpseScan::CorpseInfo> corpses = CorpseScan::instance().snapshot();
            if (corpses.empty())
                return;

            Config const& cfg = Setting::instance().get_config();
            bool const pulse_mode = cfg.hotkey_mode == Config::HotkeyMode::e_pulse;
            bool const pulse_active = pulse_mode && cfg.enabled && PulseTimer::instance().active();
            if (pulse_mode && !pulse_active)
                return;

            if (pulse_active || cfg.enabled)
            {
                float const pulse = pulse_active ? pulse_alpha(PulseTimer::instance().progress()) : 1.0f;
                Color color;
                color.decode(cfg.outline_color);
                RE::NiCamera* camera = RE::Main::WorldRootCamera();
                if (!camera)
                    return;

                if (pulse > 0.f)
                {
                    if (cfg.display_mode == Config::DisplayMode::e_icon)
                    {

                        RE::BSGraphics::ViewData const* view_data = update_view_data(camera);

                        m_icon_overlay.begin_frame(width, height);

                        std::vector<Icon::IconCandidate> candidates;
                        candidates.reserve(corpses.size());
                        for (CorpseScan::CorpseInfo const& corpse : corpses)
                        {
                            DirectX::XMFLOAT3 const top = Icon::icon_anchor(render_cast(corpse.bound_min), render_cast(corpse.bound_max));
                            DirectX::XMFLOAT2 tip{};
                            if (!project_icon_tip(camera, view_data, top, static_cast<float>(width), static_cast<float>(height), tip))
                                continue;

                            float const alpha = pulse * corpse_alpha(cfg, corpse.distance, color.a());
                            candidates.emplace_back(corpse.form_id, render_cast(corpse.anchor), tip, corpse.distance, alpha);
                        }
                        std::vector<Icon::IconMarker> const markers = Icon::icon_marker(candidates, static_cast<float>(cfg.icon_radius), cfg.max_distance, Max_Corpse_Count, m_icon_groups);

                        std::vector<Icon::IconVertex> vertices;
                        for (Icon::IconMarker const& marker : std::views::reverse(markers))
                        {
                            Icon::IconGeometry const geometry = Icon::icon_geometry(marker, { color.r(), color.g(), color.b() }, static_cast<float>(width), static_cast<float>(height));
                            if (vertices.size() + geometry.count <= Icon::Icon_Max_Vertex_Count)
                                vertices.insert(vertices.end(), geometry.vertices.begin(), geometry.vertices.begin() + static_cast<int64_t>(geometry.count));
                        }
                        m_icon_overlay.draw(context, output_target, vertices, *m_states);
                        m_icon_overlay.end_frame();

                    }
                    else
                    {
                        if (!m_mask_overlay.begin_frame(device, width, height))
                        {
                            m_mask_overlay.end_frame();
                            return;
                        }

                        // One outer element per drawn corpse. Each snapshot corpse is culled on
                        // anchor/radius (a culled corpse never touches the cache - recency tracks
                        // drawn corpses), then resolved through the render-thread form-id LRU
                        // cache: a hit reuses the cached geometries, a miss collects them this
                        // frame via collect_render_geometries (no scan-interval delay after a
                        // camera turn); empty collections are not cached, so an unresolvable
                        // corpse retries next frame. target_index is rewritten on a LOCAL copy -
                        // the cache is never mutated - and the copy moves into the draw list,
                        // keeping the nested-vector handoff to MaskOverlay::draw unchanged.
                        //
                        // Invalidation: the cache self-manages it - its equip sink queues the
                        // affected corpse ids into its own inbox (the only cross-thread
                        // structure) and drain_invalidations() applies the erases here at
                        // frame head; on every cache hit the corpse's current 3D root is
                        // compared against the root captured at collection time, so a rebuilt 3D
                        // (save load, memory purge, base swap) or gear change invalidates the
                        // entry immediately and the corpse re-collects on the miss path. An
                        // unresolved ref keeps the entry - the corpse has left the snapshot
                        // anyway.
                        RenderGeometryCache::instance().drain_invalidations();

                        std::vector<DirectX::XMFLOAT4> colors;
                        colors.reserve(corpses.size());
                        std::vector<std::vector<RenderGeometry>> render_geometries;
                        render_geometries.reserve(corpses.size());

                        uint32_t visible_index = 0;
                        size_t total_render_geometries = 0;
                        for (CorpseScan::CorpseInfo const& corpse : corpses)
                        {
                            if (visible_index >= Max_Corpse_Count)
                            {
                                logger::warn("Mask overlay: corpse cap {} reached, extra targets not drawn", Max_Corpse_Count);
                                break;
                            }
                            if (camera && !camera->PointInFrustum(corpse.anchor, corpse.radius))
                                continue;

                            // The ref resolves once per corpse so the hit path can validate the
                            // cached root; the miss path needed the ref anyway.
                            RE::TESForm* form = RE::TESForm::LookupByID(corpse.form_id);
                            RE::TESObjectREFR const* ref = form ? form->AsReference() : nullptr;
                            RE::NiAVObject* const current_root = ref ? ref->GetCurrent3D() : nullptr;

                            std::vector<RenderGeometry> corpse_geometries;
                            RenderGeometryCache::Hit const cached = RenderGeometryCache::instance().lookup(corpse.form_id);
                            if (cached.geometries)
                            {
                                // Hit: validate the cached entry's 3D root against the corpse's
                                // current one. A mismatch means the 3D was rebuilt (gear change
                                // without an event, save load, memory purge, base swap) - drop
                                // the entry and take the miss path so this frame re-collects with
                                // the fresh root. An unresolved ref keeps the entry: the corpse
                                // has left the snapshot anyway, so drawing it this frame is
                                // harmless (same behavior as before invalidation existed).
                                if (ref && current_root != cached.root3d)
                                    RenderGeometryCache::instance().erase(corpse.form_id);
                                else
                                    corpse_geometries = *cached.geometries;
                            }

                            // Miss path (a fresh miss, or the hit above was just invalidated):
                            // collect now and cache with the root captured this frame. Empty
                            // collections are not cached (insert rejects them), so an
                            // unresolvable corpse retries next frame.
                            if (corpse_geometries.empty())
                            {
                                if (ref)
                                    collect_render_geometries(ref, device, corpse_geometries);
                                RenderGeometryCache::instance().insert(corpse.form_id, current_root, std::move(corpse_geometries));
                            }
                            if (corpse_geometries.empty())
                                continue;

                            if (total_render_geometries + corpse_geometries.size() > Max_Render_Geometries_Per_Frame)
                            {
                                logger::warn("Mask overlay: render-geometry cap {} reached, extra geometry dropped", Max_Render_Geometries_Per_Frame);
                                break;
                            }
                            total_render_geometries += corpse_geometries.size();

                            for (RenderGeometry& draw : corpse_geometries)
                                draw.target_index = visible_index;
                            colors.emplace_back(color.r(), color.g(), color.b(), pulse * corpse_alpha(cfg, corpse.distance, color.a()));
                            render_geometries.push_back(std::move(corpse_geometries));
                            ++visible_index;
                        }

                        if (!render_geometries.empty())
                            m_mask_overlay.draw(device, context, camera, output_target, width, height, colors, render_geometries, *m_states);
                        m_mask_overlay.end_frame();
                    }
                }
            }
        }

        bool init(REX::W32::ID3D11Device* device)
        {
            if (m_ready)
                return true;

            // All HLSL passes are compiled once, before any overlay picks them up.
            if (!ShaderManager::instance().compile())
                return false;

            // CommonStates is the local REX::W32-typed mirror; it takes the REX device pointer directly.
            m_states = std::make_unique<CommonStates>(device);
            if (!m_states || !m_states->valid() || !m_icon_overlay.init(device) || !m_mask_overlay.init(device))
                return false;

            m_ready = true;
            return m_ready;
        }

    private:
        std::chrono::steady_clock::time_point m_last_scan;
        std::atomic<bool> m_scan_in_flight;

        uint32_t m_last_drawn_frame;

        // Icon-mode grouping state: the corpse→representative assignment of the previous drawn
        // frame (icon_marker reads it for the hysteresis and writes the new assignment back).
        // Only touched on the render thread inside draw().
        std::unordered_map<uint32_t, uint32_t> m_icon_groups;

        std::unique_ptr<CommonStates> m_states;
        Icon::IconOverlay m_icon_overlay;
        Mask::MaskOverlay m_mask_overlay;

        bool m_ready;
    };

    void pre_ui_callback(int64_t)
    {
        OverlayDirector::instance().on_pre_ui_draw();
    }
}

void Renderer::install()
{
    RenderGeometryCache::instance().install();
    (void)UiRenderHook::instance().install(&pre_ui_callback);
}

PLUGIN_NAMESPACE_END
