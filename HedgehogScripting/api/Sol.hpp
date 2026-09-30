#pragma once

// The one way to include sol2 in this engine. sol2's configuration macros must be
// identical in every translation unit that sees sol, so they are set here and
// nowhere else.

// Lua is compiled as C (ThirdParty/Lua/Build-Lua.lua), so its headers need extern "C".
#define SOL_USING_CXX_LUA 0

// Debug builds check every argument, return and userdata type. Release still checks the
// arguments of every call into C++ and every usertype access, so a script passing the wrong
// thing gets a script error, not a crash.
#if defined(DEBUG)
    #define SOL_ALL_SAFETIES_ON 1
#else
    #define SOL_SAFE_FUNCTION_CALLS 1
    #define SOL_SAFE_USERTYPE 1
#endif

#include "sol/sol.hpp"
