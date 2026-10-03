#include "HedgehogInput/api/InputActionAsset.hpp"

#include "HedgehogInput/api/InputActionFile.hpp"

#include "Logger/api/Logger.hpp"

namespace HInput
{
    namespace
    {
        // The file's write time, or nullopt when it cannot be found. FileSystem has no write-time
        // query, so this goes through the physical path, as script hot reload does.
        std::optional<std::filesystem::file_time_type> WriteTimeOf(const std::string&           virtualPath,
                                                                   const FS::FileSystemManager& fileSystem)
        {
            const std::optional<std::filesystem::path> physical = fileSystem.ResolvePhysical(virtualPath);
            if (!physical)
                return std::nullopt;
            std::error_code error;
            const auto      writeTime = std::filesystem::last_write_time(*physical, error);
            if (error)
                return std::nullopt;
            return writeTime;
        }
    }

    std::optional<InputActionSet> LoadInputActions(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        const std::optional<std::string> text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("[Input]", virtualPath + ":", "the file cannot be read.");
            return std::nullopt;
        }
        InputActionParseResult result = ParseInputActions(*text);
        if (!result.Actions)
        {
            LOGERROR("[Input]", virtualPath + ":", result.Error);
            return std::nullopt;
        }
        return std::move(result.Actions);
    }

    InputActionsWatch WatchInputActions(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        InputActionsWatch watch;
        watch.VirtualPath = virtualPath;
        watch.WriteTime   = WriteTimeOf(virtualPath, fileSystem);
        return watch;
    }

    bool PollInputActions(InputActionsWatch& watch, const FS::FileSystemManager& fileSystem,
                          std::chrono::steady_clock::time_point now, InputActionSet& out)
    {
        if (watch.LastPoll && now - *watch.LastPoll < INPUT_ACTIONS_POLL_INTERVAL)
            return false;
        watch.LastPoll = now;

        const std::optional<std::filesystem::file_time_type> writeTime = WriteTimeOf(watch.VirtualPath, fileSystem);
        if (!writeTime || writeTime == watch.WriteTime || writeTime == watch.FailedWriteTime)
            return false;

        std::optional<InputActionSet> actions = LoadInputActions(watch.VirtualPath, fileSystem);
        if (!actions)
        {
            watch.FailedWriteTime = writeTime;
            return false;
        }
        out                   = std::move(*actions);
        watch.WriteTime       = writeTime;
        watch.FailedWriteTime = std::nullopt;
        LOGINFO("[Input] Reloaded", watch.VirtualPath + ".");
        return true;
    }
}
