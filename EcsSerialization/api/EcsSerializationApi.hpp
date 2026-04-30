#pragma once

#ifdef _WIN32
    #ifdef ECS_SERIALIZATION_EXPORT
        #define ECS_SERIALIZATION_API __declspec(dllexport)
    #else
        #define ECS_SERIALIZATION_API __declspec(dllimport)
    #endif
#else
    #define ECS_SERIALIZATION_API __attribute__((visibility("default")))
#endif
