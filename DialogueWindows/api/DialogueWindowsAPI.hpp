#pragma once

#ifdef _WIN32
    #ifdef DIALOGUE_WINDOWS_EXPORT
        #define DIALOGUE_WINDOWS_API __declspec(dllexport)
    #else
        #define DIALOGUE_WINDOWS_API __declspec(dllimport)
    #endif
#else
    #ifdef DIALOGUE_WINDOWS_EXPORT
        #define DIALOGUE_WINDOWS_API __attribute__((visibility("default")))
    #else
        #define DIALOGUE_WINDOWS_API
    #endif
#endif
