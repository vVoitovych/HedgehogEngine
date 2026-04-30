#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_ENGINE_EXPORT
        #define HEDGEHOG_ENGINE_API __declspec(dllexport)
    #else
        #define HEDGEHOG_ENGINE_API __declspec(dllimport)
    #endif
#else
    #define HEDGEHOG_ENGINE_API __attribute__((visibility("default")))
#endif
