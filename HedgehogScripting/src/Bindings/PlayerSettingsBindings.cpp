#include "Bindings.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/PlayerSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "Logger/api/Logger.hpp"

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogSettings::PlayerSettings;
        using HedgehogSettings::ProjectSettings;

        std::string TypeName(const sol::object& value)
        {
            return sol::type_name(value.lua_state(), value.get_type());
        }

        std::string JoinNames(std::span<const std::string_view> names)
        {
            std::string joined;
            for (size_t i = 0; i < names.size(); ++i)
            {
                joined += (i == 0 ? "" : (i + 1 == names.size() ? " or " : ", ")) + std::string(names[i]);
            }
            return joined;
        }

        // The index of value's name in names; a script error naming the field and every name otherwise.
        size_t ReadName(const sol::object& value, std::span<const std::string_view> names, const std::string& field)
        {
            if (value.is<std::string>())
            {
                const std::string name = value.as<std::string>();
                for (size_t i = 0; i < names.size(); ++i)
                {
                    if (names[i] == name)
                    {
                        return i;
                    }
                }
            }
            const std::string what =
                value.is<std::string>() ? "'" + value.as<std::string>() + "'" : "a " + TypeName(value);
            throw std::runtime_error("PlayerSettings." + field + ": " + what + " is not " + JoinNames(names));
        }

        double ReadNumber(const sol::object& value, const std::string& field)
        {
            if (!value.is<double>())
            {
                throw std::runtime_error("PlayerSettings." + field + " takes a number, not a " + TypeName(value));
            }
            if (!std::isfinite(value.as<double>()))
            {
                throw std::runtime_error("PlayerSettings." + field + " takes a finite number");
            }
            return value.as<double>();
        }

        float ReadVolume(const sol::object& value, const std::string& field)
        {
            const double volume = ReadNumber(value, field);
            if (volume < 0.0 || volume > 1.0)
            {
                std::ostringstream text;
                text << "PlayerSettings." << field << ": " << volume << " is not a volume from 0 to 1";
                throw std::runtime_error(text.str());
            }
            return static_cast<float>(volume);
        }

        // A whole number in [min, max], or exactly `alsoAllowed`.
        int ReadWhole(const sol::object& value, const std::string& field, int min, int max, int alsoAllowed,
                      const char* range)
        {
            const double number = ReadNumber(value, field);
            if (number != std::floor(number) || ((number < min || number > max) && number != alsoAllowed))
            {
                std::ostringstream text;
                text << "PlayerSettings." << field << ": " << number << " is not " << range;
                throw std::runtime_error(text.str());
            }
            return static_cast<int>(number);
        }

        // The bus a name gives; nullopt for Master.
        std::optional<size_t> ReadBus(const sol::object& value)
        {
            if (value.is<std::string>() && value.as<std::string>() == "Master")
            {
                return std::nullopt;
            }
            static constexpr std::array<std::string_view, HedgehogSettings::PLAYER_AUDIO_BUS_COUNT + 1> NAMES = {
                "Master", "Music", "Sfx", "Voice", "Ui"
            };
            static_assert(NAMES[1] == HedgehogSettings::PLAYER_AUDIO_BUSES[0] &&
                          NAMES[4] == HedgehogSettings::PLAYER_AUDIO_BUSES[3]);
            return ReadName(value, NAMES, "bus") - 1;
        }

        // What a menu edits: changes wait in pending until apply().
        struct PlayerSettingsAccess
        {
            HedgehogEngine::EngineContext&  Context;
            std::optional<PlayerSettings>&  Pending;
            bool                            Warned = false;

            const PlayerSettings& Read() const { return Pending ? *Pending : Context.GetPlayerSettings(); }

            // Outside Play (a script's top level also runs when the editor describes it) nothing
            // changes, and the first call says why.
            bool InPlay(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                {
                    return true;
                }
                if (!Warned)
                {
                    LOGWARNING("[Script] Player settings change only in Play mode; ", call, " did nothing.");
                }
                Warned = true;
                return false;
            }

            PlayerSettings* Write(const char* call)
            {
                if (!InPlay(call))
                {
                    return nullptr;
                }
                if (!Pending)
                {
                    Pending = Context.GetPlayerSettings();
                }
                return &*Pending;
            }
        };
    }

    void RegisterPlayerSettings(sol::state&                                      lua,
                                HedgehogEngine::EngineContext&                    context,
                                std::optional<HedgehogSettings::PlayerSettings>& pending)
    {
        sol::table table = lua.create_named_table("PlayerSettings");
        sol::table meta  = lua.create_table();
        auto       access = std::make_shared<PlayerSettingsAccess>(PlayerSettingsAccess{ context, pending });

        meta.set_function("getBusVolume", [access](const sol::object& bus)
        {
            const PlayerSettings& settings = access->Read();
            const auto            index    = ReadBus(bus);
            return index ? settings.BusVolumes[*index] : settings.MasterVolume;
        });
        meta.set_function("setBusVolume", [access](const sol::object& bus, const sol::object& value)
        {
            const auto  index  = ReadBus(bus);
            const float volume = ReadVolume(value, "setBusVolume");
            if (PlayerSettings* settings = access->Write("PlayerSettings.setBusVolume"))
            {
                (index ? settings->BusVolumes[*index] : settings->MasterVolume) = volume;
            }
        });
        meta.set_function("getResolutions", [&context](sol::this_state state)
        {
            sol::state_view view(state);
            sol::table      list = view.create_table();
            for (const HW::VideoMode& mode : context.GetApplicationRequests().VideoModes)
            {
                list.add(view.create_table_with("width", mode.Width, "height", mode.Height,
                                                "refreshRate", mode.RefreshRate));
            }
            return list;
        });
        meta.set_function("getQualityLevels", [](sol::this_state state)
        {
            sol::state_view view(state);
            sol::table      list = view.create_table();
            for (const std::string_view name : HedgehogSettings::PLAYER_QUALITY_NAMES)
            {
                list.add(std::string(name));
            }
            return list;
        });
        meta.set_function("hasPendingChanges", [access]() { return access->Pending.has_value(); });
        meta.set_function("apply", [access]()
        {
            if (!access->InPlay("PlayerSettings.apply") || !access->Pending)
            {
                return;
            }
            access->Context.GetPlayerSettings() = *access->Pending;
            access->Pending.reset();
            access->Context.MarkPlayerSettingsChanged();
        });
        meta.set_function("save", [access]()
        {
            if (!access->InPlay("PlayerSettings.save"))
            {
                return false;
            }
            return HedgehogSettings::SavePlayerSettings(access->Context.GetFileSystem(),
                                                        access->Context.GetPlayerSettings());
        });
        meta.set_function("revert", [access]()
        {
            if (!access->InPlay("PlayerSettings.revert"))
            {
                return false;
            }
            HedgehogEngine::EngineContext& engine = access->Context;
            PlayerSettings                 saved =
                HedgehogSettings::MakeDefaultPlayerSettings(engine.GetSettings().GetProjectSettings());
            if (!HedgehogSettings::LoadPlayerSettings(engine.GetFileSystem(), saved))
            {
                return false;
            }
            access->Pending.reset();
            engine.GetPlayerSettings() = saved;
            engine.MarkPlayerSettingsChanged();
            return true;
        });

        // The table stays empty, so every read reaches __index and every write __newindex; the
        // functions live in the metatable, read from there so no closure holds a reference into the state.
        meta[sol::meta_function::index] = [access](sol::table self, const sol::object& key,
                                                   sol::this_state state) -> sol::object
        {
            if (!key.is<std::string>())
            {
                return sol::lua_nil;
            }
            const std::string     name     = key.as<std::string>();
            const PlayerSettings& settings = access->Read();
            if (name == "windowMode")
            {
                const size_t mode = static_cast<size_t>(settings.WindowMode);
                return sol::make_object(state, std::string(HedgehogSettings::PLAYER_WINDOW_MODE_NAMES[mode]));
            }
            if (name == "quality")
            {
                const size_t quality = static_cast<size_t>(settings.Quality);
                return sol::make_object(state, std::string(HedgehogSettings::PLAYER_QUALITY_NAMES[quality]));
            }
            if (name == "width")
            {
                return sol::make_object(state, settings.Width);
            }
            if (name == "height")
            {
                return sol::make_object(state, settings.Height);
            }
            if (name == "vsync")
            {
                return sol::make_object(state, settings.VSync);
            }
            if (name == "maxFps")
            {
                return sol::make_object(state, settings.MaxFps);
            }
            if (name == "masterVolume")
            {
                return sol::make_object(state, settings.MasterVolume);
            }
            return self[sol::metatable_key].get<sol::table>().raw_get<sol::object>(name);
        };
        meta[sol::meta_function::new_index] = [access](sol::table, const sol::object& key, const sol::object& value)
        {
            const std::string name = key.is<std::string>() ? key.as<std::string>() : "<key>";
            const std::string call = "PlayerSettings." + name;
            // Every value is checked before the Play check, so a wrong one is an error in Edit too.
            if (name == "windowMode")
            {
                const size_t mode = ReadName(value, HedgehogSettings::PLAYER_WINDOW_MODE_NAMES, name);
                if (PlayerSettings* settings = access->Write(call.c_str()))
                {
                    settings->WindowMode = static_cast<HedgehogSettings::PlayerWindowMode>(mode);
                }
            }
            else if (name == "quality")
            {
                const size_t quality = ReadName(value, HedgehogSettings::PLAYER_QUALITY_NAMES, name);
                if (PlayerSettings* settings = access->Write(call.c_str()))
                {
                    settings->Quality = static_cast<HedgehogSettings::PlayerQuality>(quality);
                }
            }
            else if (name == "width" || name == "height")
            {
                const int size = ReadWhole(value, name, static_cast<int>(ProjectSettings::MIN_WINDOW_SIZE),
                                           static_cast<int>(ProjectSettings::MAX_WINDOW_SIZE), 0,
                                           "a whole number from 64 to 16384, or 0 for the project's size");
                if (PlayerSettings* settings = access->Write(call.c_str()))
                {
                    (name == "width" ? settings->Width : settings->Height) = static_cast<uint32_t>(size);
                }
            }
            else if (name == "vsync")
            {
                if (!value.is<bool>())
                {
                    throw std::runtime_error("PlayerSettings.vsync takes a boolean, not a " + TypeName(value));
                }
                if (PlayerSettings* settings = access->Write(call.c_str()))
                {
                    settings->VSync = value.as<bool>();
                }
            }
            else if (name == "maxFps")
            {
                const int fps = ReadWhole(value, name, ProjectSettings::MIN_FRAME_RATE_CAP,
                                          ProjectSettings::MAX_FRAME_RATE_CAP, ProjectSettings::NO_FRAME_RATE_CAP,
                                          "a whole number from 15 to 1000, or 0 for no cap");
                if (PlayerSettings* settings = access->Write(call.c_str()))
                {
                    settings->MaxFps = fps;
                }
            }
            else if (name == "masterVolume")
            {
                const float volume = ReadVolume(value, name);
                if (PlayerSettings* settings = access->Write(call.c_str()))
                {
                    settings->MasterVolume = volume;
                }
            }
            else
            {
                throw std::runtime_error(call + " is not a field that can be set");
            }
        };
        // getmetatable gives this, and setmetatable refuses, so no script takes the table from the others.
        meta[sol::meta_function::metatable] = "PlayerSettings";
        table[sol::metatable_key]           = meta;
    }
}
