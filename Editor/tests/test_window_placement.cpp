#include "Docking/WindowPlacement.hpp"
#include "EditorSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "doctest/doctest/doctest.h"

#include <array>
#include <memory>

using namespace Editor;

namespace
{
    // Two 1920x1080 monitors side by side, the second to the right; work areas without a taskbar.
    constexpr ScreenRect PRIMARY   = { 0.0f, 0.0f, 1920.0f, 1080.0f };
    constexpr ScreenRect SECONDARY = { 1920.0f, 0.0f, 1920.0f, 1080.0f };
    constexpr std::array<ScreenRect, 2> BOTH_MONITORS = { PRIMARY, SECONDARY };
    constexpr std::array<ScreenRect, 1> PRIMARY_ONLY  = { PRIMARY };

    // user:// at a temporary folder, as the editor mounts it.
    struct UserFolder
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        UserFolder()
        {
            auto fileSystem = std::make_unique<FS::FileSystem>();
            fileSystem->RegisterPath(EditorSettings::USER_ALIAS, Dir.Path());
            REQUIRE(Files.Register(std::move(fileSystem)));
        }
    };
}

TEST_CASE("WindowPlacement - a window a monitor shows stays where it is")
{
    CHECK(IsOnAnyMonitor({ 100.0f, 100.0f, 400.0f, 300.0f }, PRIMARY_ONLY));
    // On the second monitor, while it is there.
    CHECK_FALSE(FindPlaceOnScreen({ 2500.0f, 200.0f, 400.0f, 300.0f }, BOTH_MONITORS, PRIMARY).has_value());
    // Mostly off the right edge, but its title bar's left end still shows: enough to grab it.
    CHECK(IsOnAnyMonitor({ 1880.0f, 100.0f, 400.0f, 300.0f }, PRIMARY_ONLY));
    // Across both monitors.
    CHECK(IsOnAnyMonitor({ 1800.0f, 500.0f, 400.0f, 300.0f }, BOTH_MONITORS));
}

TEST_CASE("WindowPlacement - a window left on an unplugged monitor comes back onto the main one")
{
    const ScreenRect window = { 2500.0f, 200.0f, 400.0f, 300.0f };
    REQUIRE(IsOnAnyMonitor(window, BOTH_MONITORS));
    const auto place = FindPlaceOnScreen(window, PRIMARY_ONLY, PRIMARY);
    REQUIRE(place.has_value());
    CHECK(place->X == 1520.0f); // moved as little as it takes: its right edge on the monitor's
    CHECK(place->Y == 200.0f);
    CHECK(place->Width == 400.0f);
    CHECK(place->Height == 300.0f);
    CHECK(IsOnAnyMonitor(*place, PRIMARY_ONLY));
}

TEST_CASE("WindowPlacement - a title bar too little of which shows, or one above the screen, is moved")
{
    // Only 20 pixels of its width on the monitor: too little to grab.
    CHECK_FALSE(IsOnAnyMonitor({ 1900.0f, 100.0f, 400.0f, 300.0f }, PRIMARY_ONLY));
    // Its title bar above the top edge, the rest showing.
    const auto place = FindPlaceOnScreen({ 300.0f, -200.0f, 400.0f, 300.0f }, PRIMARY_ONLY, PRIMARY);
    REQUIRE(place.has_value());
    CHECK(place->X == 300.0f);
    CHECK(place->Y == 0.0f);
    // Nowhere at all, with no monitor known.
    CHECK_FALSE(IsOnAnyMonitor({ 0.0f, 0.0f, 400.0f, 300.0f }, {}));
}

TEST_CASE("WindowPlacement - a window larger than the monitor goes to its top-left corner")
{
    const ScreenRect moved = MoveOntoMonitor({ -5000.0f, 3000.0f, 2500.0f, 1500.0f }, PRIMARY);
    CHECK(moved.X == 0.0f);
    CHECK(moved.Y == 0.0f);
    CHECK(moved.Width == 2500.0f);
}

TEST_CASE("EditorSettings - floating panels' places and sizes and the floating views round-trip")
{
    UserFolder folder;
    EditorSettings settings;
    settings.dockLayout.InitDefaults();
    settings.dockLayout.MovePanel(PanelId::Inspector, DockArea::Floating);
    settings.dockLayout.FloatingPositions[static_cast<int>(PanelId::Inspector)] = { 2400.0f, 180.0f };
    settings.dockLayout.FloatingSizes[static_cast<int>(PanelId::Inspector)]     = { 520.0f, 760.0f };
    settings.SceneViewFloating = true;
    settings.Save(EditorSettings::PATH, folder.Files);

    EditorSettings loaded;
    REQUIRE(loaded.Load(EditorSettings::PATH, folder.Files));
    CHECK(loaded.dockLayout.FindPanelArea(PanelId::Inspector) == DockArea::Floating);
    const PanelPos& position = loaded.dockLayout.FloatingPositions[static_cast<int>(PanelId::Inspector)];
    const PanelPos& size     = loaded.dockLayout.FloatingSizes[static_cast<int>(PanelId::Inspector)];
    CHECK(position.x == 2400.0f);
    CHECK(position.y == 180.0f);
    CHECK(size.x == 520.0f);
    CHECK(size.y == 760.0f);
    CHECK(loaded.SceneViewFloating);
    CHECK_FALSE(loaded.GameViewFloating);
}

TEST_CASE("EditorSettings - a file from before floating sizes and views gives the defaults")
{
    UserFolder folder;
    folder.Dir.WriteFile("editor_settings.yaml",
                         "dock_layout:\n"
                         "  left_width: 300\n"
                         "  floating_positions:\n"
                         "    console: [30, 40]\n"
                         "  floating_sizes:\n"
                         "    content: [0, 200]\n" // not a size: ignored
                         "content_icon_size: 64\n");
    EditorSettings loaded;
    REQUIRE(loaded.Load(EditorSettings::PATH, folder.Files));
    CHECK(loaded.dockLayout.FloatingPositions[static_cast<int>(PanelId::Console)].x == 30.0f);
    for (const PanelPos& size : loaded.dockLayout.FloatingSizes)
    {
        CHECK(size.x == DEFAULT_FLOATING_PANEL_SIZE.x);
        CHECK(size.y == DEFAULT_FLOATING_PANEL_SIZE.y);
    }
    CHECK_FALSE(loaded.SceneViewFloating);
    CHECK_FALSE(loaded.GameViewFloating);
}
