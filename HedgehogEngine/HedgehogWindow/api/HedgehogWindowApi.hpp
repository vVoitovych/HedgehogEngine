#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_WINDOW_EXPORT
        #define HEDGEHOG_WINDOW_API __declspec(dllexport)
    #else
        #define HEDGEHOG_WINDOW_API __declspec(dllimport)
    #endif
#else
    #ifdef HEDGEHOG_WINDOW_EXPORT
        #define HEDGEHOG_WINDOW_API __attribute__((visibility("default")))
    #else
        #define HEDGEHOG_WINDOW_API
    #endif
#endif
