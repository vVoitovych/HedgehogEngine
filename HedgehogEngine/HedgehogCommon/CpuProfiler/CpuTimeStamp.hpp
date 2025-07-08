#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>

namespace Context
{
    class CpuTimeStampNode
    {
    public:
        CpuTimeStampNode(const std::string& name);

        void End();

        double DurationMs() const;

        void AddChild(std::unique_ptr<CpuTimeStampNode>&& node);

        const std::string& GetName() const;
        const  std::vector<std::unique_ptr<CpuTimeStampNode>>& GetChildren() const;

    private:
        std::string m_Name;
        std::chrono::high_resolution_clock::time_point m_StartTime;
        std::chrono::high_resolution_clock::time_point m_EndTime;
        std::vector<std::unique_ptr<CpuTimeStampNode>> m_Children;


    };


}

