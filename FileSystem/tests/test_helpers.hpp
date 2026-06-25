#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

// RAII helper: creates a unique temporary directory and removes it on destruction.
// Use this per TEST_CASE so every test starts with a clean, isolated scratch area.
class TempDir
{
public:
    TempDir()
    {
        m_Path = std::filesystem::temp_directory_path()
            / ("fs_test_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(m_Path);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Path, ec);
    }

    // Non-copyable, non-movable — each test owns exactly one temp dir.
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    const std::filesystem::path& Path() const { return m_Path; }

    // Creates a file with the given content inside this temp dir.
    // Returns the path of the created file.
    std::filesystem::path WriteFile(const std::string& relativeName,
                                    const std::string& content) const
    {
        auto filePath = m_Path / relativeName;
        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream out(filePath, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return filePath;
    }

    // Creates a subdirectory inside this temp dir and returns its path.
    std::filesystem::path MakeSubdir(const std::string& relativeName) const
    {
        auto dirPath = m_Path / relativeName;
        std::filesystem::create_directories(dirPath);
        return dirPath;
    }

private:
    std::filesystem::path m_Path;
};
