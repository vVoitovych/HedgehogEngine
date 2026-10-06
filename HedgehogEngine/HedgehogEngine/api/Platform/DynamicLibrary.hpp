#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <filesystem>
#include <string>
#include <utility>

namespace HedgehogEngine
{
    // A shared library opened at runtime (a plugin DLL), closed when this goes. The platform code
    // lives in src/Platform/DynamicLibrary<Platform>.cpp, so another platform adds one file.
    class DynamicLibrary
    {
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary() { Close(); }

        DynamicLibrary(const DynamicLibrary&)            = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        DynamicLibrary(DynamicLibrary&& other) noexcept
            : m_Handle(std::exchange(other.m_Handle, nullptr))
            , m_Path(std::move(other.m_Path))
            , m_Error(std::move(other.m_Error))
        {
        }

        DynamicLibrary& operator=(DynamicLibrary&& other) noexcept
        {
            if (this != &other)
            {
                Close();
                m_Handle = std::exchange(other.m_Handle, nullptr);
                m_Path   = std::move(other.m_Path);
                m_Error  = std::move(other.m_Error);
            }
            return *this;
        }

        // Opens the library at path (closing one already open); its own dependencies are looked
        // for beside it first. False, with GetError, when it cannot be opened.
        HEDGEHOG_ENGINE_API bool Open(const std::filesystem::path& path);
        // The exported symbol name, or nullptr (with GetError) when the library has none.
        [[nodiscard]] HEDGEHOG_ENGINE_API void* FindSymbol(const char* name);
        HEDGEHOG_ENGINE_API void Close();

        [[nodiscard]] bool                         IsOpen() const { return m_Handle != nullptr; }
        [[nodiscard]] const std::filesystem::path& GetPath() const { return m_Path; }
        // Why the last Open or FindSymbol failed.
        [[nodiscard]] const std::string&           GetError() const { return m_Error; }

    private:
        void*                 m_Handle = nullptr;
        std::filesystem::path m_Path;
        std::string           m_Error;
    };

    // The folder holding HedgehogEngine.dll: the shared binaries folder in development, the game's
    // own folder in a package. Plugin DLLs are looked for there.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::filesystem::path GetEngineModuleDirectory();
}
