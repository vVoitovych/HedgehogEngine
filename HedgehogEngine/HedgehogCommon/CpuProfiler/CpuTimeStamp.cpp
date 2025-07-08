#include "CpuTimeStamp.hpp"

namespace Context
{
	CpuTimeStampNode::CpuTimeStampNode(const std::string& name) 
        : m_Name(name)
    {
        m_StartTime = std::chrono::high_resolution_clock::now();
    }

    void CpuTimeStampNode::End()
    {
        m_EndTime = std::chrono::high_resolution_clock::now();
    }

    double CpuTimeStampNode::DurationMs() const
    {
        return std::chrono::duration<double, std::milli>(m_EndTime - m_StartTime).count();
    }

    void CpuTimeStampNode::AddChild(std::unique_ptr<CpuTimeStampNode>&& node)
    {
        m_Children.push_back(std::move(node));
    }

    const std::string& CpuTimeStampNode::GetName() const
    {
        return m_Name;
    }

    const std::vector<std::unique_ptr<CpuTimeStampNode>>& CpuTimeStampNode::GetChildren() const
    {
        return m_Children;
    }


}


