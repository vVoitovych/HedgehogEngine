#include "api/ShaderDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int shaderFilterNum = 1;
    char const* shaderFilterPatterns[shaderFilterNum] = { "*.shader" };
    const char* shaderFilterDescription = "Shader files (*.shader)";

    constexpr int spvFilterNum = 1;
    char const* spvFilterPatterns[spvFilterNum] = { "*.spv" };
    const char* spvFilterDescription = "SPIR-V files (*.spv)";

    char* ShaderOpenDialogue()
    {
        return tinyfd_openFileDialog(
            "Open shader",
            "../",
            shaderFilterNum,
            shaderFilterPatterns,
            shaderFilterDescription,
            0);
    }

    char* ShaderSaveDialogue()
    {
        return tinyfd_saveFileDialog(
            "Save shader",
            "../",
            shaderFilterNum,
            shaderFilterPatterns,
            shaderFilterDescription);
    }

    char* SpvOpenDialogue()
    {
        return tinyfd_openFileDialog(
            "Open SPIR-V binary",
            "../",
            spvFilterNum,
            spvFilterPatterns,
            spvFilterDescription,
            0);
    }
}
