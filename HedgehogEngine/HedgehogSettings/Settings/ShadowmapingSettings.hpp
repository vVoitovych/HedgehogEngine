#pragma once

#include <cstdint>

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

		uint32_t GetShadowmapSize() const;
		void SetShadowmapSize(uint32_t size);

		uint32_t GetCascadesCount() const;
		void SetCascadesCount(uint32_t cascadesCount);

		float GetCascadeSplitLambda() const;
		void SetCascadeSplitLambda(float val);

		float GetSplit1() const;
		void SetSplit1(float val);

		float GetSplit2() const;
		void SetSplit2(float val);

		float GetSplit3() const;
		void SetSplit3(float val);

		void SetDefaultSplits();

		bool IsDirty() const;
		void CleanDirtyState();

	private:
		uint32_t m_ShadowmapSize = 2048;
		uint32_t m_CascadesCount = 4;

		float m_CascadeSplitLambda = 0.95f;

		float m_Split1 = 10.0f;
		float m_Split2 = 25.0f;
		float m_Split3 = 50.0f;

		bool m_IsDirty = false;
	};
}


