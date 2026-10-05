#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

namespace FS
{
    class FileSystemManager;
}

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Editor
{
    // The editor's small line icons (Editor/Resources/Icons/UI/, sources in its README).
    enum class EditorIcon
    {
        Logo,
        Play,
        Pause,
        Stop,
        Settings,
        Plus,
        Search,
        More,
        Hierarchy,
        Scene,
        Game,
        Inspector,
        Project,
        Console,
        GameObject,
        Folder,
        Camera,
        Light,
        Mesh,
        Transform,
        Material,
        Script,
        Animator,
        UiCanvas,
        UiRect,
        UiImage,
        UiText,
        UiButton,
        AudioSource,
        AudioListener,
        Count
    };

    inline constexpr size_t EDITOR_ICON_COUNT = static_cast<size_t>(EditorIcon::Count);

    // The ImGui texture id of each icon, indexed by EditorIcon; nullptr for one that did not load.
    using EditorIconIds = std::array<void*, EDITOR_ICON_COUNT>;

    // The icon's file name, which also names its texture id.
    [[nodiscard]] const char* GetEditorIconFile(EditorIcon icon);

    // The icon a name stands for: its file name without ".png" (the names the component type
    // registry uses, "mesh", "ui_canvas", ...). Nullopt for an empty or unknown name.
    [[nodiscard]] std::optional<EditorIcon> FindEditorIcon(std::string_view name);

    // The icon textures, uploaded once when the device is ready. An icon whose file does not load
    // logs one warning and has no texture; where it would be drawn, nothing is.
    class EditorIcons
    {
    public:
        EditorIcons();
        ~EditorIcons();

        EditorIcons(const EditorIcons&)            = delete;
        EditorIcons& operator=(const EditorIcons&) = delete;
        EditorIcons(EditorIcons&&)                 = delete;
        EditorIcons& operator=(EditorIcons&&)      = delete;

        void Load(const RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);

        // Destroys the textures: after the GPU is idle and every id made from them is released.
        void Release();

        // The icon's texture, or nullptr.
        [[nodiscard]] const RHI::IRHITexture* Get(EditorIcon icon) const;

    private:
        std::array<std::unique_ptr<RHI::IRHITexture>, EDITOR_ICON_COUNT> m_Textures;
    };
}
