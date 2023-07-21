#pragma once

#include "..\Common\pch.h"
#include "Vertex.h"

namespace Renderer
{
	class Device;

	class Mesh
	{
	public:
		Mesh();
		~Mesh();

		void CreateVertexBuffer(Device& device);
		void Cleanup(Device& device);

		VkBuffer GetVertexBuffer();
	private:
		std::vector<Vertex> mVerticies;
		VkBuffer mVertexBuffer;
		VkDeviceMemory mVertexBufferMemory;
	};

}



