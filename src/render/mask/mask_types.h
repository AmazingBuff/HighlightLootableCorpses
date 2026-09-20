//
// Created by AmazingBuff on 2026/9/13.
//

#pragma once

#define MASK_NAMESPACE_BEGIN PLUGIN_NAMESPACE_BEGIN namespace Mask {
#define MASK_NAMESPACE_END PLUGIN_NAMESPACE_END }

MASK_NAMESPACE_BEGIN

// mask RT clear color
inline constexpr float Mask_Clear_Color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// mask render target (a reference plus a distance-faded opacity, ordered by target number, i.e. the corpse index)
struct MaskTarget
{
    RE::NiPointer<RE::TESObjectREFR> ref;
    DirectX::XMFLOAT4 color;
};

MASK_NAMESPACE_END
