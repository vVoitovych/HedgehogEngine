#include "ShellActions.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <string>

namespace Editor
{
    namespace
    {
        // ShellExecute returns a value above 32 on success.
        constexpr INT_PTR SHELL_EXECUTE_SUCCESS = 32;
    }

    bool OpenWithDefaultApplication(const std::filesystem::path& file)
    {
        const HINSTANCE result = ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > SHELL_EXECUTE_SUCCESS;
    }

    bool LaunchDetached(const std::filesystem::path& executable, const std::filesystem::path& workingDirectory)
    {
        const HINSTANCE result = ShellExecuteW(nullptr, L"open", executable.c_str(), nullptr, workingDirectory.c_str(),
                                               SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > SHELL_EXECUTE_SUCCESS;
    }

    bool ShowInExplorer(const std::filesystem::path& item)
    {
        const std::wstring arguments = L"/select,\"" + std::filesystem::path(item).make_preferred().wstring() + L"\"";
        const HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > SHELL_EXECUTE_SUCCESS;
    }
}
