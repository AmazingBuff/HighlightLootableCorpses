#pragma once

PLUGIN_NAMESPACE_BEGIN

class Menu
{
public:
    Menu() = delete;
    ~Menu() = delete;
    Menu(Menu const&) = delete;
    Menu(Menu const&&) = delete;
    Menu operator=(Menu&) = delete;
    Menu operator=(Menu&&) = delete;

    static void register_menu();
    [[nodiscard]] static bool is_menu_open();

    // Hotkey rebinding while the MCP menu is open: render_general toggles the capture, the input
    // sink feeds button events, and feed_rebind stores the pressed key's SKSE macro code in the
    // config and ends the capture. update_rebinding applies the 5s timeout from the menu frame.
    // Mouse presses over the framework panel are not captured (imgui keeps them, so clicking the
    // hotkey button again cancels instead of binding); keyboard and gamepad presses always bind.
    [[nodiscard]] static bool is_rebinding();
    static void toggle_rebinding();
    static void update_rebinding();
    static void feed_rebind(RE::ButtonEvent const& event, uint32_t macro_key);
};

PLUGIN_NAMESPACE_END