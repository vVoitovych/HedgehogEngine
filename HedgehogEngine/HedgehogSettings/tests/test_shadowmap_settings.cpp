#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>

using HedgehogSettings::Settings;
using HedgehogSettings::ShadowmapSettings;

namespace
{
    constexpr const char* PATH = "project://engine_settings.yaml";

    // project:// on a folder, a fresh temp one by default.
    struct SettingsFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        SettingsFiles() : SettingsFiles(std::filesystem::path()) {}

        explicit SettingsFiles(const std::filesystem::path& root)
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("project://", root.empty() ? Dir.Path() : root);
            Files.Register(std::move(fs));
        }

        void Write(const std::string& text) { REQUIRE(Files.WriteTextFile(PATH, text)); }

        std::string Read() const
        {
            std::string text = Files.ReadTextFile(PATH).value_or("");
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            return text;
        }
    };

    std::filesystem::path RepositoryRoot()
    {
        // tests/ -> HedgehogSettings/ -> HedgehogEngine/ -> the repository.
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    }

    void CheckDefaults(const ShadowmapSettings& shadow)
    {
        CHECK(shadow.GetDepthBias() == ShadowmapSettings::DEFAULT_DEPTH_BIAS);
        CHECK(shadow.GetSlopeBias() == ShadowmapSettings::DEFAULT_SLOPE_BIAS);
        CHECK(shadow.GetNormalOffset() == ShadowmapSettings::DEFAULT_NORMAL_OFFSET);
        CHECK(shadow.GetPcfRadius() == ShadowmapSettings::DEFAULT_PCF_RADIUS);
        CHECK(shadow.GetCascadeBlend() == ShadowmapSettings::DEFAULT_CASCADE_BLEND);
    }
}

TEST_CASE("Shadow sampling settings - the defaults, and the setters clamp to their ranges")
{
    ShadowmapSettings shadow;
    CHECK(ShadowmapSettings::DEFAULT_DEPTH_BIAS == 0.0005f);
    CHECK(ShadowmapSettings::DEFAULT_SLOPE_BIAS == 0.002f);
    CHECK(ShadowmapSettings::DEFAULT_NORMAL_OFFSET == 1.0f);
    CHECK(ShadowmapSettings::DEFAULT_PCF_RADIUS == 1u);
    CHECK(ShadowmapSettings::DEFAULT_CASCADE_BLEND == 0.1f);
    CheckDefaults(shadow);

    shadow.SetDepthBias(1.0f);
    shadow.SetSlopeBias(-1.0f);
    shadow.SetNormalOffset(100.0f);
    shadow.SetPcfRadius(9);
    shadow.SetCascadeBlend(0.75f);
    CHECK(shadow.GetDepthBias() == ShadowmapSettings::MAX_DEPTH_BIAS);
    CHECK(shadow.GetSlopeBias() == 0.0f);
    CHECK(shadow.GetNormalOffset() == ShadowmapSettings::MAX_NORMAL_OFFSET);
    CHECK(shadow.GetPcfRadius() == ShadowmapSettings::MAX_PCF_RADIUS);
    CHECK(shadow.GetCascadeBlend() == ShadowmapSettings::MAX_CASCADE_BLEND);

    // A non-finite value changes nothing, and none of them marks the settings dirty.
    shadow.SetNormalOffset(std::numeric_limits<float>::quiet_NaN());
    shadow.SetDepthBias(std::numeric_limits<float>::infinity());
    CHECK(shadow.GetNormalOffset() == ShadowmapSettings::MAX_NORMAL_OFFSET);
    CHECK(shadow.GetDepthBias() == ShadowmapSettings::MAX_DEPTH_BIAS);
    CHECK_FALSE(shadow.IsDirty());
}

TEST_CASE("Shadow sampling settings - every value round-trips through engine_settings.yaml and saves the same text twice")
{
    Settings written;
    auto&    shadow = *written.GetShadowmapSettings();
    shadow.SetDepthBias(0.001f);
    shadow.SetSlopeBias(0.01f);
    shadow.SetNormalOffset(2.5f);
    shadow.SetPcfRadius(3);
    shadow.SetCascadeBlend(0.25f);

    SettingsFiles files;
    REQUIRE(written.Save(PATH, files.Files));
    const std::string text = files.Read();
    CHECK(text.find("  depth_bias: 0.001\n  slope_bias: 0.01\n  normal_offset: 2.5\n  pcf_radius: 3\n"
                    "  cascade_blend: 0.25\n")
          != std::string::npos);

    Settings   read;
    LogCapture log;
    REQUIRE(read.Load(PATH, files.Files));
    CHECK(log.Lines("[WARN").empty());
    const auto& loaded = *read.GetShadowmapSettings();
    CHECK(loaded.GetDepthBias() == 0.001f);
    CHECK(loaded.GetSlopeBias() == 0.01f);
    CHECK(loaded.GetNormalOffset() == 2.5f);
    CHECK(loaded.GetPcfRadius() == 3u);
    CHECK(loaded.GetCascadeBlend() == 0.25f);

    SettingsFiles again;
    REQUIRE(read.Save(PATH, again.Files));
    CHECK(again.Read() == text);
}

TEST_CASE("Shadow sampling settings - a file without them loads the defaults")
{
    SettingsFiles files;
    files.Write("shadowmap:\n  size: 1024\n");
    Settings   settings;
    LogCapture log;
    REQUIRE(settings.Load(PATH, files.Files));
    CHECK(log.Lines("[WARN").empty());
    CHECK(settings.GetShadowmapSettings()->GetShadowmapSize() == 1024u);
    CheckDefaults(*settings.GetShadowmapSettings());
}

TEST_CASE("Shadow sampling settings - each bad value gives one warning naming it, the rest of the file still read")
{
    struct Case
    {
        const char* Line;
        const char* Named;
        // What the value is afterwards: the default when unreadable, clamped when out of range.
        float (*Value)(const ShadowmapSettings&);
        float Expected;
    };
    const auto depth  = [](const ShadowmapSettings& s) { return s.GetDepthBias(); };
    const auto slope  = [](const ShadowmapSettings& s) { return s.GetSlopeBias(); };
    const auto offset = [](const ShadowmapSettings& s) { return s.GetNormalOffset(); };
    const auto pcf    = [](const ShadowmapSettings& s) { return static_cast<float>(s.GetPcfRadius()); };
    const auto blend  = [](const ShadowmapSettings& s) { return s.GetCascadeBlend(); };
    const Case cases[] = {
        { "depth_bias: lots", "shadowmap.depth_bias", depth, ShadowmapSettings::DEFAULT_DEPTH_BIAS },
        { "depth_bias: 0.2", "shadowmap.depth_bias", depth, ShadowmapSettings::MAX_DEPTH_BIAS },
        { "slope_bias: -0.5", "shadowmap.slope_bias", slope, 0.0f },
        { "slope_bias: [1, 2]", "shadowmap.slope_bias", slope, ShadowmapSettings::DEFAULT_SLOPE_BIAS },
        { "normal_offset: .nan", "shadowmap.normal_offset", offset, ShadowmapSettings::DEFAULT_NORMAL_OFFSET },
        { "normal_offset: 50", "shadowmap.normal_offset", offset, ShadowmapSettings::MAX_NORMAL_OFFSET },
        { "pcf_radius: 1.5", "shadowmap.pcf_radius", pcf, static_cast<float>(ShadowmapSettings::DEFAULT_PCF_RADIUS) },
        { "pcf_radius: 7", "shadowmap.pcf_radius", pcf, static_cast<float>(ShadowmapSettings::MAX_PCF_RADIUS) },
        { "pcf_radius: -1", "shadowmap.pcf_radius", pcf, 0.0f },
        { "cascade_blend: 0.9", "shadowmap.cascade_blend", blend, ShadowmapSettings::MAX_CASCADE_BLEND },
    };
    for (const Case& bad : cases)
    {
        CAPTURE(bad.Line);
        SettingsFiles files;
        files.Write(std::string("shadowmap:\n  size: 1024\n  ") + bad.Line + "\nlayers:\n  3: Water\n");
        Settings   settings;
        LogCapture log;
        REQUIRE(settings.Load(PATH, files.Files));
        const auto warnings = log.Lines("[WARN");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings[0].find(bad.Named) != std::string::npos);
        CHECK(bad.Value(*settings.GetShadowmapSettings()) == bad.Expected);
        CHECK(settings.GetShadowmapSettings()->GetShadowmapSize() == 1024u);
        CHECK(settings.GetLayerSettings()->GetLayerName(3) == "Water");
    }
}

TEST_CASE("Shadow sampling settings - the shipped FeatureTest and template files hold the defaults and save as written")
{
    for (const char* folder : { "Projects/FeatureTest", "Templates/Empty" })
    {
        CAPTURE(folder);
        SettingsFiles shipped(RepositoryRoot() / folder);
        const std::string text = shipped.Read();
        REQUIRE(text.find("  depth_bias: 0.0005\n  slope_bias: 0.002\n  normal_offset: 1\n  pcf_radius: 1\n"
                          "  cascade_blend: 0.1\n")
                != std::string::npos);

        Settings   settings;
        LogCapture log;
        REQUIRE(settings.Load(PATH, shipped.Files));
        CHECK(log.Lines("[WARN").empty());
        CheckDefaults(*settings.GetShadowmapSettings());

        SettingsFiles copy;
        REQUIRE(settings.Save(PATH, copy.Files));
        CHECK(copy.Read() == text);
    }
}
