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

// Render-geometry cap for one frame's draw list: past it the render branch stops appending and
// extra geometry is dropped with a WARN. The render thread draws one corpse's cached list at a
// time, so the branch enforces this as the flat per-frame total across corpses.
inline constexpr size_t Max_Render_Geometries_Per_Frame = 256;


// Weight/index layout inside the skinned vertex buffer (self-calibration result)
struct SkinLayout
{
    REX::W32::DXGI_FORMAT weight_format;
    uint32_t weight_offset;
    REX::W32::DXGI_FORMAT index_format;
    uint32_t index_offset;
};

// One geometry draw (collected and cached on the render thread; VB/IB are borrowed from game
// objects). Lifetime: node keeps the static-path geometry (and its GPU buffers) alive; the skinned
// path keeps its buffData/VB/IB alive through the skin (NiSkinInstance→skinPartition→buffData)
// chain. Cached entries live in the render thread's form-id LRU cache (RenderGeometryCache) and
// are dropped by eviction, releasing the last NiPointer reference in a same-thread refcount
// decrement.
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

    // Index of the target in the caller's target list (collection time). The render thread compacts
    // frustum-surviving corpses into the per-frame visible-target order, rewriting target_index to
    // the outer index on a local copy (the cache's stored list is never mutated), so style indexing
    // and outline grouping stay per-frame correct without any form-id remap.
    uint32_t target_index;

    // Position attribute layout: the static path stores the calibrate_position_format result
    // (UNKNOWN means derive from desc); the skinned path stores the offset-spacing result.
    RE::BSGraphics::VertexDesc vertex_desc;
    REX::W32::DXGI_FORMAT position_format;
    uint32_t position_offset;
    uint32_t partition;
    // Skinning weight/index layout: only skinned geometries store a calibration result; static geometries keep the default values.
    SkinLayout skin_layout;

    // Positionless skinned partitions (partition vertex desc without VF_VERTEX, e.g. FaceGen-family
    // BSDynamicTriShape head parts - head, eyes, hair): the partition buffer carries only
    // UV/normal/tangent/color and the SKINNING block; model-space positions live in
    // BSDynamicTriShape::dynamicData (one float4 per ORIGINAL vertex). At collection time the
    // position stream is rebuilt (identity for whole-mesh index space, vertexMap-remapped for
    // packed subsets) into this dedicated GPU buffer. Corpses are static
    // (dynamicData no longer changes after death), so the stream is uploaded once per collection
    // and bound directly, instead of being re-uploaded through a scratch buffer every frame.
    // Ownership: created by collect_render_geometries on the render thread; owned by the
    // RenderGeometryCache entry and released when that entry is erased or evicted - per-frame
    // copies of RenderGeometry borrow the pointer and must never release it. Null for the
    // standard single-stream paths (slot 0 then carries positions inside vertex_buffer).
    REX::W32::ID3D11Buffer* position_buffer;
    uint32_t position_stride;  // byte stride of one position-stream vertex (float4 = 16 bytes); like position_buffer, meaningful only when it is set
};

// Collect one run's render geometries from the mask targets along two paths, static (the
// BSTriShape family) and skinned (NiSkinPartition partitions), including position-format and
// skin-layout self-calibration, per-mesh validation and the creation of the positionless
// geometry's cached position-stream buffers - geometries with no solution are skipped (better
// to draw too little than to smear garbage over the screen). Runs on the render thread
// (Present path): OverlayDirector's mask branch calls it per cache miss - first appearance of a
// corpse in view pays its traversal and calibration on that frame (one corpse), while the steady
// state is LRU cache hits and performs no scene-graph traversal at all. The order of the target
// list is the target index; the draw-list cap Max_Render_Geometries_Per_Frame applies to the
// per-frame total across corpses in the render branch.
void collect_render_geometries(RE::TESObjectREFR const* ref, REX::W32::ID3D11Device* device, std::vector<RenderGeometry>& render_geometries);

PLUGIN_NAMESPACE_END
