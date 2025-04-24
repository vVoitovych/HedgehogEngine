#pragma once

namespace Wrappers
{
	class DescriptorLayoutBuilder;
}

namespace Context
{
	class FrameUniformBufferBilder
	{
	public:
		static Wrappers::DescriptorLayoutBuilder Build();
	};
}


