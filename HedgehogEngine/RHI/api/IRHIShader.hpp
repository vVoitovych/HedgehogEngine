#pragma once

#include "RHITypes.hpp"

namespace RHI
{

class IRHIShader
{
public:
    virtual ~IRHIShader() = default;

    IRHIShader(const IRHIShader&)            = delete;
    IRHIShader& operator=(const IRHIShader&) = delete;
    IRHIShader(IRHIShader&&)                 = delete;
    IRHIShader& operator=(IRHIShader&&)      = delete;

    virtual ShaderStage GetStage() const = 0;

protected:
    IRHIShader() = default;
};

} // namespace RHI
