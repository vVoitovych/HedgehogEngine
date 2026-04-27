#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_SETTINGS_EXPORT
        #define HEDGEHOG_SETTINGS_API __declspec(dllexport)
    #else
        #define HEDGEHOG_SETTINGS_API __declspec(dllimport)
    #endif
#else
    #define HEDGEHOG_SETTINGS_API __attribute__((visibility("default")))
#endif
