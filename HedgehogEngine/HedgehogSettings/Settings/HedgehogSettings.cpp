#include "HedgehogSettings.hpp"

#include "ShadowmapingSettings.hpp"

namespace HedgehogSettings
{
	Settings::Settings()
	{
		m_ShadowmapSettings = std::make_unique<ShadowmapSettings>();
	}

	Settings::~Settings()
	{
	}
	std::unique_ptr<ShadowmapSettings>& Settings::GetShadowmapSettings()
	{
		return m_ShadowmapSettings;
	}
}

