#pragma once

namespace RHI
{

class IRHISampler
{
public:
    virtual ~IRHISampler() = default;

    IRHISampler(const IRHISampler&)            = delete;
    IRHISampler& operator=(const IRHISampler&) = delete;
    IRHISampler(IRHISampler&&)                 = delete;
    IRHISampler& operator=(IRHISampler&&)      = delete;

protected:
    IRHISampler() = default;
};

} // namespace RHI
