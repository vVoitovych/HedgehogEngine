#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace Renderer
{
    // Collects CPU timing samples per named zone while a capture is active
    // (Editor --benchmark). Outside a capture, sampling is a single bool check.
    class FrameStats
    {
    public:
        void BeginCapture();
        void EndCapture();
        bool IsCapturing() const { return m_Capturing; }

        void AddSample(const char* zoneName, double milliseconds);

        // Logs avg/min/max/p95 per zone, in first-seen order.
        void LogReport() const;

    private:
        struct ZoneSeries
        {
            std::string         Name;
            std::vector<double> SamplesMs;
        };

        ZoneSeries& findOrAddZone(const char* zoneName);

        std::vector<ZoneSeries> m_Zones;
        bool                    m_Capturing = false;
    };

    // RAII CPU timer feeding FrameStats; no-op when not capturing.
    class ScopedCpuSample
    {
    public:
        ScopedCpuSample(FrameStats& stats, const char* zoneName)
            : m_Stats(stats)
            , m_ZoneName(zoneName)
            , m_Active(stats.IsCapturing())
        {
            if (m_Active)
                m_Start = std::chrono::steady_clock::now();
        }

        ~ScopedCpuSample()
        {
            if (!m_Active)
                return;

            const auto end = std::chrono::steady_clock::now();
            m_Stats.AddSample(m_ZoneName,
                std::chrono::duration<double, std::milli>(end - m_Start).count());
        }

        ScopedCpuSample(const ScopedCpuSample&)            = delete;
        ScopedCpuSample& operator=(const ScopedCpuSample&) = delete;
        ScopedCpuSample(ScopedCpuSample&&)                 = delete;
        ScopedCpuSample& operator=(ScopedCpuSample&&)      = delete;

    private:
        FrameStats&                           m_Stats;
        const char*                           m_ZoneName;
        bool                                  m_Active;
        std::chrono::steady_clock::time_point m_Start;
    };
}
