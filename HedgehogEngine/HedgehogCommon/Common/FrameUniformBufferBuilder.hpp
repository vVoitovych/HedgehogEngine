#pragma once

namespace Wrappers
{
	class DescriptorLayoutBuilder;
}

namespace HedgehogCommon
{
	class FrameUniformBufferBilder
	{
	public:
		static Wrappers::DescriptorLayoutBuilder Build();
	};
}


