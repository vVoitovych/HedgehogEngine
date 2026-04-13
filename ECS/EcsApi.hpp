#pragma once

#ifdef _WIN32
    #ifdef ECS_EXPORT
        #define ECS_API __declspec(dllexport)
    #else
        #define ECS_API __declspec(dllimport)
    #endif
#else
    #define ECS_API __attribute__((visibility("default")))
#endif
