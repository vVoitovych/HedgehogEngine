#pragma once

#ifdef _WIN32
    #ifdef PROFILER_EXPORT
        #define PROFILER_API __declspec(dllexport)
    #else
        #define PROFILER_API __declspec(dllimport)
    #endif
#else
    #define PROFILER_API __attribute__((visibility("default")))
#endif
