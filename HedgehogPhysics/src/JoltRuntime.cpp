#include "JoltRuntime.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace HP
{
    namespace
    {
        std::mutex& RuntimeMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        int& RuntimeUsers()
        {
            static int users = 0;
            return users;
        }

        void TraceToLogger(const char* format, ...)
        {
            char    message[1024];
            va_list arguments;
            va_start(arguments, format);
            std::vsnprintf(message, sizeof(message), format, arguments);
            va_end(arguments);
            LOGINFO("[Jolt]", message);
        }

#ifdef JPH_ENABLE_ASSERTS
        // Logged, and execution goes on: breaking into a debugger would end a run with none attached.
        bool AssertToLogger(const char* expression, const char* message, const char* file, JPH::uint line)
        {
            LOGERROR("[Jolt] Assertion failed:", expression, message != nullptr ? message : "", "at", file, ":", line);
            return false;
        }
#endif
    }

    void AcquireJoltRuntime()
    {
        std::lock_guard<std::mutex> lock(RuntimeMutex());
        if (RuntimeUsers()++ > 0)
            return;

        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceToLogger;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertToLogger;)
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        // Jolt and HedgehogPhysics compiled with one configuration (Dependencies.lua's JoltDefines).
        [[maybe_unused]] const bool sameVersion = JPH::VerifyJoltVersionID();
        assert(sameVersion && "Jolt was compiled with other JoltDefines than HedgehogPhysics");
    }

    void ReleaseJoltRuntime()
    {
        std::lock_guard<std::mutex> lock(RuntimeMutex());
        assert(RuntimeUsers() > 0 && "ReleaseJoltRuntime without AcquireJoltRuntime");
        if (--RuntimeUsers() > 0)
            return;

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}
