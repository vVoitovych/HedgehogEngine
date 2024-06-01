#pragma once

#include <glm/glm.hpp>

namespace Hedgehog
{
	namespace Wrappers
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

			glm::vec2 MousePos = glm::vec2(0.0f, 0.0f);
			glm::vec2 MouseDelta = glm::vec2(0.0f, 0.0f);
			glm::vec2 ScrollDelta = glm::vec2(0.0f, 0.0f);

		};
	}
}


