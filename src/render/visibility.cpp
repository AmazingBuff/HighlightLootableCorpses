//
// Created by AmazingBuff on 2026/10/01.
//

#include "visibility.h"

#include "config/config.h"
#include "ui/ui_menu.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // A pushed-on, non-always-open menu blocks gameplay HUD content (inventory,
    // container, map, tween, journal, console, mod menus, ...). The always-open
    // menus the HUD itself is made of (HUD Menu, Cursor Menu, Console, Loading
    // Menu) are exempt - the engine keeps them visible under other menus for
    // the same reason. Same rule QuickLoot IE applies to decide whether its
    // loot list may show.
    bool is_blocking_menu_open(RE::UI& ui)
    {
        for (RE::GPtr<RE::IMenu> const& menu : ui.menuStack)
        {
            if (menu && menu->OnStack() && !menu->AlwaysOpen())
                return true;
        }
        return false;
    }

    // Contexts under which a paused / HUD-blocking interface still shows the
    // world and the HUD: the favorites panel (kPausesGame, world stays on
    // screen behind it) and the console (transparent overlay). kGameplay is
    // deliberately NOT here: kNone-context menus (loading screens, some mod
    // menus) never push the input stack, leaving kGameplay on top while they
    // pause or hide the HUD - only the UI counters below catch those. TrueHUD
    // classifies the same way; its kGameplay branch is the "nothing else open"
    // case, covered by the !paused && !hud_blocked path below.
    bool is_world_visible_context(RE::ControlMap const& control_map)
    {
        RE::BSTArray<RE::UserEvents::INPUT_CONTEXT_ID> const& priority_stack = control_map.GetRuntimeData().contextPriorityStack;
        if (priority_stack.empty())
            return false;

        switch (priority_stack.back())
        {
        case RE::UserEvents::INPUT_CONTEXT_ID::kFavorites:
        case RE::UserEvents::INPUT_CONTEXT_ID::kConsole:
            return true;
        default:
            return false;
        }
    }

    bool evaluate()
    {
        RE::UI* ui = RE::UI::GetSingleton();
        if (!ui)
            return false;

        // kNone-context full-screen states never push the input context stack
        // (the stack top stays kGameplay during a loading screen), so only the
        // UI counters can catch them: the menu-system master switch and the
        // application-menu counter (main menu, loading screens).
        if (!ui->IsShowingMenus())
            return false;

        if (ui->IsApplicationMenuOpen())
            return false;

        bool const paused = ui->GameIsPaused();
        bool const hud_blocked = is_blocking_menu_open(*ui);
        if (!paused && !hud_blocked)
            return true;  // live gameplay, or kNone overlays that behave like part of the HUD

        // A paused or HUD-blocking menu is up; only favorites and console keep
        // the world and HUD visible behind them.
        RE::ControlMap const* control_map = RE::ControlMap::GetSingleton();
        return control_map && is_world_visible_context(*control_map);
    }
}

RE::BSEventNotifyControl Visibility::MenuOpenCloseHandler::ProcessEvent(RE::MenuOpenCloseEvent const* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
    if (event)
        instance().refresh();
    return RE::BSEventNotifyControl::kContinue;
}

Visibility& Visibility::instance()
{
    static Visibility s_instance;
    return s_instance;
}

void Visibility::install()
{
    RE::UI::GetSingleton()->AddEventSink(&m_menu_open_close_handler);
    logger::info("Registered menu-open-close sink for visibility"sv);
}

bool Visibility::overlay_allowed() const
{
    return m_overlay_allowed.load(std::memory_order_relaxed);
}

Visibility::Visibility() : m_overlay_allowed(false) {}

Visibility::~Visibility() {}

void Visibility::refresh()
{
    m_overlay_allowed.store(evaluate(), std::memory_order_relaxed);
}



PLUGIN_NAMESPACE_END
