#pragma once

#include <bitset>

#define MAX_ENTITIES 512
#define MAX_COMPONENTS 16

namespace ECS
{
	using Entity = size_t;
	using Signature = std::bitset<MAX_COMPONENTS>;
}

