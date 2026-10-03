#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_AUDIO_EXPORT
        #define HEDGEHOG_AUDIO_API __declspec(dllexport)
    #else
        #define HEDGEHOG_AUDIO_API __declspec(dllimport)
    #endif
#else
    #ifdef HEDGEHOG_AUDIO_EXPORT
        #define HEDGEHOG_AUDIO_API __attribute__((visibility("default")))
    #else
        #define HEDGEHOG_AUDIO_API
    #endif
#endif
