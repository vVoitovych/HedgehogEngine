#pragma once

// Thin wrapper over Tracy so instrumented code compiles in every configuration.
// TRACY_ENABLE is defined in Release only (see Build-HedgehogRenderer.lua);
// with TRACY_ON_DEMAND the client stays dormant until a Tracy server connects.

#ifdef TRACY_ENABLE

#include <tracy/Tracy.hpp>

#define HH_PROFILE_ZONE(name) ZoneScopedN(name)
#define HH_PROFILE_FRAME()    FrameMark

#else

#define HH_PROFILE_ZONE(name)
#define HH_PROFILE_FRAME()

#endif
