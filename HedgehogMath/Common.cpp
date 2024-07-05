#include "Common.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

namespace HM
{
	float ToRadians(float angle)
	{
		return (float)M_PI / 180.0f * angle;
	}
	double ToRadians(double angle)
	{
		return M_PI / 180.0 * angle;
	}
	float ToDegree(float angle)
	{
		return 180.0f / (float)M_PI * angle;
	}
	double ToDegree(double angle)
	{
		return 180.0 / M_PI * angle;
	}
}

