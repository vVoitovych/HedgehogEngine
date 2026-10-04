#pragma once

#include <cstdint>

namespace HedgehogSettings
{
    // The Lua debugger (epic HE-180), engine_settings.yaml's lua_debugger section. Off by default:
    // then the editor and the game create no debugger at all (no thread, no socket, no hook). On,
    // they listen for VS Code on 127.0.0.1:Port.
    struct LuaDebuggerSettings
    {
        static constexpr uint16_t DEFAULT_PORT = 4711;

        bool     Enabled = false;
        uint16_t Port    = DEFAULT_PORT;
    };
}
