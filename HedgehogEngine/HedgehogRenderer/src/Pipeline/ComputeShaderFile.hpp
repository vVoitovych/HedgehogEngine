#pragma once

#include <string>

namespace Renderer
{
    // What a compute .shader file names: its pipeline layout and its one compute stage's SPIR-V, both
    // as written (relative to the .shader file). Error is empty on success, else why the file is
    // refused, naming it.
    struct ComputeShaderFile
    {
        std::string LayoutPath;
        std::string ShaderPath;
        std::string Error;
    };

    // Reads a compute .shader: `pipeline_layout` and `shaders` holding exactly one `stage: compute`.
    // A file that does not parse, lacks either key, has no compute stage, or has any other stage
    // beside it is refused. No file is read and no device is touched, so it is testable headless.
    [[nodiscard]] ComputeShaderFile ParseComputeShaderFile(const std::string& text, const std::string& virtualPath);
}
