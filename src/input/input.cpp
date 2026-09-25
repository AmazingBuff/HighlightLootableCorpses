#include "input.h"

#include "config/config.h"
#include "ui/pulse_timer.h"
#include "ui/ui_menu.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // SKSE macro code of a button event: keyboard (DIK), then mouse buttons/wheel and gamepad
    // (SKSE::InputMap offsets). cfg.hotkey is stored in this same space, so the trigger and the
    // MCP rebinding capture compare raw macro codes with no per-device translation.
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

    void button_event(RE::ButtonEvent* event)
    {
        uint32_t key = 0;
        if (!macro_key_code(*event, key))
            return;

        // While the MCP menu is open the rebinding capture consumes key presses (ESC may close
        // the panel, but the sink sees the down event first and the bind still applies).
        if (Menu::is_menu_open())
        {
            Menu::feed_rebind(*event, key);
            return;
        }

        if (uint32_t const hotkey = Setting::instance().get_config().hotkey)
        {
            if (event->IsDown() && key == hotkey)
            {
                Config& cfg = Setting::instance().get_config();
                if (cfg.hotkey_mode == Config::HotkeyMode::e_pulse)
                {
                    if (cfg.enabled)
                        PulseTimer::instance().trigger(cfg.pulse_duration_ms);
                }
                else
                {
                    bool& enabled = cfg.enabled;
                    enabled = !enabled;
                }
            }
        }
    }

    class InputHandler final : public RE::BSTEventSink<RE::InputEvent*>
    {
        InputHandler() = default;
        ~InputHandler() override = default;
    public:
        static InputHandler* instance()
        {
            static InputHandler s_instance;
            return &s_instance;
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* event, RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (*event)
            {
                for (RE::InputEvent* current = *event; current; current = current->next)
                {
                    if (RE::ButtonEvent* button = current->AsButtonEvent())
                        button_event(button);
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };
} // namespace

void InputManager::install()
{
    RE::BSInputDeviceManager::GetSingleton()->AddEventSink(InputHandler::instance());

    logger::info("Input handler added");
}

PLUGIN_NAMESPACE_END
