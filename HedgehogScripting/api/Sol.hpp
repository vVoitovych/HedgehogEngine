#pragma once

// The one way to include sol2 in this engine. sol2's configuration macros must be
// identical in every translation unit that sees sol, so they are set here and
// nowhere else.

// Lua is compiled as C (ThirdParty/Lua/Build-Lua.lua), so its headers need extern "C".
#define SOL_USING_CXX_LUA 0

// Debug builds check every argument, return and userdata type. Release still checks the
// arguments of every call into C++, so a script passing a number or table where a Vector3 or
// Quat belongs (Vector3(1, 2, 3) + 5) gets a script error, not a crash.
#if defined(DEBUG)
    #define SOL_ALL_SAFETIES_ON 1
#else
    #define SOL_SAFE_FUNCTION_CALLS 1
    #define SOL_SAFE_USERTYPE 1
#endif

#include "sol/sol.hpp"

#include "HedgehogMath/api/Vector.hpp"

// HM::Vector has begin/end/size, so sol2 would push it as a container (a table-like proxy) and
// ignore the Vector3 usertype. Every translation unit must agree on this, hence it lives here.
template<>
struct sol::is_container<HM::Vector3> : std::false_type
{
};
