#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_PHYSICS_EXPORT
        #define HEDGEHOG_PHYSICS_API __declspec(dllexport)
    #else
        #define HEDGEHOG_PHYSICS_API __declspec(dllimport)
    #endif
#else
    #ifdef HEDGEHOG_PHYSICS_EXPORT
        #define HEDGEHOG_PHYSICS_API __attribute__((visibility("default")))
    #else
        #define HEDGEHOG_PHYSICS_API
    #endif
#endif
