//
// Created by AmazingBuff on 2026/9/28.
//

#include "ui_render_hook.h"

#include <detours/detours.h>

PLUGIN_NAMESPACE_BEGIN

UiRenderHook& UiRenderHook::instance()
{
    static UiRenderHook s_instance;
    return s_instance;
}

UiRenderHook::UiRenderHook() : m_ref_original(nullptr), m_callback(nullptr) {}

UiRenderHook::~UiRenderHook() {}

bool UiRenderHook::install(Callback callback)
{
    if (!callback)
    {
        logger::error("Failed to install pre-UI renderer hook at callback ({})", ERROR_INVALID_PARAMETER);
        return false;
    }

    static constexpr REL::RelocationID Relocation_Id(79947, 82084);
    std::uintptr_t const target_address = Relocation_Id.address();
    if (!target_address)
    {
        logger::error("Failed to install pre-UI renderer hook at target lookup ({})", ERROR_PROC_NOT_FOUND);
        return false;
    }

    m_ref_original = reinterpret_cast<DrawInterfaceFunc>(target_address);
    m_callback = callback;

    LONG result = DetourTransactionBegin();
    if (result != NO_ERROR)
    {
        logger::error("Failed to install pre-UI renderer hook at transaction begin ({})", result);
        return false;
    }

    result = DetourUpdateThread(GetCurrentThread());
    if (result == NO_ERROR)
        result = DetourAttach(reinterpret_cast<PVOID*>(&m_ref_original), reinterpret_cast<PVOID>(&draw_interface_thunk));

    if (result != NO_ERROR)
    {
        DetourTransactionAbort();
        m_ref_original = nullptr;
        m_callback = nullptr;

        logger::error("Failed to install pre-UI renderer hook at transaction attach ({})", result);
        return false;
    }

    result = DetourTransactionCommit();
    if (result != NO_ERROR)
    {
        m_ref_original = nullptr;
        m_callback = nullptr;

        logger::error("Failed to install pre-UI renderer hook at transaction commit ({})", result);
        return false;
    }

    logger::info("Installed pre-UI renderer hook");
    return true;
}

void UiRenderHook::draw_interface_thunk(int64_t argument)
{
    const UiRenderHook& hook = instance();
    if (hook.m_callback)
        hook.m_callback(argument);
    hook.m_ref_original(argument);
}

PLUGIN_NAMESPACE_END
