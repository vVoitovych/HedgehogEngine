#pragma once

namespace HedgehogSettings
{
	class ShadowmapSettings
	{
	public:
		ShadowmapSettings();
		
		~ShadowmapSettings() = default;
		ShadowmapSettings(const ShadowmapSettings&) = delete;
		ShadowmapSettings(ShadowmapSettings&&) = delete;
		ShadowmapSettings& operator=(const ShadowmapSettings&) = delete;
		ShadowmapSettings& operator=(ShadowmapSettings&&) = delete;

		int GetShadowmapSize() const;
		void SetShadowmapSize(int size);

		int GetCascadesCount() const;
		void SetCascadesCount(int cascadesCount);

		float GetSplit1() const;
		void SetSplit1(float val);

		float GetSplit2() const;
		void SetSplit2(float val);

		float GetSplit3() const;
		void SetSplit3(float val);
	private:
		int m_ShadowmapSize = 2048;
		int m_CascadesCount = 4;

		float m_Split1 = 10.0f;
		float m_Split2 = 25.0f;
		float m_Split3 = 50.0f;


	};
}


