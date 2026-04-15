#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_COMMON_EXPORT
        #define HEDGEHOG_COMMON_API __declspec(dllexport)
    #else
        #define HEDGEHOG_COMMON_API __declspec(dllimport)
    #endif
#else
    #ifdef HEDGEHOG_COMMON_EXPORT
        #define HEDGEHOG_COMMON_API __attribute__((visibility("default")))
    #else
        #define HEDGEHOG_COMMON_API
    #endif
#endif
