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

    // Hotkey rebinding while the MCP menu is open: render_general toggles the capture, and the
    // framework's own input hook (on_framework_input, registered via AddInputEvent - the
    // framework freezes engine input while one of its windows is open, so the engine sink never
    // sees these events) feeds it button events. rebind stores the pressed key's SKSE macro code
    // in the config and ends the capture; update_rebinding applies the 5s timeout from the menu
    // frame. Mouse presses over the framework panel are not captured when imgui reports them
    // captured; a short lockout after each capture keeps the same physical click from restarting
    // the capture through the hotkey button's toggle.
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
    std::chrono::steady_clock::time_point m_last_capture;  // restart-lockout stamp (see Rebind_Click_Lockout in ui_menu.cpp)
};

PLUGIN_NAMESPACE_END