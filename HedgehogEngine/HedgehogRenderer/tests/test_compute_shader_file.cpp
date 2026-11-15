#include "../src/Pipeline/ComputeShaderFile.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Renderer;

namespace
{
    const std::string PATH = "engine://Shaders/Emit.shader";

    // True when the file is refused with an error naming it and the given reason.
    bool Refused(const std::string& text, const std::string& reason)
    {
        const ComputeShaderFile file = ParseComputeShaderFile(text, PATH);
        return file.LayoutPath.empty() && file.ShaderPath.empty()
            && file.Error.rfind(PATH + ": ", 0) == 0 && file.Error.find(reason) != std::string::npos;
    }
}

TEST_CASE("A compute .shader names its layout and its one compute stage")
{
    const ComputeShaderFile file = ParseComputeShaderFile(
        "pipeline_layout: ../Pipelines/ParticleCompute.pl\n"
        "shaders:\n"
        "  - stage: compute\n"
        "    path: Particles/Emit.comp.spv\n",
        PATH);
    CHECK(file.Error.empty());
    CHECK(file.LayoutPath == "../Pipelines/ParticleCompute.pl");
    CHECK(file.ShaderPath == "Particles/Emit.comp.spv");
}

TEST_CASE("A .shader that is not one compute stage is refused with one error naming the file")
{
    CHECK(Refused("pipeline_layout: [", "does not parse"));
    CHECK(Refused("- a\n- b\n", "is not a map"));
    CHECK(Refused("shaders:\n  - stage: compute\n    path: a.spv\n", "has no pipeline_layout"));
    CHECK(Refused("pipeline_layout: a.pl\n", "has no shaders"));
    CHECK(Refused("pipeline_layout: a.pl\nshaders: []\n", "has no shaders"));
    CHECK(Refused("pipeline_layout: a.pl\nshaders:\n  - stage: compute\n", "without a stage and a path"));
    // A graphics shader, and a compute stage mixed with a graphics one.
    CHECK(Refused("pipeline_layout: a.pl\nshaders:\n  - stage: vertex\n    path: a.vert.spv\n",
                  "has a vertex stage"));
    CHECK(Refused("pipeline_layout: a.pl\nshaders:\n"
                  "  - stage: compute\n    path: a.comp.spv\n"
                  "  - stage: fragment\n    path: a.frag.spv\n",
                  "has a fragment stage"));
    CHECK(Refused("pipeline_layout: a.pl\nshaders:\n"
                  "  - stage: compute\n    path: a.comp.spv\n"
                  "  - stage: compute\n    path: b.comp.spv\n",
                  "more than one compute stage"));
}
