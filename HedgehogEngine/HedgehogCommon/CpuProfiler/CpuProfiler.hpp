#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stack>

namespace Context
{
	class CpuTimeStampNode;

	class CpuProfiler
	{
	public:
		static CpuProfiler& Instance();

	public:
		void StartTimeStamp(const std::string& name);
		void EndTimeStamp(const std::string& name);

		void FinalizeTimeStamps();

		std::vector<std::unique_ptr<CpuTimeStampNode>>& GetTimeStamps();

	private:
		CpuProfiler() = default;
		~CpuProfiler() = default;
		CpuProfiler(const CpuProfiler&) = delete;
		CpuProfiler(CpuProfiler&&) = delete;
		CpuProfiler& operator=(const CpuProfiler&) = delete;
		CpuProfiler& operator=(CpuProfiler&&) = delete;

	private:
		std::stack< std::unique_ptr<CpuTimeStampNode>> m_NodesStack;

		std::vector<std::unique_ptr<CpuTimeStampNode>> m_Nodes;
		std::vector<std::unique_ptr<CpuTimeStampNode>> m_ReadyNodes;

	};


}

void START_TIME_STAMP(const std::string& name);
void END_TIME_STAMP(const std::string& name);
void FINALIZE_TIME_STAMP();
std::vector<std::unique_ptr<Context::CpuTimeStampNode>>& GET_TIME_STAMP();


