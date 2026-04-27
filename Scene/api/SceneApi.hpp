#pragma once

#ifdef _WIN32
    #ifdef SCENE_EXPORT
        #define SCENE_API __declspec(dllexport)
    #else
        #define SCENE_API __declspec(dllimport)
    #endif
#else
    #define SCENE_API __attribute__((visibility("default")))
#endif
