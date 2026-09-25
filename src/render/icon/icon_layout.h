//
// Created by AmazingBuff on 2026/09/17.
//

#pragma once

#include "icon_types.h"

ICON_NAMESPACE_BEGIN

[[nodiscard]] DirectX::XMFLOAT3 icon_anchor(DirectX::XMFLOAT3 const& minimum, DirectX::XMFLOAT3 const& maximum);
[[nodiscard]] float icon_radius(float base_radius, float distance, float max_distance, bool clustered);
[[nodiscard]] bool icon_screen_tip(float px, float py, DirectX::XMFLOAT2& tip);
[[nodiscard]] bool icon_clip_tip(DirectX::XMFLOAT4 const& clip, float width, float height, DirectX::XMFLOAT2& tip);
// previous_groups carries the corpse→representative assignment of the previous frame (owned by
// the caller, the render thread): it drives the grouping hysteresis (the Exit_Factor slack and
// the representative preference) and is replaced with this frame's assignment on return. Without
// a persisted map the grouping re-derives from scratch every frame and markers jump whenever a
// camera move changes the screen-space distances.
[[nodiscard]] std::vector<IconMarker> icon_marker(std::span<IconCandidate const> candidates, float base_radius, float max_distance, size_t marker_limit, std::unordered_map<uint32_t, uint32_t>& previous_groups);
[[nodiscard]] IconGeometry icon_geometry(IconMarker const& marker, DirectX::XMFLOAT3 const& color, float width, float height);

ICON_NAMESPACE_END