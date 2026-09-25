#pragma once

PLUGIN_NAMESPACE_BEGIN

// SKSE macro code of a button event: keyboard (DIK), then mouse buttons/wheel and gamepad
// (SKSE::InputMap offsets). cfg.hotkey lives in this same space, so the trigger and the MCP
// rebinding capture compare raw macro codes with no per-device translation.
[[nodiscard]] uint32_t macro_key_code(RE::ButtonEvent const& event, uint32_t& out);

class Menu
{
public:

    Menu(Menu const&) = delete;
    Menu(Menu const&&) = delete;
    Menu operator=(Menu&) = delete;
    Menu operator=(Menu&&) = delete;

    static Menu& instance();

    void register_menu();
    [[nodiscard]] bool is_menu_open();

    // Hotkey rebinding while the MCP menu is open: render_general toggles the capture, and the
    // framework's own input hook (on_framework_input, registered via AddInputEvent - the
    // framework freezes engine input while one of its windows is open, so the engine sink never
    // sees these events) feeds it button events. rebind stores the pressed key's SKSE macro code
    // in the config and ends the capture; update_rebinding applies the 5s timeout from the menu
    // frame. Mouse presses over the framework panel are not captured (imgui keeps them, so
    // clicking the hotkey button again cancels instead of binding); keyboard and gamepad presses
    // always bind.
    [[nodiscard]] bool is_rebinding() const;
    void toggle_rebinding();
    void update_rebinding();
    void rebind(RE::ButtonEvent const& event, uint32_t macro_key);
private:
    Menu();
    ~Menu();
private:
    static bool __stdcall on_framework_input(RE::InputEvent* event);

    bool m_rebinding;
    std::chrono::steady_clock::time_point m_rebind_start;
};

PLUGIN_NAMESPACE_END