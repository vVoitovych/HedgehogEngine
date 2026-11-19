#pragma once

#include "doctest/doctest/doctest.h"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace
{
    constexpr uint32_t RATE           = 48000;
    constexpr uint32_t CLIP_FRAMES    = 4800; // 0.1 s
    constexpr uint64_t CHUNK_FRAMES   = 480;

    void Append(std::string& out, uint32_t value, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
        {
            out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
        }
    }

    // A mono 16-bit WAV of a 440 Hz sine at half amplitude.
    std::string MakeWav(uint32_t frames)
    {
        std::string data;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double  t      = static_cast<double>(i) / RATE;
            const int16_t sample = static_cast<int16_t>(16384.0 * std::sin(2.0 * std::numbers::pi * 440.0 * t));
            Append(data, static_cast<uint16_t>(sample), 2);
        }
        std::string wav = "RIFF";
        Append(wav, static_cast<uint32_t>(36 + data.size()), 4);
        wav += "WAVEfmt ";
        Append(wav, 16, 4);       // fmt chunk size
        Append(wav, 1, 2);        // PCM
        Append(wav, 1, 2);        // mono
        Append(wav, RATE, 4);
        Append(wav, RATE * 2, 4); // bytes per second
        Append(wav, 2, 2);        // bytes per frame
        Append(wav, 16, 2);       // bits per sample
        wav += "data";
        Append(wav, static_cast<uint32_t>(data.size()), 4);
        return wav + data;
    }

    // A started engine without a device over a temporary assets:// holding the test clips.
    struct AudioWorld
    {
        TempDir               Dir;
        FS::FileSystemManager Files;
        HA::AudioEngine       Engine;

        AudioWorld()
        {
            Dir.WriteFile("Audio/beep.wav", MakeWav(CLIP_FRAMES));
            Dir.WriteFile("Audio/broken.wav", "this is not audio");
            auto fileSystem = std::make_unique<FS::FileSystem>();
            fileSystem->RegisterPath("assets://", Dir.Path());
            REQUIRE(Files.Register(std::move(fileSystem)));

            HA::AudioEngineDesc desc;
            desc.NoDevice = true;
            REQUIRE(Engine.Init(desc));
        }

        HA::AudioClipId Beep() { return Engine.LoadClip("assets://Audio/beep.wav", Files); }

        // Mixes frames and sums each channel's magnitude.
        void Mix(uint64_t frames, double* left = nullptr, double* right = nullptr)
        {
            std::vector<float> buffer(CHUNK_FRAMES * 2);
            for (uint64_t done = 0; done < frames; done += CHUNK_FRAMES)
            {
                REQUIRE(Engine.ReadFrames(buffer.data(), CHUNK_FRAMES) == CHUNK_FRAMES);
                for (uint64_t i = 0; i < CHUNK_FRAMES; ++i)
                {
                    if (left)
                    {
                        *left += std::abs(buffer[i * 2]);
                    }
                    if (right)
                    {
                        *right += std::abs(buffer[i * 2 + 1]);
                    }
                }
            }
        }
    };
}
