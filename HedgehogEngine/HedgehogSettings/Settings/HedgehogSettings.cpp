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
	const std::unique_ptr<ShadowmapSettings>& Settings::GetShadowmapSettings() const
	{
		return m_ShadowmapSettings;
	}
	bool Settings::IsDirty() const
	{
		return m_ShadowmapSettings->IsDirty();
	}
	void Settings::CleanDirtyState()
	{
		m_ShadowmapSettings->CleanDirtyState();
	}
}

