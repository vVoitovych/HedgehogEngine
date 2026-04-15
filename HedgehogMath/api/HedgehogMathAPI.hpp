#pragma once

#ifdef _WIN32
    #ifdef HEDGEHOG_MATH_EXPORTS
        #define HEDGEHOG_MATH_API __declspec(dllexport)
    #else
        #define HEDGEHOG_MATH_API __declspec(dllimport)
    #endif
#else
    #ifdef HEDGEHOG_MATH_EXPORTS
        #define HEDGEHOG_MATH_API __attribute__((visibility("default")))
    #else
        #define HEDGEHOG_MATH_API
    #endif
#endif
