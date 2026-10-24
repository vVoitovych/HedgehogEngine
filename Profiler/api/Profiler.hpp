#pragma once

#include "Profiler/api/ProfilerApi.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The engine's CPU profiler: named zones recorded per thread into a ring of recent frames, for the
// editor's profiler. Off until SetEnabled(true); a zone of a disabled recorder is one check and
// records nothing. HH_PROFILE_ZONE also opens a Tracy zone where TRACY_ENABLE is defined.
namespace Profiling
{
    // One zone that ended in its frame. Times are on the recorder's clock (NowNs); a zone that began
    // in an earlier frame starts before its frame does.
    struct ZoneRecord
    {
        const char* Name       = nullptr; // the zone's name, a string literal
        uint64_t    StartNs    = 0;
        uint64_t    DurationNs = 0;
        uint32_t    Depth      = 0;       // zones open around it on its thread when it began
    };

    // A thread's zones of one frame, in the order they ended: a child before its parent.
    struct ThreadRecord
    {
        uint32_t                ThreadIndex = 0; // numbered as threads first record, from 0
        std::string             ThreadName;      // SetCurrentThreadName's, else "Thread <index>"
        std::vector<ZoneRecord> Zones;
    };

    // From one MarkFrame to the next: every thread that ended a zone in it.
    struct FrameRecord
    {
        uint64_t                  Index   = 0; // frames marked since the process started
        uint64_t                  StartNs = 0;
        uint64_t                  EndNs   = 0;
        std::vector<ThreadRecord> Threads;
    };

    inline constexpr size_t DEFAULT_FRAME_HISTORY = 240;

    PROFILER_API void SetEnabled(bool enabled);
    [[nodiscard]] PROFILER_API bool IsEnabled();

    // Ends the current frame and starts the next (once per frame, on the frame's thread). While
    // enabled it keeps the ended frame in the ring, the oldest dropped past the history; its
    // buffers are reused, so once warm recording allocates nothing.
    PROFILER_API void MarkFrame();

    // The frames kept, the newest at age 0. Read them on the thread that marks frames: MarkFrame
    // reuses the oldest's storage.
    [[nodiscard]] PROFILER_API size_t             GetFrameCount();
    [[nodiscard]] PROFILER_API const FrameRecord& GetFrame(size_t age);

    // How many frames are kept (at least 1); drops those kept.
    PROFILER_API void SetFrameHistory(size_t frames);
    // Drops every frame kept and every zone not yet in one.
    PROFILER_API void Clear();

    PROFILER_API void SetCurrentThreadName(const char* name);

    [[nodiscard]] PROFILER_API uint64_t NowNs();

    // Opens a zone on this thread, returning whether it is recorded (false while disabled); only a
    // recorded zone is ended. Use HH_PROFILE_ZONE or ScopedZone rather than these.
    [[nodiscard]] PROFILER_API bool BeginZone(const char* name);
    PROFILER_API void               EndZone();

    class ScopedZone
    {
    public:
        explicit ScopedZone(const char* name)
            : m_Recorded(BeginZone(name))
        {
        }

        ~ScopedZone()
        {
            if (m_Recorded)
                EndZone();
        }

        ScopedZone(const ScopedZone&)            = delete;
        ScopedZone& operator=(const ScopedZone&) = delete;

    private:
        bool m_Recorded;
    };
}

#define HH_PROFILE_CONCAT_INNER(a, b) a##b
#define HH_PROFILE_CONCAT(a, b)       HH_PROFILE_CONCAT_INNER(a, b)

#ifdef TRACY_ENABLE

#include <tracy/Tracy.hpp>

#define HH_PROFILE_ZONE(name)                                                \
    ::Profiling::ScopedZone HH_PROFILE_CONCAT(hhProfileZone, __LINE__)(name); \
    ZoneScopedN(name)
#define HH_PROFILE_FRAME() \
    ::Profiling::MarkFrame(); \
    FrameMark

#else

#define HH_PROFILE_ZONE(name) ::Profiling::ScopedZone HH_PROFILE_CONCAT(hhProfileZone, __LINE__)(name)
#define HH_PROFILE_FRAME()    ::Profiling::MarkFrame()

#endif
