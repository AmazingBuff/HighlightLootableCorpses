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

// Render-geometry cap for one collection run: past it the traversal stops and extra geometry is
// dropped with a WARN. CorpseScan::search() collects one corpse per call and passes its remaining
// cross-corpse budget (down from Max_Render_Geometries_Per_Frame) so the per-call cap enforces the
// global one.
inline constexpr size_t Max_Render_Geometries_Per_Frame = 256;


// Weight/index layout inside the skinned vertex buffer (self-calibration result)
struct SkinLayout
{
    REX::W32::DXGI_FORMAT weight_format;
    uint32_t weight_offset;
    REX::W32::DXGI_FORMAT index_format;
    uint32_t index_offset;
};

// One geometry draw (cached by the scan task, consumed by the render thread; VB/IB are borrowed
// from game objects). Lifetime: node keeps the static-path geometry (and its GPU buffers) alive;
// the skinned path keeps its buffData/VB/IB alive through the skin (NiSkinInstance→skinPartition→
// buffData) chain. NiPointer refcounts are atomic, so the cached entry stays valid across the
// scan→draw gap of up to one scan interval; a 3D unload between scan and draw cannot invalidate
// the VB/IB pointers (the render thread finishes consuming its copy within the frame it took it).
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

    // Index of the target in the caller's target list (collection time). The scan-side cache
    // keeps the per-corpse slot index (0-based within one corpse); the render thread compacts
    // frustum-surviving corpses into the per-frame visible-target order, rewriting target_index
    // to the outer index on its local snapshot copy, so style indexing and outline grouping stay
    // per-frame correct without any form-id remap.
    uint32_t target_index;

    // Position attribute layout: the static path stores the calibrate_position_format result
    // (UNKNOWN means derive from desc); the skinned path stores the offset-spacing result.
    RE::BSGraphics::VertexDesc vertex_desc;
    REX::W32::DXGI_FORMAT position_format;
    uint32_t position_offset;
    uint32_t partition;
    // Skinning weight/index layout: only skinned geometries store a calibration result; static geometries keep the default values.
    SkinLayout skin_layout;
};

// Collect one run's render geometries from the mask targets along two paths, static (the
// BSTriShape family) and skinned (NiSkinPartition partitions), including position-format and
// skin-layout self-calibration and per-mesh validation - geometries with no solution are skipped
// (better to draw too little than to smear garbage over the screen). Runs on the SKSE main-thread
// scan task (scene-graph reads are serialized with the engine there, same as CorpseScan::search);
// it must not be called from the Present thread. The order of the target list is the target
// index; after the render-geometry cap the traversal stops (extra geometry dropped, WARN). CorpseScan
// calls it once per corpse with a single-element target list and a shrinking budget, so the
// per-call cap doubles as the cross-corpse budget; the cap clamps to Max_Render_Geometries_Per_Frame.
void collect_render_geometries(RE::TESObjectREFR const* ref, std::vector<RenderGeometry>& render_geometries);

PLUGIN_NAMESPACE_END
