#pragma once

#include "FileSystemApi.hpp"

#include <filesystem>

namespace FS
{
    // Returns the directory containing the running executable.
    // Only implemented on Windows; calls GetModuleFileNameA internally.
    FILE_SYSTEM_API std::filesystem::path GetExecutableDirectory();

    // Returns the engine repository root, computed by walking 4 levels up from
    // the executable directory (Binaries/Windows-x64/<Config>/<Project>/).
    // Asserts that the resulting path exists.
    FILE_SYSTEM_API std::filesystem::path GetEngineRootDirectory();
}
