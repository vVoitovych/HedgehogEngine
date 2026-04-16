#pragma once

#include <cstddef>

namespace RHI
{

class IRHIBuffer
{
public:
    virtual ~IRHIBuffer() = default;

    IRHIBuffer(const IRHIBuffer&)            = delete;
    IRHIBuffer& operator=(const IRHIBuffer&) = delete;
    IRHIBuffer(IRHIBuffer&&)                 = delete;
    IRHIBuffer& operator=(IRHIBuffer&&)      = delete;

    virtual size_t GetSize() const = 0;

    // For CPU-accessible buffers (MemoryUsage::CpuToGpu / GpuToCpu).
    virtual void  CopyData(const void* data, size_t size, size_t offset = 0) = 0;
    virtual void* Map()   = 0;
    virtual void  Unmap() = 0;

protected:
    IRHIBuffer() = default;
};

} // namespace RHI
