#pragma once

#include "../Common/pch.h"
#include "Device.h"
#include"SwapChain.h"
#include "RenderPass.h"

namespace Renderer
{
	class Pipeline
	{
	public:
		Pipeline();
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass);
		void Cleanup(Device& device);

		VkPipeline GetPipeline() const;
	private:
		VkPipeline mPipeline;
		VkPipelineLayout mGraphycsPipelineLayout;

	};
}