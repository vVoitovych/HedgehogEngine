#include "MeshContainer.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Logger/Logger.hpp"

namespace Renderer
{
    void MeshContainer::AddFilePath(std::string filePath)
    {
        auto it = std::find(mFilePathes.begin(), mFilePathes.end(), filePath);
        if (it != mFilePathes.end())
            mFilePathes.push_back(filePath);
    }

    void MeshContainer::ClearFileList()
    {
        mFilePathes.clear();
    }

    void MeshContainer::LoadMeshData()
    {
        mMeshes.clear();
        Mesh mesh;
        for (size_t i = 0; i < mFilePathes.size(); ++i)
        {
            mMeshes.push_back(mesh);
            auto meshInContainer = mMeshes.back();
            meshInContainer.LoadData(mFilePathes[i]);
            LOGINFO("Loaded mesh data: ", mFilePathes[i]);

        }
    }

    void MeshContainer::LoadSingleMesh(std::string filePath)
    {
        auto it = std::find(mFilePathes.begin(), mFilePathes.end(), filePath);
        if (it == mFilePathes.end())
            return;
        mFilePathes.push_back(filePath);
        Mesh mesh;
        mMeshes.push_back(mesh);
        auto meshInContainer = mMeshes.back();
        meshInContainer.LoadData(filePath);
        LOGINFO("Loaded mesh data: ", filePath);

    }

    void MeshContainer::Initialize(const Device& device)
    {
        std::vector<VertexDescription> verticies;
        std::vector<uint32_t> indicies;
        verticies.clear();
        indicies.clear();

        for (size_t i = 0; i < mMeshes.size(); ++i)
        {
            auto& mesh = mMeshes[i];
            mesh.SetVertexOffset(verticies.size());
            mesh.SetFirstIndex(indicies.size());

            auto meshVerticies = mesh.GetVerticies();
            for (size_t j = 0; j < meshVerticies.size(); ++j)
            {
                verticies.push_back(meshVerticies[j]);
            }
            auto meshIndicies = mesh.GetIndicies();
            for (size_t j = 0; j < meshIndicies.size(); ++j)
            {
                indicies.push_back(meshIndicies[j]);
            }
        }

        CreateVertexBuffer(device, verticies);
        CreateIndexBuffer(device, indicies);
    }

    void MeshContainer::Cleanup(const Device& device)
    {
        vkDestroyBuffer(device.GetNativeDevice(), mVertexBuffer, nullptr);
        vkFreeMemory(device.GetNativeDevice(), mVertexBufferMemory, nullptr);
        mVertexBuffer = nullptr;
        mVertexBufferMemory = nullptr;
        LOGINFO("Vertex buffer cleaned");

        vkDestroyBuffer(device.GetNativeDevice(), mIndexBuffer, nullptr);
        vkFreeMemory(device.GetNativeDevice(), mIndexBufferMemory, nullptr);
        mIndexBuffer = nullptr;
        mIndexBufferMemory = nullptr;
        LOGINFO("Index buffer cleaned");
    }

    VkBuffer MeshContainer::GetVertexBuffer()
    {
        return mVertexBuffer;
    }

    VkBuffer MeshContainer::GetIndexBuffer()
    {
        return mIndexBuffer;
    }

    void MeshContainer::CreateVertexBuffer(const Device& device, const std::vector<VertexDescription> verticies)
    {
        VkDeviceSize size = sizeof(verticies[0]) * verticies.size();

        VkBuffer staginBuffer;
        VkDeviceMemory staginBufferMemory;
        device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staginBuffer, staginBufferMemory);
        device.CopyDataToBufferMemory(staginBufferMemory, verticies.data(), (size_t)size);

        device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mVertexBuffer, mVertexBufferMemory);
        device.CopyBuffer(staginBuffer, mVertexBuffer, size);

        device.DestroyBuffer(staginBuffer, nullptr);
        device.FreeMemory(staginBufferMemory, nullptr);
        LOGINFO("Vertex buffer created");
    }

    void MeshContainer::CreateIndexBuffer(const Device& device, const std::vector<uint32_t> indicies)
    {
        VkDeviceSize size = sizeof(indicies[0]) * indicies.size();

        VkBuffer staginBuffer;
        VkDeviceMemory staginBufferMemory;
        device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staginBuffer, staginBufferMemory);
        device.CopyDataToBufferMemory(staginBufferMemory, indicies.data(), (size_t)size);

        device.CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mIndexBuffer, mIndexBufferMemory);
        device.CopyBuffer(staginBuffer, mIndexBuffer, size);

        device.DestroyBuffer(staginBuffer, nullptr);
        device.FreeMemory(staginBufferMemory, nullptr);
        LOGINFO("Index buffer created");
    }

}




