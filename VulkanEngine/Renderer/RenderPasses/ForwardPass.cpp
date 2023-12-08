#include "ForwardPass.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Context/RenderContext.hpp"


namespace Renderer
{
	void ForwardPass::Render(RenderContext& renderContext, ResourceTracker& resourceTracker)
	{
		//auto currentFrame = renderContext.GetCurrentFrame();
		//auto& commandBuffer = renderContext.GetCommandBuffer(currentFrame);

		//auto extend = renderContext.GetExtend();
		//
		//commandBuffer.BeginRenderPass(extend, mRenderPass, mFrameBuffer.GetNativeFrameBuffer());
		////commandBuffer.BindPipeline(mPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		//commandBuffer.SetViewport(0.0f, 0.0f, (float)extend.width, (float)extend.height, 0.0f, 1.0f);
		//commandBuffer.SetScissor({ 0, 0 }, extend);
		////VkBuffer vertexBuffers[] = { mMesh.GetVertexBuffer() };
		//VkDeviceSize offsets[] = { 0 };
		////commandBuffer.BindVertexBuffers(0, 1, vertexBuffers, offsets);
		////commandBuffer.BindIndexBuffer(mMesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		////commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline, 0, 1, mDescriptorSets[currentFrame].GetNativeSet(), 0, nullptr);

		////commandBuffer.DrawIndexed(mMesh.GetIndiciesCount(), 1, 0, 0, 0);
		//commandBuffer.EndRenderPass();
		
	}

	void ForwardPass::Initialize(Device& device, RenderContext& renderContext)
	{
	}

	void ForwardPass::Cleanup(Device& device)
	{
	}

}

