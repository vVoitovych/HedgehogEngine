#pragma once

#ifdef _WIN32
    #ifdef CONTENT_LOADER_EXPORT
        #define CONTENT_LOADER_API __declspec(dllexport)
    #else
        #define CONTENT_LOADER_API __declspec(dllimport)
    #endif
#else
    #ifdef CONTENT_LOADER_EXPORT
        #define CONTENT_LOADER_API __attribute__((visibility("default")))
    #else
        #define CONTENT_LOADER_API
    #endif
#endif
