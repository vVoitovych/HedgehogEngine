#pragma once

#include "HedgehogMath/Vector.hpp"

namespace WinManager
{
	struct Controls
	{
		bool IsPressedW = false;
		bool IsPressedA = false;
		bool IsPressedS = false;
		bool IsPressedD = false;
		bool IsPressedQ = false;
		bool IsPressedE = false;

		bool IsPressedControl = false;

		bool IsLeftMouseButton = false;
		bool IsMiddleMouseButton = false;
		bool IsRightMouseButton = false;

		HM::Vector2 MousePos = HM::Vector2(0.0f, 0.0f);
		HM::Vector2 MouseDelta = HM::Vector2(0.0f, 0.0f);
		HM::Vector2 ScrollDelta = HM::Vector2(0.0f, 0.0f);

	};
}


