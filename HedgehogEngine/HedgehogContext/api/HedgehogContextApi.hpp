#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_CONTEXT_EXPORT
        #define HEDGEHOG_CONTEXT_API __declspec(dllexport)
    #else
        #define HEDGEHOG_CONTEXT_API __declspec(dllimport)
    #endif
#else
    #define HEDGEHOG_CONTEXT_API __attribute__((visibility("default")))
#endif
