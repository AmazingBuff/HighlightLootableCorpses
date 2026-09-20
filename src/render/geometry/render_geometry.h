//
// Created by AmazingBuff on 2026/9/13.
//

#pragma once

#include "render/mask/mask_types.h"

PLUGIN_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Skinned palette constant-buffer budget (matrices per draw); 128×64B = 8KB.
// ---------------------------------------------------------------------------
inline constexpr size_t Max_Palette_Bones = 128;
inline constexpr size_t Palette_CB_Bytes = Max_Palette_Bones * 64;
static_assert(Palette_CB_Bytes == Max_Palette_Bones * sizeof(float) * 16, "palette CB layout must be float4x4 slots");


// Weight/index layout inside the skinned vertex buffer (self-calibration result)
struct SkinLayout
{
    REX::W32::DXGI_FORMAT weight_format;
    uint32_t weight_offset;
    REX::W32::DXGI_FORMAT index_format;
    uint32_t index_offset;
};

// One geometry draw (a temporary render-thread list; VB/IB are borrowed from game objects).
// Lifetime: node_ref keeps the static-path geometry (and its GPU buffers) alive; the skinned path
// keeps its buffData/VB/IB alive through the skin (NiSkinInstance→skinPartition→buffData) chain.
struct RenderGeometry
{
    REX::W32::ID3D11Buffer* vertex_buffer;
    REX::W32::ID3D11Buffer* index_buffer;
    uint32_t vertex_stride;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t index_count;

    RE::NiPointer<RE::NiSkinInstance> skin;  // keeps the skin instance alive (bone world matrices)
    RE::NiPointer<RE::BSGeometry> node;  // keeps the static-path geometry (and its GPU buffers) alive

    uint32_t target_index;

    // Position attribute layout: the static path stores the calibrate_position_format result
    // (UNKNOWN means derive from desc); the skinned path stores the offset-spacing result.
    RE::BSGraphics::VertexDesc vertex_desc;
    REX::W32::DXGI_FORMAT position_format;
    uint32_t position_offset;
    uint32_t partition;
    // Skinning weight/index layout: only skinned draws store a calibration result; static draws keep the default values.
    SkinLayout skin_layout;
};

// Collect this frame's geometry draws from the mask targets along two paths, static (the
// BSTriShape family) and skinned (NiSkinPartition partitions), including position-format and
// skin-layout self-calibration and per-mesh validation - draws with no solution are skipped
// (better to draw too little than to smear garbage over the screen). The order of the target list
// is the corpse index, and a target beyond the slots is clamped to the last index.
void collect_render_geometries(std::vector<RE::TESObjectREFR*> const& targets, std::vector<RenderGeometry>& draws);

PLUGIN_NAMESPACE_END
