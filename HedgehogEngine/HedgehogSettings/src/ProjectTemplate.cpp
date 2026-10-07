#include "HedgehogSettings/api/ProjectTemplate.hpp"
#include "HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include <memory>
#include <system_error>

namespace HedgehogSettings
{
    namespace
    {
        constexpr const char* PROJECT_FILE = "Project.yaml";

        // Copies the template's folders and files under target, placeholders left out.
        std::string CopyTemplate(const std::filesystem::path& templateDir, const std::filesystem::path& target)
        {
            std::error_code error;
            for (auto it = std::filesystem::recursive_directory_iterator(templateDir, error);
                 !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
            {
                const std::filesystem::path relative    = it->path().lexically_relative(templateDir);
                const std::filesystem::path destination = target / relative;
                if (it->is_directory(error))
                    std::filesystem::create_directories(destination, error);
                else if (it->path().filename() != FOLDER_PLACEHOLDER_NAME)
                    std::filesystem::copy_file(it->path(), destination, error);
                if (error)
                    return relative.generic_string() + " cannot be copied: " + error.message();
            }
            return error ? templateDir.string() + " cannot be read: " + error.message() : std::string();
        }

        // Takes back what a failed creation made: the folder it created, or the contents of the
        // empty folder it was given.
        void Undo(const std::filesystem::path& target, bool createdTarget)
        {
            std::error_code error;
            if (createdTarget)
            {
                std::filesystem::remove_all(target, error);
                return;
            }
            for (const auto& entry : std::filesystem::directory_iterator(target, error))
                std::filesystem::remove_all(entry.path(), error);
        }
    }

    std::string CreateProject(const std::filesystem::path& templateDir, const std::filesystem::path& targetDir,
                              const std::string& name)
    {
        if (!ProjectSettings::IsValidName(name))
            return "'" + name + "' is not a project name: 1 to 64 letters, digits, '_', '-' or inner spaces.";

        std::error_code error;
        if (!std::filesystem::is_regular_file(templateDir / PROJECT_FILE, error))
            return templateDir.string() + " is not a project template: it holds no " + PROJECT_FILE + ".";

        const bool targetExists = std::filesystem::exists(targetDir, error);
        if (targetExists && (!std::filesystem::is_directory(targetDir, error) || !std::filesystem::is_empty(targetDir, error)))
            return targetDir.string() + " is not an empty folder.";
        if (!targetExists && !std::filesystem::create_directories(targetDir, error))
            return targetDir.string() + " cannot be created: " + error.message();

        std::string failure = CopyTemplate(templateDir, targetDir);

        // The name, through the project's own settings, read and written as the engine will.
        if (failure.empty())
        {
            FS::FileSystemManager files;
            auto                  mount = std::make_unique<FS::FileSystem>();
            ProjectSettings       project;
            if (!mount->RegisterPath(FS::PROJECT_ALIAS, targetDir) || !files.Register(std::move(mount)) ||
                !project.Load(ProjectSettings::PATH, files))
                failure = "the template's " + std::string(PROJECT_FILE) + " cannot be read.";
            else if (!project.SetName(name) || !project.Save(ProjectSettings::PATH, files))
                failure = (targetDir / PROJECT_FILE).string() + " cannot be written.";
        }

        if (!failure.empty())
            Undo(targetDir, !targetExists);
        return failure;
    }
}
