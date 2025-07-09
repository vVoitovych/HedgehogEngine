#include "CpuTimeStamp.hpp"

namespace Context
{
	CpuTimeStampNode::CpuTimeStampNode(const std::string& name) 
        : m_Name(name)
        , m_Duration(0.0)
    {
        m_StartTime = std::chrono::high_resolution_clock::now();
    }

    void CpuTimeStampNode::End()
    {
        m_Duration = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_StartTime).count();
    }

    double CpuTimeStampNode::DurationMs() const
    {
        return m_Duration;
    }

    void CpuTimeStampNode::AddChild(std::unique_ptr<CpuTimeStampNode>&& node)
    {
        m_Children.push_back(std::move(node));
    }

    void CpuTimeStampNode::AccumulateDuration(double val)
    {
        m_Duration += val;
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


