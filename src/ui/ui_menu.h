#pragma once

PLUGIN_NAMESPACE_BEGIN

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

    // Hotkey rebinding while the MCP menu is open: render_general toggles the capture, the input
    // sink feeds button events, and feed_rebind stores the pressed key's SKSE macro code in the
    // config and ends the capture. update_rebinding applies the 5s timeout from the menu frame.
    // Mouse presses over the framework panel are not captured (imgui keeps them, so clicking the
    // hotkey button again cancels instead of binding); keyboard and gamepad presses always bind.
    [[nodiscard]] bool is_rebinding() const;
    void toggle_rebinding();
    void update_rebinding();
    void rebind(RE::ButtonEvent const& event, uint32_t macro_key);
private:
    Menu();
    ~Menu();
private:
    bool m_rebinding;
    std::chrono::steady_clock::time_point m_rebind_start;
};

PLUGIN_NAMESPACE_END