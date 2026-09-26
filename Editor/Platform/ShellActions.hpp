#pragma once

#include <filesystem>

namespace Editor
{
    // Opens a file with the application the OS associates with it. False when the OS refuses.
    bool OpenWithDefaultApplication(const std::filesystem::path& file);

    // Opens a File Explorer window on the item's folder with the item selected.
    bool ShowInExplorer(const std::filesystem::path& item);
}
