#pragma once

#include <cstdint>
#include <span>

namespace RHI
{

// GPU timestamps: a command list resets a range (ResetQueries, outside rendering) and writes
// timestamps into it (WriteTimestamp); once the GPU has run that work, ReadTimestamps reads them
// without waiting. Ticks times GetTickPeriodNs() is nanoseconds; only differences mean anything.
class IRHIQueryPool
{
public:
    virtual ~IRHIQueryPool() = default;

    IRHIQueryPool(const IRHIQueryPool&)            = delete;
    IRHIQueryPool& operator=(const IRHIQueryPool&) = delete;
    IRHIQueryPool(IRHIQueryPool&&)                 = delete;
    IRHIQueryPool& operator=(IRHIQueryPool&&)      = delete;

    virtual uint32_t GetCount() const = 0;

    // Nanoseconds per timestamp tick.
    virtual double GetTickPeriodNs() const = 0;

    // The timestamps first .. first + out.size() - 1, in ticks. False, without waiting, while any of
    // them has not been written by the GPU since its reset (out is then left unspecified), or for a
    // range past the pool.
    virtual bool ReadTimestamps(uint32_t first, std::span<uint64_t> out) const = 0;

protected:
    IRHIQueryPool() = default;
};

} // namespace RHI
