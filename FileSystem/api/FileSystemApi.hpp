#pragma once

#ifdef _WIN32
    #ifdef FILE_SYSTEM_EXPORT
        #define FILE_SYSTEM_API __declspec(dllexport)
    #else
        #define FILE_SYSTEM_API __declspec(dllimport)
    #endif
#else
    #ifdef FILE_SYSTEM_EXPORT
        #define FILE_SYSTEM_API __attribute__((visibility("default")))
    #else
        #define FILE_SYSTEM_API
    #endif
#endif
