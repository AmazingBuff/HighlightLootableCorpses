//
// Created by AmazingBuff on 2026/9/28.
//

#pragma once

#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

class UiRenderHook
{
public:
    using Callback = void (*)(int64_t);

    static UiRenderHook& instance();

    bool install(Callback callback);
private:
    UiRenderHook();
    ~UiRenderHook();

    using DrawInterfaceFunc = void (*)(int64_t);

    static void draw_interface_thunk(int64_t argument);

    DrawInterfaceFunc m_ref_original;
    Callback m_callback;
};

PLUGIN_NAMESPACE_END
