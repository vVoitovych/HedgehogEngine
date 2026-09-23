#pragma once

#include "CompiledGraph.hpp"
#include "GraphDescription.hpp"

#include <vector>

namespace Renderer
{
    struct CompileResult
    {
        bool                      Success = false;
        std::vector<CompileError> Errors;   // populated only when !Success
        CompiledGraph             Graph;    // populated only when Success
    };

    // RENDERING.md section 5.3. Device-free by construction: takes a GraphDescription, returns
    // either a culled/sorted/barrier-annotated CompiledGraph or the full list of validation
    // failures found. Never partially succeeds — Success == false means Graph is empty and
    // Errors names every problem found.
    class GraphCompiler
    {
    public:
        CompileResult Compile(const GraphDescription& description) const;
    };
}
