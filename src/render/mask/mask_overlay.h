//
// Created by AmazingBuff on 2026/9/13.
//

#pragma once

#include "mask_passes.h"
#include "mask_types.h"

MASK_NAMESPACE_BEGIN

// Render-thread-only facade. Targets are retained by NiPointer. Silhouettes use
// private depth; outlines merge independent whole-target masks. Both ignore scene depth.
class MaskOverlay
{
public:
    MaskOverlay();
    ~MaskOverlay();

    bool init(REX::W32::ID3D11Device* device);
    bool begin_frame(REX::W32::ID3D11Device* device, uint32_t width, uint32_t height);


    // Called from the Present callback: render the mask for the pre-collected geometry draws and
    // blend the fill/outline onto overlay_target according to the current display_mode
    // - the caller must pass the RTV of the back buffer Present will show (the contract matches the
    // icon path and does not rely on whichever render target happens to be bound at the engine's
    // Present moment).
    // width/height are the back-buffer dimensions (the mask RT has the same size and is
    // recreated when they change).
    // draws is the geometry cache snapshot for this frame: collected by the SKSE scan task and
    // frustum-culled/compacted by the caller (target_index is the compacted visible-entry index,
    // matching the slot order of colors); this function never walks the scene graph - it reads
    // only per-frame state (world transforms, skin palettes, worldBound) and issues the draws.
    // colors is the matching per-target style table (same order as the compacted entries);
    // an empty list draws nothing.
    void draw(
        REX::W32::ID3D11Device* device,
        REX::W32::ID3D11DeviceContext* context,
        RE::NiCamera* camera,
        REX::W32::ID3D11RenderTargetView* overlay_target,
        uint32_t width,
        uint32_t height,
        std::vector<DirectX::XMFLOAT4> const& colors,
        std::vector<RenderGeometry> const& draws,
        CommonStates const& states);

    void end_frame();
private:
    void draw_silhouette(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
        REX::W32::ID3D11RenderTargetView* overlay_target, std::span<RenderGeometry const> group,
        DirectX::XMFLOAT4X4 const& view_proj, REX::W32::D3D11_VIEWPORT const& viewport, CommonStates const& states);
    void draw_outline(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
    REX::W32::ID3D11RenderTargetView* overlay_target, std::vector<RenderGeometry> const& draws,
    DirectX::XMFLOAT4X4 const& view_proj, REX::W32::D3D11_VIEWPORT const& vp, CommonStates const& states);
private:
    // Facade state (owned exclusively by the render thread)
    REX::W32::ID3D11Device* m_ref_device;
    RenderTarget m_mask_rt;
    MaskGeometryPass m_geometry_pass;
    SilhouettePass m_silhouette_pass;
    OutlinePass m_outline_pass;

    uint32_t m_width;
    uint32_t m_height;

    bool m_pass_ready;
    bool m_rt_ready;
};

MASK_NAMESPACE_END
