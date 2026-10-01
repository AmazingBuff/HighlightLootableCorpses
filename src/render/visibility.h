//
// Created by AmazingBuff on 2026/10/01.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

// Mirrors the engine's own UI-visibility decision surface so the overlay hides
// exactly when the native interface does (the pre-UI hook runs inside
// MenuManager::DrawInterfaceStart, which the engine calls every rendered frame
// regardless of whether any menu content follows).
//
// The engine consults its menu stack and the counting members maintained by UI
// (numPausesGame, numApplicationMenus, menuSystemVisible) to decide whether
// gameplay HUD content is visible. Reading that state happens on the main
// thread; the overlay draws on the render thread, so the verdict is recomputed
// on the main thread whenever a menu opens/closes (MenuOpenCloseEvent) and
// handed over through an atomic the render thread reads - the same handoff
// shape the geometry cache uses for its equip invalidations.
class Visibility
{
public:
    Visibility(Visibility const&) = delete;
    Visibility(Visibility const&&) = delete;
    Visibility operator=(Visibility&) = delete;
    Visibility operator=(Visibility&&) = delete;

    static Visibility& instance();

    void install();

    // True when the native interface would draw gameplay HUD content, i.e. when
    // the overlay may draw. Reads the cached main-thread verdict AND the MCP
    // framework window state live (the framework does not emit
    // MenuOpenCloseEvent, so its windows cannot be part of the cache).
    // Called on the render thread.
    [[nodiscard]] bool overlay_allowed() const;
private:
    class MenuOpenCloseHandler final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(RE::MenuOpenCloseEvent const* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;
    };
private:
    Visibility();
    ~Visibility();

    void refresh();
private:
    std::atomic_bool m_overlay_allowed;
    MenuOpenCloseHandler m_menu_open_close_handler;
};

PLUGIN_NAMESPACE_END
