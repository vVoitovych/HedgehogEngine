#pragma once

#ifdef _WIN32
    #ifdef LOGGER_EXPORT
        #define LOGGER_API __declspec(dllexport)
    #else
        #define LOGGER_API __declspec(dllimport)
    #endif
#else
    #define LOGGER_API __attribute__((visibility("default")))
#endif
