#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AudioSystem.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::AudioSourceComponent;
        using AudioSourceRef = ScriptComponentRef<AudioSourceComponent>;

        constexpr std::string_view ASSETS_PREFIX = "assets://";

        float RequireFinite(float value, const char* what)
        {
            if (!std::isfinite(value))
                throw std::runtime_error(std::string(what) + " must be a finite number");
            return value;
        }

        // A bus as a name ("Music", ..., or "Master" when allowed) or an AudioBus value; nullopt is Master.
        std::optional<HA::AudioBus> ReadBus(const sol::object& value, bool allowMaster)
        {
            std::optional<HA::AudioBus> bus;
            bool                        found = false;
            if (value.is<std::string>())
            {
                const std::string name = value.as<std::string>();
                if (allowMaster && name == "Master")
                {
                    return std::nullopt;
                }
                bus   = HA::FindBus(name);
                found = bus.has_value();
                if (!found && name == "Master")
                {
                    throw std::runtime_error("a one-shot plays on a bus, not Master");
                }
            }
            else if (value.is<double>())
            {
                const double number = value.as<double>();
                if (number >= 0.0 && number < static_cast<double>(HA::AUDIO_BUS_COUNT) && number == std::floor(number))
                {
                    bus   = static_cast<HA::AudioBus>(static_cast<int>(number));
                    found = true;
                }
            }
            if (!found)
            {
                std::string what = "a " + std::string(sol::type_name(value.lua_state(), value.get_type()));
                if (value.is<std::string>())
                {
                    what = "\"" + value.as<std::string>() + "\"";
                }
                else if (value.is<double>())
                {
                    std::ostringstream number;
                    number << value.as<double>();
                    what = number.str();
                }
                throw std::runtime_error(what + " is not an audio bus (" +
                                         std::string(allowMaster ? "Master, " : "") + "Music, Sfx, Voice, Ui)");
            }
            return bus;
        }

        // Audio plays only in Play mode (a script's top level also runs when the editor describes
        // it): outside it a call does nothing and the first one logs why.
        struct PlayGuard
        {
            HedgehogEngine::EngineContext& Context;
            bool                           Warned = false;

            bool Allows(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                    return true;
                if (!Warned)
                    LOGWARNING("[Script] Audio plays only in Play mode;", std::string(call), "did nothing.");
                Warned = true;
                return false;
            }
        };

        void RegisterAudioSource(sol::state& lua, HedgehogEngine::EngineContext& context,
                                 const std::shared_ptr<PlayGuard>& guard)
        {
            ECS::ECS&                    ecs    = context.GetECS();
            HedgehogEngine::AudioSystem& system = *context.GetAudioSystem();
            HA::AudioEngine&             audio  = context.GetAudioEngine();
            lua.new_usertype<AudioSourceRef>(
                "AudioSource", sol::no_constructor,
                // play(): the source's clip from its start, at the entity's place, replacing a sound
                // it was playing. Returns whether a sound started (false for a clip that does not
                // load, logged once by the engine).
                "play", [&ecs, &system, guard](const AudioSourceRef& ref)
                {
                    (void)Resolve(ecs, ref);
                    return guard->Allows("AudioSource:play()") && system.Play(ecs, ref.Entity.Id).IsValid();
                },
                "stop", [&ecs, &audio, guard](const AudioSourceRef& ref)
                {
                    AudioSourceComponent& source = Resolve(ecs, ref);
                    if (!guard->Allows("AudioSource:stop()"))
                        return;
                    audio.Stop(source.Sound);
                    source.Sound = {};
                },
                // False once stopped, paused, or a clip that does not loop has ended.
                "isPlaying", [&ecs, &audio](const AudioSourceRef& ref) { return audio.IsPlaying(Resolve(ecs, ref).Sound); },
                // The path under assets://, prefix optional; the next play() loads it.
                "clip", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).Clip; },
                    [&ecs](const AudioSourceRef& ref, const std::string& path) { Resolve(ecs, ref).Clip = ToAssetPath(path, "an audio clip", "audio clips"); }),
                // Volume and pitch apply to a playing sound from the next frame, the others at the
                // next play().
                "volume", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).Volume; },
                    [&ecs](const AudioSourceRef& ref, float volume)
                    { Resolve(ecs, ref).Volume = std::max(RequireFinite(volume, "a volume"), 0.0f); }),
                "pitch", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).Pitch; },
                    [&ecs](const AudioSourceRef& ref, float pitch)
                    {
                        if (!(RequireFinite(pitch, "a pitch") > 0.0f))
                            throw std::runtime_error("a pitch must be above 0");
                        Resolve(ecs, ref).Pitch = pitch;
                    }),
                "loop", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).Loop; },
                    [&ecs](const AudioSourceRef& ref, bool loop) { Resolve(ecs, ref).Loop = loop; }),
                "stream", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).Stream; },
                    [&ecs](const AudioSourceRef& ref, bool stream) { Resolve(ecs, ref).Stream = stream; }),
                "spatial", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).Spatial; },
                    [&ecs](const AudioSourceRef& ref, bool spatial) { Resolve(ecs, ref).Spatial = spatial; }),
                "playOnStart", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).PlayOnStart; },
                    [&ecs](const AudioSourceRef& ref, bool playOnStart) { Resolve(ecs, ref).PlayOnStart = playOnStart; }),
                "minDistance", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).MinDistance; },
                    [&ecs](const AudioSourceRef& ref, float distance)
                    { Resolve(ecs, ref).MinDistance = std::max(RequireFinite(distance, "a distance"), 0.0f); }),
                "bus", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return static_cast<int>(Resolve(ecs, ref).Bus); },
                    [&ecs](const AudioSourceRef& ref, const sol::object& value)
                    {
                        // As the bus arguments of the Audio table take it, so every place reads a bus alike.
                        const HA::AudioBus bus = *ReadBus(value, false);
                        Resolve(ecs, ref).Bus  = static_cast<HedgehogEngine::AudioSourceBus>(bus);
                    }),
                "maxDistance", sol::property(
                    [&ecs](const AudioSourceRef& ref) { return Resolve(ecs, ref).MaxDistance; },
                    [&ecs](const AudioSourceRef& ref, float distance)
                    { Resolve(ecs, ref).MaxDistance = std::max(RequireFinite(distance, "a distance"), 0.0f); }),
                sol::meta_function::to_string, ToText<AudioSourceComponent>("AudioSource"));
        }

        void RegisterAudioTable(sol::state& lua, HedgehogEngine::EngineContext& context,
                                const std::shared_ptr<PlayGuard>& guard)
        {
            HA::AudioEngine&             audio = context.GetAudioEngine();
            const FS::FileSystemManager& files = context.GetFileSystem();
            sol::table                   table = lua.create_named_table("Audio");

            lua.new_enum("AudioBus",
                         "Music", static_cast<int>(HA::AudioBus::Music),
                         "Sfx",   static_cast<int>(HA::AudioBus::Sfx),
                         "Voice", static_cast<int>(HA::AudioBus::Voice),
                         "Ui",    static_cast<int>(HA::AudioBus::Ui));

            // playOneShot(clip, position?, volume?, bus?): plays a clip once, in 2D, or in 3D at position
            // (a Vector3); the engine frees it when it ends. Returns whether it started.
            table["playOneShot"] = [&audio, &files, guard](const std::string& clip, sol::object position,
                                                           sol::optional<float> volume, sol::object bus)
            {
                const std::string path = std::string(ASSETS_PREFIX) + ToAssetPath(clip, "an audio clip", "audio clips");
                HA::PlayParams    params;
                if (position.valid() && position != sol::lua_nil)
                {
                    if (!position.is<HM::Vector3>())
                        throw std::runtime_error("a one-shot's position must be a Vector3");
                    params.Spatial  = true;
                    params.Position = position.as<HM::Vector3>();
                }
                if (volume)
                    params.Volume = std::max(RequireFinite(*volume, "a volume"), 0.0f);
                if (bus.valid() && bus != sol::lua_nil)
                {
                    params.Bus = *ReadBus(bus, false);
                }
                if (!guard->Allows("Audio.playOneShot()"))
                    return false;
                return audio.Play(audio.LoadClip(path, files), params).IsValid();
            };
            table["setMasterVolume"] = [&audio, guard](float volume)
            {
                RequireFinite(volume, "a volume");
                if (guard->Allows("Audio.setMasterVolume()"))
                    audio.SetMasterVolume(volume);
            };
            table["getMasterVolume"] = [&audio]() { return audio.GetMasterVolume(); };
            // setBusVolume(bus, volume) / getBusVolume(bus): a bus by name or AudioBus value, or "Master".
            table["setBusVolume"] = [&audio, guard](sol::object bus, float volume)
            {
                const std::optional<HA::AudioBus> target = ReadBus(bus, true);
                RequireFinite(volume, "a volume");
                if (!guard->Allows("Audio.setBusVolume()"))
                {
                    return;
                }
                if (target)
                {
                    audio.SetBusVolume(*target, volume);
                }
                else
                {
                    audio.SetMasterVolume(volume);
                }
            };
            table["getBusVolume"] = [&audio](sol::object bus)
            {
                const std::optional<HA::AudioBus> target = ReadBus(bus, true);
                return target ? audio.GetBusVolume(*target) : audio.GetMasterVolume();
            };
        }
    }

    void RegisterAudio(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        auto guard = std::make_shared<PlayGuard>(PlayGuard{ context });
        RegisterAudioSource(lua, context, guard);
        RegisterAudioTable(lua, context, guard);
    }
}
