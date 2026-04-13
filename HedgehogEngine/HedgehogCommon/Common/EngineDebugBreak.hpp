#pragma once


#define ENGINE_NOOP() \
    {              \
        (void)0;   \
    }


#ifdef DEBUG

#define ENGINE_DEBUG_BREAK() \
    {                     \
        __debugbreak();   \
    }

#else

#define ENGINE_DEBUG_BREAK() ENGINE_NOOP()

#endif

