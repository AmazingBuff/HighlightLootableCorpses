#include "ui_menu.h"

#include "pulse_timer.h"

#include "config/config.h"
#include "render/render_util.h"
#include "search/corpse_finder.h"

#pragma warning(push)
#pragma warning(disable: 4996 5054 4099 4267 4244 4061 4062)
#include <SKSEMCP/utils.hpp>
#pragma warning(pop)

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Hotkey rebinding state: active while the General page shows "Press any key...". The
    // capture ends on the first accepted key press (Menu::feed_rebind) or after the timeout.
    constexpr std::chrono::milliseconds Rebind_Timeout{5000};


    // Display name of a stored hotkey: cfg.hotkey lives in the SKSE macro code space (keyboard
    // DIK, then mouse buttons/wheel and gamepad), which GetKeyName dispatches by range.
    std::string hotkey_name(uint32_t key)
    {
        if (key == 0)
            return "None";
        return SKSE::InputMap::GetKeyName(key);
    }

    // MCP menu callbacks: run on the game's main thread (the framework calls them inside an imgui
    // frame) and read/write the Config settings directly; a change takes effect immediately and
    // the render thread reads it without a lock (the same pattern as set_enabled). The pages
    // mirror the INI sections (General / Display / LootFilter), plus a Stat page with the runtime
    // status and the Save action.

    constexpr Config::HotkeyMode s_hotkey_modes[] = {
        Config::HotkeyMode::e_constant,
        Config::HotkeyMode::e_pulse,
    };
    constexpr char const* s_hotkey_mode_names[] = { "Constant", "Pulse" };

    constexpr Config::DisplayMode s_display_modes[] = {
        Config::DisplayMode::e_silhouette,
        Config::DisplayMode::e_outline,
        Config::DisplayMode::e_icon,
    };
    constexpr char const* s_display_mode_names[] = { "Silhouette", "Outline", "Icon" };

    // INI section [General]
    void render_general()
    {
        Config& cfg = Setting::instance().get_config();

        ImGuiMCP::Checkbox("Enabled", &cfg.enabled);

        Menu::instance().update_rebinding();
        std::string const label = Menu::instance().is_rebinding() ? std::string("Press any key...") : fmt::format("Hotkey: {}", hotkey_name(cfg.hotkey));
        if (ImGuiMCP::Button(label.c_str()))
            Menu::instance().toggle_rebinding();

        size_t hk_index = static_cast<size_t>(cfg.hotkey_mode);
        if (ImGuiMCP::Button(fmt::format("Hotkey Mode: {}", s_hotkey_mode_names[hk_index]).c_str()))
        {
            hk_index = (hk_index + 1) % std::size(s_hotkey_modes);
            Config::HotkeyMode const previous = cfg.hotkey_mode;
            cfg.hotkey_mode = s_hotkey_modes[hk_index];
            if (previous == Config::HotkeyMode::e_constant && cfg.hotkey_mode == Config::HotkeyMode::e_pulse && cfg.enabled)
                PulseTimer::instance().trigger(cfg.pulse_duration_ms);
            else if (previous == Config::HotkeyMode::e_pulse && cfg.hotkey_mode == Config::HotkeyMode::e_constant)
                PulseTimer::instance().reset();
        }

        if (cfg.hotkey_mode == Config::HotkeyMode::e_pulse)
            ImGuiMCP::SliderInt("Pulse Duration (ms)", &cfg.pulse_duration_ms, Setting::Min_Pulse_Duration_Ms, Setting::Max_Pulse_Duration_Ms);

        ImGuiMCP::SliderInt("Scan Interval (ms)", &cfg.scan_interval_ms, Setting::Min_Scan_Interval, Setting::Max_Scan_Interval);
    }

    // INI section [Display]
    void render_display()
    {
        Config& cfg = Setting::instance().get_config();

        size_t mode_index = static_cast<size_t>(cfg.display_mode);
        if (ImGuiMCP::Button(fmt::format("Display Mode: {}", s_display_mode_names[mode_index]).c_str()))
        {
            mode_index = (mode_index + 1) % std::size(s_display_modes);
            cfg.display_mode = s_display_modes[mode_index];
        }
        if (cfg.display_mode == Config::DisplayMode::e_outline)
            ImGuiMCP::SliderInt("Outline Glow Size", &cfg.outline_thickness, Setting::Min_Outline_Thickness, Setting::Max_Outline_Thickness);
        else if (cfg.display_mode == Config::DisplayMode::e_icon)
            ImGuiMCP::SliderInt("Icon Size", &cfg.icon_radius, Setting::Min_Icon_Radius, Setting::Max_Icon_Radius);

        Color rgb;
        rgb.decode(cfg.outline_color);
        if (ImGuiMCP::ColorEdit4("Outline Color", reinterpret_cast<float*>(&rgb)))
            cfg.outline_color = rgb.encode();

        ImGuiMCP::SliderFloat("Min Opacity", &cfg.min_opacity, 0.0f, 1.0f, "%.2f");
        ImGuiMCP::SliderFloat("Max Search Distance", &cfg.max_distance, Setting::Min_Max_Distance, Setting::Max_Max_Distance, "%.0f");
        ImGuiMCP::SliderFloat("Fade Start Distance", &cfg.fade_start_distance, 0.0f, cfg.max_distance, "%.0f");
        ImGuiMCP::SliderFloat("Fade Power", &cfg.fade_power, Setting::Min_Fade_Power, Setting::Max_Fade_Power, "%.1f");
    }

    // INI section [LootFilter]
    void render_loot_filter()
    {
        Config& cfg = Setting::instance().get_config();

        ImGuiMCP::Checkbox("Hide Searched Corpses", &cfg.hide_searched_enabled);

        ImGuiMCP::Checkbox("Enable Value Filter", &cfg.value_filter_enabled);
        ImGuiMCP::BeginDisabled(!cfg.value_filter_enabled);
        ImGuiMCP::Checkbox("Quest Items", &cfg.value_quest_items);
        ImGuiMCP::Checkbox("Keys", &cfg.value_keys);
        ImGuiMCP::Checkbox("Enchanted Gear", &cfg.value_enchanted);
        ImGuiMCP::Checkbox("High-Value Items", &cfg.value_high_value);
        ImGuiMCP::SliderInt("High Value Threshold", &cfg.high_value_threshold, Setting::Min_High_Value_Threshold, Setting::Max_High_Value_Threshold);

        static constexpr char const* s_book_modes[] = { "Spell Books", "Skill Books", "Unread Books" };
        bool book_mode[] = {
            static_cast<bool>(cfg.book_filter_mode & Config::BookType::e_spell),
            static_cast<bool>(cfg.book_filter_mode & Config::BookType::e_skill),
            static_cast<bool>(cfg.book_filter_mode & Config::BookType::e_not_read)
        };

        ImGuiMCP::Checkbox(s_book_modes[0], &book_mode[0]);
        ImGuiMCP::SameLine();
        ImGuiMCP::Checkbox(s_book_modes[1], &book_mode[1]);
        ImGuiMCP::SameLine();
        ImGuiMCP::Checkbox(s_book_modes[2], &book_mode[2]);

        cfg.book_filter_mode = Config::BookType::e_none;
        if (book_mode[0])
            cfg.book_filter_mode |= Config::BookType::e_spell;
        if (book_mode[1])
            cfg.book_filter_mode |= Config::BookType::e_skill;
        if (book_mode[2])
            cfg.book_filter_mode |= Config::BookType::e_not_read;


        ImGuiMCP::Checkbox("Consumables", &cfg.value_consumables);
        ImGuiMCP::EndDisabled();
    }

    // Runtime status and persistence (no editable settings)
    void render_stat()
    {
        Config const& cfg = Setting::instance().get_config();

        ImGuiMCP::Text("Enabled: %s", cfg.enabled ? "yes" : "no");
        ImGuiMCP::Text("Display Mode: %s", s_display_mode_names[static_cast<size_t>(cfg.display_mode)]);
        ImGuiMCP::Text("Hotkey Mode: %s", s_hotkey_mode_names[static_cast<size_t>(cfg.hotkey_mode)]);
        ImGuiMCP::Text("Hotkey: %s", hotkey_name(cfg.hotkey).c_str());

        ImGuiMCP::Separator();

        std::vector<CorpseScan::CorpseInfo> const corpses = CorpseScan::instance().snapshot();
        float nearest = 0.0f;
        for (auto const& corpse : corpses)
            nearest = nearest == 0.0f ? corpse.distance : std::min(nearest, corpse.distance);

        ImGuiMCP::Text("Corpses: %d | Nearest: %.0f units", static_cast<int>(corpses.size()), nearest);

        ImGuiMCP::Separator();

        if (ImGuiMCP::Button("Save"))
            Setting::instance().save();
    }
} // namespace

Menu &Menu::instance()
{
    static Menu s_instance;
    return s_instance;
}

bool Menu::is_menu_open()
{
    return SKSEMenuFramework::IsInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened();
}

uint32_t macro_key_code(RE::ButtonEvent const& event, uint32_t& out)
{
    switch (event.device.get())
    {
    case RE::INPUT_DEVICE::kKeyboard:
        out = event.idCode;
        return true;
    case RE::INPUT_DEVICE::kMouse:
        out = SKSE::InputMap::kMacro_MouseButtonOffset + event.idCode;
        return true;
    case RE::INPUT_DEVICE::kGamepad:
        out = SKSE::InputMap::kMacro_GamepadOffset + SKSE::InputMap::GamepadMaskToKeycode(event.idCode);
        return true;
    default:
        return false;
    }
}

bool Menu::is_rebinding() const
{
    return m_rebinding;
}

void Menu::toggle_rebinding()
{
    m_rebinding = !m_rebinding;
    m_rebind_start = std::chrono::steady_clock::now();
}

void Menu::update_rebinding()
{
    if (m_rebinding && std::chrono::steady_clock::now() - m_rebind_start > Rebind_Timeout)
        m_rebinding = false;
}

void Menu::rebind(RE::ButtonEvent const& event, uint32_t macro_key)
{
    if (!m_rebinding || !event.IsDown() || macro_key == 0)
        return;

    Setting::instance().get_config().hotkey = macro_key;
    m_rebinding = false;
}

bool __stdcall Menu::on_framework_input(RE::InputEvent* event)
{
    // The framework freezes engine input while one of its windows is open, so the capture is fed
    // through this hook instead of the engine sink. While the capture is active every key press
    // is consumed (ESC included - it binds instead of closing the panel); anything else passes
    // through to the framework untouched.
    Menu& menu = instance();
    if (!menu.is_rebinding() || !event)
        return false;

    if (RE::ButtonEvent const* const button = event->AsButtonEvent())
    {
        uint32_t key = 0;
        if (macro_key_code(*button, key))
        {
            menu.rebind(*button, key);
            return true;
        }
    }
    return false;
}

void Menu::register_menu()
{
    static bool s_registered = false;
    if (s_registered)
        return;

    if (!SKSEMenuFramework::IsInstalled())
    {
        logger::warn("SKSE Menu Framework (SKSEMenuFramework.dll) not installed, in-game settings menu disabled");
        return;
    }

    SKSEMenuFramework::SetSection("Highlight Lootable Corpses");
    // The pages mirror the INI sections; Stat carries the runtime status and the Save action.
    SKSEMenuFramework::AddSectionItem("General", render_general);
    SKSEMenuFramework::AddSectionItem("Display", render_display);
    SKSEMenuFramework::AddSectionItem("LootFilter", render_loot_filter);
    SKSEMenuFramework::AddSectionItem("Stat", render_stat);
    // The framework freezes engine input while one of its windows is open - the rebinding
    // capture is fed through the framework's own input hook (the returned registration handle
    // is intentionally kept for the plugin's lifetime, like every other registration here).
    SKSEMenuFramework::AddInputEvent(&Menu::on_framework_input);
    s_registered = true;

    logger::info("Registered settings pages (General, Display, LootFilter, Stat; SKSE Menu Framework v{:.2f})",
        SKSEMenuFramework::GetMenuFrameworkVersion());
}

Menu::Menu() : m_rebinding(false) {}

Menu::~Menu() = default;

PLUGIN_NAMESPACE_END
