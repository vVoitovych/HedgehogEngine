#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace FS
{
    std::filesystem::path GetExecutableDirectory()
    {
#ifdef _WIN32
        char buffer[MAX_PATH];
        const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        assert(len > 0 && len < MAX_PATH && "GetModuleFileNameA failed or path truncated");
        return std::filesystem::path(buffer).parent_path();
#else
        assert(false && "GetExecutableDirectory is not implemented on this platform.");
        return {};
#endif
    }

    std::filesystem::path GetEngineRootDirectory()
    {
        const std::filesystem::path exeDir = GetExecutableDirectory();
        // Binary layout: Binaries/Windows-x64/<Config>/<Project>/exe
        // Walk up 4 levels to reach the repo root.
        const std::filesystem::path root =
            exeDir.parent_path().parent_path().parent_path().parent_path();
        assert(std::filesystem::exists(root) &&
               "Engine root not found — binary must be 4 levels deep "
               "(Binaries/Windows-x64/<Config>/<Project>/)");
        return root;
    }
}
