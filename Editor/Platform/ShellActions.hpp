#pragma once

#include <filesystem>

namespace Editor
{
    // Opens a file with the application the OS associates with it. False when the OS refuses.
    bool OpenWithDefaultApplication(const std::filesystem::path& file);

    // Opens a File Explorer window on the item's folder with the item selected.
    bool ShowInExplorer(const std::filesystem::path& item);

    // Starts executable as a process of its own, in workingDirectory, without waiting for it.
    bool LaunchDetached(const std::filesystem::path& executable, const std::filesystem::path& workingDirectory);
}
