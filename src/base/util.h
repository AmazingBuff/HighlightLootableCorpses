//
// Created by AmazingBuff on 2026/9/10.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

namespace Util
{
    [[nodiscard]] RE::TESObjectREFR* get_container_object(RE::TESObjectREFR* ref);

    [[nodiscard]] bool is_corpse_actor(RE::Actor* actor);
    [[nodiscard]] bool is_ash_pile(RE::TESObjectREFR const* ref);
    [[nodiscard]] bool is_corpse_object(RE::TESObjectREFR const* ref);
    [[nodiscard]] bool is_corpse(RE::TESObjectREFR* ref);

    // SKSE macro code of a button event: keyboard (DIK), then mouse buttons/wheel and gamepad
    // (SKSE::InputMap offsets). cfg.hotkey lives in this same space, so the trigger and the MCP
    // rebinding capture compare raw macro codes with no per-device translation.
    [[nodiscard]] uint32_t macro_key_code(RE::ButtonEvent const& event, uint32_t& out);
}
PLUGIN_NAMESPACE_END