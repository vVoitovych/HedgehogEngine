#include "Profiler/api/Profiler.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace Profiling
{
    namespace
    {
        struct OpenZone
        {
            const char* Name    = nullptr;
            uint64_t    StartNs = 0;
        };

        // One thread's zones since the last frame. Its owner writes and MarkFrame takes them, each
        // under the buffer's own lock, so threads never wait for one another.
        struct ThreadBuffer
        {
            std::mutex              Mutex;
            uint32_t                Index = 0;
            std::string             Name;
            std::vector<OpenZone>   Open;
            std::vector<ZoneRecord> Ended;
            bool                    Finished = false; // its thread ended; dropped once its zones are taken
        };

        struct Recorder
        {
            std::atomic<bool>                          Enabled{ false };
            std::mutex                                 Mutex; // the thread list and the ring
            std::vector<std::shared_ptr<ThreadBuffer>> Threads;
            uint32_t                                   NextThreadIndex = 0;
            std::vector<FrameRecord>                   Ring = std::vector<FrameRecord>(DEFAULT_FRAME_HISTORY);
            size_t                                     Newest = 0;
            size_t                                     Count  = 0;
            uint64_t                                   NextFrameIndex = 0;
            uint64_t                                   FrameStartNs   = 0;
        };

        // Never destroyed: threads may still end zones while the process exits.
        Recorder& GetRecorder()
        {
            static Recorder* recorder = new Recorder();
            return *recorder;
        }

        // Registers the calling thread on first use and lets go of it when the thread ends.
        struct ThreadSlot
        {
            std::shared_ptr<ThreadBuffer> Buffer = std::make_shared<ThreadBuffer>();

            ThreadSlot()
            {
                Recorder&             recorder = GetRecorder();
                const std::lock_guard lock(recorder.Mutex);
                Buffer->Index = recorder.NextThreadIndex++;
                Buffer->Name  = "Thread " + std::to_string(Buffer->Index);
                recorder.Threads.push_back(Buffer);
            }

            // A thread that ends before the frame is marked still has its zones in that frame.
            ~ThreadSlot()
            {
                Recorder&             recorder = GetRecorder();
                const std::lock_guard lock(recorder.Mutex);
                const std::lock_guard threadLock(Buffer->Mutex);
                if (Buffer->Ended.empty())
                    std::erase(recorder.Threads, Buffer);
                else
                    Buffer->Finished = true;
            }
        };

        ThreadBuffer& CurrentThread()
        {
            thread_local ThreadSlot slot;
            return *slot.Buffer;
        }
    }

    uint64_t NowNs()
    {
        static const auto epoch = std::chrono::steady_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - epoch).count());
    }

    void SetEnabled(bool enabled)
    {
        GetRecorder().Enabled.store(enabled, std::memory_order_relaxed);
    }

    bool IsEnabled()
    {
        return GetRecorder().Enabled.load(std::memory_order_relaxed);
    }

    bool BeginZone(const char* name)
    {
        if (!IsEnabled())
            return false;
        ThreadBuffer&         thread = CurrentThread();
        const std::lock_guard lock(thread.Mutex);
        thread.Open.push_back({ name, NowNs() });
        return true;
    }

    void EndZone()
    {
        const uint64_t        now    = NowNs();
        ThreadBuffer&         thread = CurrentThread();
        const std::lock_guard lock(thread.Mutex);
        if (thread.Open.empty())
            return; // Clear dropped it
        const OpenZone zone = thread.Open.back();
        thread.Open.pop_back();
        thread.Ended.push_back({ zone.Name, zone.StartNs, now - zone.StartNs, static_cast<uint32_t>(thread.Open.size()) });
    }

    void MarkFrame()
    {
        Recorder&             recorder = GetRecorder();
        const uint64_t        now      = NowNs();
        const std::lock_guard lock(recorder.Mutex);

        const uint64_t start   = recorder.FrameStartNs;
        recorder.FrameStartNs  = now;
        const uint64_t index   = recorder.NextFrameIndex++;
        if (!IsEnabled())
        {
            std::erase_if(recorder.Threads, [](const std::shared_ptr<ThreadBuffer>& buffer) { return buffer->Finished; });
            return;
        }

        recorder.Newest = (recorder.Newest + 1) % recorder.Ring.size();
        recorder.Count  = std::min(recorder.Count + 1, recorder.Ring.size());
        FrameRecord& frame = recorder.Ring[recorder.Newest];
        frame.Index   = index;
        frame.StartNs = start;
        frame.EndNs   = now;

        // The slot's thread records are reused in place, and each thread's ended zones are swapped
        // with the slot's old ones, so steady recording allocates nothing.
        size_t used = 0;
        for (const std::shared_ptr<ThreadBuffer>& buffer : recorder.Threads)
        {
            const std::lock_guard threadLock(buffer->Mutex);
            if (buffer->Ended.empty())
                continue;
            if (used == frame.Threads.size())
                frame.Threads.emplace_back();
            ThreadRecord& thread = frame.Threads[used++];
            thread.ThreadIndex   = buffer->Index;
            thread.ThreadName    = buffer->Name;
            thread.Zones.swap(buffer->Ended);
            buffer->Ended.clear();
        }
        // Records past the ones used keep their storage for later frames, emptied.
        for (size_t i = used; i < frame.Threads.size(); ++i)
            frame.Threads[i].Zones.clear();
        frame.Threads.resize(used);
        std::erase_if(recorder.Threads, [](const std::shared_ptr<ThreadBuffer>& buffer) { return buffer->Finished; });
    }

    size_t GetFrameCount()
    {
        return GetRecorder().Count;
    }

    const FrameRecord& GetFrame(size_t age)
    {
        Recorder& recorder = GetRecorder();
        const size_t size  = recorder.Ring.size();
        return recorder.Ring[(recorder.Newest + size - (std::min(age, size - 1))) % size];
    }

    void SetFrameHistory(size_t frames)
    {
        Recorder&             recorder = GetRecorder();
        const std::lock_guard lock(recorder.Mutex);
        recorder.Ring.assign(std::max<size_t>(frames, 1), FrameRecord{});
        recorder.Newest = 0;
        recorder.Count  = 0;
    }

    void Clear()
    {
        Recorder&             recorder = GetRecorder();
        const std::lock_guard lock(recorder.Mutex);
        for (FrameRecord& frame : recorder.Ring)
            frame.Threads.clear();
        recorder.Newest = 0;
        recorder.Count  = 0;
        for (const std::shared_ptr<ThreadBuffer>& buffer : recorder.Threads)
        {
            const std::lock_guard threadLock(buffer->Mutex);
            buffer->Open.clear();
            buffer->Ended.clear();
        }
        std::erase_if(recorder.Threads, [](const std::shared_ptr<ThreadBuffer>& buffer) { return buffer->Finished; });
    }

    void SetCurrentThreadName(const char* name)
    {
        ThreadBuffer& thread = CurrentThread();
        Recorder&     recorder = GetRecorder();
        const std::lock_guard lock(recorder.Mutex); // MarkFrame copies names under it
        thread.Name = name;
    }
}
