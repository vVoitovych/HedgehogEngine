#include "HedgehogEngine/api/Platform/DynamicLibrary.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>

namespace HedgehogEngine
{
    namespace
    {
        // The system's message for the last error, without its trailing line break.
        std::string LastErrorMessage()
        {
            const DWORD code = GetLastError();
            std::array<char, 512> buffer{};
            const DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                                                buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
            std::string message(buffer.data(), length);
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ' || message.back() == '.'))
                message.pop_back();
            return message.empty() ? "error " + std::to_string(code) : message;
        }
    }

    bool DynamicLibrary::Open(const std::filesystem::path& path)
    {
        Close();
        m_Path = std::filesystem::absolute(path);
        m_Error.clear();
        // DLL_LOAD_DIR: the plugin's own dependencies resolve beside it (it needs an absolute path).
        HMODULE module = LoadLibraryExW(m_Path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module)
        {
            m_Error = "cannot open " + m_Path.string() + ": " + LastErrorMessage();
            return false;
        }
        m_Handle = module;
        return true;
    }

    void* DynamicLibrary::FindSymbol(const char* name)
    {
        if (!m_Handle)
        {
            m_Error = "no library is open";
            return nullptr;
        }
        FARPROC symbol = GetProcAddress(static_cast<HMODULE>(m_Handle), name);
        if (!symbol)
        {
            m_Error = m_Path.filename().string() + " has no symbol " + name;
            return nullptr;
        }
        return reinterpret_cast<void*>(symbol);
    }

    void DynamicLibrary::Close()
    {
        if (m_Handle)
            FreeLibrary(static_cast<HMODULE>(m_Handle));
        m_Handle = nullptr;
    }

    void DynamicLibrary::Release()
    {
        m_Handle = nullptr;
    }

    bool DynamicLibrary::Contains(const void* address) const
    {
        if (!m_Handle || !address)
            return false;
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCWSTR>(address), &module))
            return false;
        return module == static_cast<HMODULE>(m_Handle);
    }

    uint32_t GetCurrentProcessNumber()
    {
        return static_cast<uint32_t>(GetCurrentProcessId());
    }

    std::filesystem::path GetEngineModuleDirectory()
    {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&GetEngineModuleDirectory), &module);
        std::wstring path(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
            if (length < path.size())
            {
                path.resize(length);
                break;
            }
            path.resize(path.size() * 2);
        }
        return std::filesystem::path(path).parent_path();
    }
}
