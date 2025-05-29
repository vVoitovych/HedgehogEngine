#include "ShadowmapingSettings.hpp"

#include <algorithm>

namespace HedgehogSettings
{
	ShadowmapSettings::ShadowmapSettings()
	{
	}

	int ShadowmapSettings::GetShadowmapSize() const
	{
		return m_ShadowmapSize;
	}

	void ShadowmapSettings::SetShadowmapSize(int size)
	{
		m_ShadowmapSize = std::max(1, std::min(size, 4096));
		m_IsDirty = true;
	}

	int ShadowmapSettings::GetCascadesCount() const
	{
		return m_CascadesCount;
	}

	void ShadowmapSettings::SetCascadesCount(int cascadesCount)
	{
		m_CascadesCount = std::max(1, std::min(cascadesCount, 4));
	}

	float ShadowmapSettings::GetSplit1() const
	{
		return m_Split1;
	}

	void ShadowmapSettings::SetSplit1(float val)
	{
		m_Split1 = std::max(0.0f, std::min(val, m_Split2 - 0.1f));
	}

	float ShadowmapSettings::GetSplit2() const
	{
		return m_Split2;
	}

	void ShadowmapSettings::SetSplit2(float val)
	{
		m_Split2 = std::max(m_Split1 + 0.1f, std::min(val, m_Split3 - 0.1f));
	}

	float ShadowmapSettings::GetSplit3() const
	{
		return m_Split3;
	}

	void ShadowmapSettings::SetSplit3(float val)
	{
		m_Split3 = std::max(m_Split2 + 0.1f, std::min(val, 100.0f));
	}

	void ShadowmapSettings::SetDefaultSplits()
	{
		if (m_CascadesCount == 1)
		{
			m_Split1 = 100.0f;
			m_Split2 = 100.0f;
			m_Split3 = 100.0f;
		}
		else if (m_CascadesCount == 2)
		{
			m_Split1 = 35.0f;
			m_Split2 = 100.0f;
			m_Split3 = 100.0f;
		}
		else if (m_CascadesCount == 3)
		{
			m_Split1 = 20.0f;
			m_Split2 = 60.0f;
			m_Split3 = 100.0f;
		}
		else if(m_CascadesCount == 4)
		{
			m_Split1 = 10.0f;
			m_Split2 = 25.0f;
			m_Split3 = 50.0f;
		}
	}

	bool ShadowmapSettings::IsDirty() const
	{
		return m_IsDirty;
	}

	void ShadowmapSettings::CleanDirtyState()
	{
		m_IsDirty = false;
	}

}

