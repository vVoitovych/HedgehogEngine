#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"
#include "HedgehogInput/api/InputOverrides.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

using HedgehogEngine::EngineContext;

namespace
{
    // Every Logger line while it lives (the engine's tests cannot use HedgehogScripting's capture).
    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Text += line + '\n'; }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        [[nodiscard]] bool Has(const std::string& fragment) const { return m_Text.find(fragment) != std::string::npos; }

    private:
        int         m_Sink = 0;
        std::string m_Text;
    };

    HW::RawInput WithKey(HW::Key key)
    {
        HW::RawInput input;
        input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }

    HInput::InputActionMap SubmitOnF()
    {
        HInput::InputBinding binding;
        binding.Source = HInput::BindingSource::Key;
        binding.Code   = static_cast<uint16_t>(HW::Key::F);
        HInput::InputActionMap overrides;
        overrides.Actions.push_back({ "UiSubmit", { binding } });
        return overrides;
    }

    // Whether key, pressed on a fresh frame in Play, presses UiSubmit.
    bool SubmitsWith(EngineContext& context, HW::Key key)
    {
        const std::optional<size_t> submit = HInput::FindAction(context.GetInputActions().Game, "UiSubmit");
        REQUIRE(submit);
        context.UpdateGameInput(HW::RawInput{});
        context.UpdateGameInput(WithKey(key));
        return HInput::WasActionPressed(context.GetGameActionState(), *submit);
    }

    // An engine whose player:// and assets:// are temp folders, assets:// holding the default actions.
    struct OverridesWorld
    {
        EngineContext Context;
        TempDir       Player;
        TempDir       Assets;

        OverridesWorld()
        {
            FS::FileSystemManager& files = Context.GetFileSystem();
            REQUIRE(FS::MountPlayer(files, Player.Path()));
            std::unique_ptr<FS::FileSystem> shipped = files.Unregister("assets://");
            REQUIRE(shipped);
            auto replaced = std::make_unique<FS::FileSystem>();
            for (const auto& [alias, path] : shipped->GetMountPoints())
            {
                REQUIRE(replaced->RegisterPath(alias, alias == "assets://" ? Assets.Path() : path));
            }
            REQUIRE(files.Register(std::move(replaced)));
            WriteProjectActions(Context.GetProjectInputActions());
        }

        // The project's actions file, its write time moved on so the next poll reads it.
        void WriteProjectActions(const HInput::InputActionSet& actions) const
        {
            Assets.WriteFile("Input/actions.yaml", HInput::WriteInputActions(actions));
            const std::filesystem::path path = Assets.Path() / "Input" / "actions.yaml";
            std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(10));
        }
    };
}

TEST_CASE("Input overrides - a file rebinding UiSubmit to F makes F press it and Enter not")
{
    OverridesWorld world;
    HInput::InputActionSet file;
    file.Game = SubmitOnF();
    world.Player.WriteFile("input_overrides.yaml", HInput::WriteInputActions(file));

    REQUIRE(world.Context.LoadInputOverrides());
    CHECK(world.Context.GetInputOverrides() == SubmitOnF());
    CHECK(world.Context.GetInputActions().Editor == world.Context.GetProjectInputActions().Editor);
    REQUIRE(world.Context.Play());
    CHECK(SubmitsWith(world.Context, HW::Key::F));
    CHECK_FALSE(SubmitsWith(world.Context, HW::Key::Enter));
    CHECK(world.Context.GetLastGameInput().Keys[static_cast<size_t>(HW::Key::Enter)]);
}

TEST_CASE("Input overrides - the project's file reloading keeps them, and resetting one restores the project's")
{
    OverridesWorld world;
    world.Context.SetInputOverrides(SubmitOnF());

    // The project gains an action; its reload merges the override again.
    HInput::InputActionSet project = world.Context.GetProjectInputActions();
    project.Game.Actions.push_back({ "Jump", { HInput::InputBinding{ HInput::BindingSource::Key,
                                                                     static_cast<uint16_t>(HW::Key::J) } } });
    world.WriteProjectActions(project);
    world.Context.ReloadInputActions(std::chrono::steady_clock::now() + std::chrono::hours(1));
    REQUIRE(HInput::FindAction(world.Context.GetInputActions().Game, "Jump"));
    REQUIRE(world.Context.Play());
    CHECK(SubmitsWith(world.Context, HW::Key::F));
    CHECK_FALSE(SubmitsWith(world.Context, HW::Key::Enter));

    HInput::InputActionMap overrides = world.Context.GetInputOverrides();
    REQUIRE(HInput::ResetActionBindings(overrides, "UiSubmit"));
    world.Context.SetInputOverrides(overrides);
    CHECK(SubmitsWith(world.Context, HW::Key::Enter));
    CHECK_FALSE(SubmitsWith(world.Context, HW::Key::F));
}

TEST_CASE("Input overrides - saved and loaded again; a missing file changes nothing")
{
    OverridesWorld world;
    CHECK_FALSE(world.Context.LoadInputOverrides()); // no file yet

    world.Context.SetInputOverrides(SubmitOnF());
    REQUIRE(world.Context.SaveInputOverrides());
    world.Context.SetInputOverrides({});
    REQUIRE(world.Context.LoadInputOverrides());
    CHECK(world.Context.GetInputOverrides() == SubmitOnF());
}

TEST_CASE("Input overrides - a malformed file logs its error and plays with the project's bindings")
{
    OverridesWorld world;
    world.Context.SetInputOverrides(SubmitOnF());
    world.Player.WriteFile("input_overrides.yaml", "Version: 1\nGame:\n  UiSubmit:\n    - Key: NoSuchKey\n");

    LogCapture log;
    CHECK_FALSE(world.Context.LoadInputOverrides());
    CHECK(log.Has("[Input] player://input_overrides.yaml"));
    CHECK(log.Has("playing with the project's bindings"));
    CHECK(world.Context.GetInputOverrides().Actions.empty());
    REQUIRE(world.Context.Play());
    CHECK(SubmitsWith(world.Context, HW::Key::Enter));
}
