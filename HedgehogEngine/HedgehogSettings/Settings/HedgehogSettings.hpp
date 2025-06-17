#pragma once

#include <memory>

namespace HedgehogSettings
{
	class ShadowmapSettings;

	class Settings
	{
	public:
		Settings();
		~Settings();

		Settings(const Settings&) = delete;
		Settings(Settings&&) = delete;
		Settings& operator=(const Settings&) = delete;
		Settings& operator=(Settings&&) = delete;

		std::unique_ptr<ShadowmapSettings>& GetShadowmapSettings();
		const std::unique_ptr<ShadowmapSettings>& GetShadowmapSettings() const;
		bool IsDirty() const;
		void CleanDirtyState();

	private:
		std::unique_ptr<ShadowmapSettings> m_ShadowmapSettings;

	};
}


