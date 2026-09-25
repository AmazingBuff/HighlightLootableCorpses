#include "input.h"

#include "base/util.h"
#include "config/config.h"
#include "ui/pulse_timer.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    void button_event(RE::ButtonEvent* event)
    {
        uint32_t key = 0;
        if (!Util::macro_key_code(*event, key))
            return;

        // While an MCP window is open the framework freezes engine input, so this sink receives
        // nothing and the rebinding capture is fed through the framework's own input hook
        // (Menu::on_framework_input) instead - there is nothing to do here behind that branch.

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
