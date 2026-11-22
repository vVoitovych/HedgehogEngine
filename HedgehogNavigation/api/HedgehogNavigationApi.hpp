#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_NAVIGATION_EXPORT
        #define HEDGEHOG_NAVIGATION_API __declspec(dllexport)
    #else
        #define HEDGEHOG_NAVIGATION_API __declspec(dllimport)
    #endif
#else
    #ifdef HEDGEHOG_NAVIGATION_EXPORT
        #define HEDGEHOG_NAVIGATION_API __attribute__((visibility("default")))
    #else
        #define HEDGEHOG_NAVIGATION_API
    #endif
#endif
