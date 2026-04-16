#pragma once

namespace RHI
{

// ── Fence — CPU/GPU synchronization ──────────────────────────────────────────

class IRHIFence
{
public:
    virtual ~IRHIFence() = default;

    IRHIFence(const IRHIFence&)            = delete;
    IRHIFence& operator=(const IRHIFence&) = delete;
    IRHIFence(IRHIFence&&)                 = delete;
    IRHIFence& operator=(IRHIFence&&)      = delete;

    // Block the calling CPU thread until the fence is signaled.
    virtual void Wait()  = 0;

    // Reset to unsignaled state so it can be reused.
    virtual void Reset() = 0;

protected:
    IRHIFence() = default;
};

// ── Semaphore — GPU/GPU synchronization ──────────────────────────────────────

class IRHISemaphore
{
public:
    virtual ~IRHISemaphore() = default;

    IRHISemaphore(const IRHISemaphore&)            = delete;
    IRHISemaphore& operator=(const IRHISemaphore&) = delete;
    IRHISemaphore(IRHISemaphore&&)                 = delete;
    IRHISemaphore& operator=(IRHISemaphore&&)      = delete;

protected:
    IRHISemaphore() = default;
};

} // namespace RHI
