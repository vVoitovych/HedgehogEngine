#include "CpuProfiler.hpp"
#include "CpuTimeStamp.hpp"

#include "Logger/Logger.hpp"

namespace Context
{
	CpuProfiler& CpuProfiler::Instance()
	{
		static CpuProfiler instance;
		return instance;
	}

	void CpuProfiler::StartTimeStamp(const std::string& name)
	{
		auto node = std::make_unique<CpuTimeStampNode>(name);
		m_NodesStack.push(std::move(node));
	}

	void CpuProfiler::EndTimeStamp(const std::string& name)
	{
		if (m_NodesStack.empty())
		{
			LOGERROR("EndTimeStamp('", name, "') called with no matching StartTimeStamp!");
			return;
		}

		auto& node = m_NodesStack.top();
		if (node->GetName() != name) 
		{
			LOGERROR("Mismatched EndTimeStamp! Expected '", node->GetName(), "', got '", name);
			return;
		}

		node->End();
		if (m_NodesStack.empty())
		{
			m_Nodes.push_back(std::move(node));
		}
		else
		{
			m_NodesStack.top()->AddChild(std::move(node));
		}
		m_NodesStack.pop();
	}

	void CpuProfiler::FinalizeTimeStamps()
	{
		while (!m_NodesStack.empty())
		{
			auto& node = m_NodesStack.top();
			node->End();
			if (m_NodesStack.empty())
			{
				m_Nodes.push_back(std::move(node));
			}
			else
			{
				m_NodesStack.top()->AddChild(std::move(node));
			}
			m_NodesStack.pop();
		}
		m_ReadyNodes = std::move(m_Nodes);
	}

	std::vector<std::unique_ptr<CpuTimeStampNode>>& CpuProfiler::GetTimeStamps()
	{
		std::vector<std::unique_ptr<CpuTimeStampNode>>&& result = std::move(m_ReadyNodes);
		return result;
	}
}

void START_TIME_STAMP(const std::string& name)
{
#ifdef DEBUG
	Context::CpuProfiler::Instance().StartTimeStamp(name);
#endif
}

void END_TIME_STAMP(const std::string& name)
{
#ifdef DEBUG
	Context::CpuProfiler::Instance().EndTimeStamp(name);
#endif
}

void FINALIZE_TIME_STAMP()
{
#ifdef DEBUG
	Context::CpuProfiler::Instance().FinalizeTimeStamps();
#endif
}

std::vector<std::unique_ptr<Context::CpuTimeStampNode>>& GET_TIME_STAMP()
{
	std::vector<std::unique_ptr<Context::CpuTimeStampNode>> result;
#ifdef DEBUG
	result = std::move(Context::CpuProfiler::Instance().GetTimeStamps());
#endif
	return result;
}
