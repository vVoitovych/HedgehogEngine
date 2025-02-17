#include "MeshContainer.hpp"

#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "Scene/Scene.hpp"

#include "Logger/Logger.hpp"

#include <algorithm>

namespace Context
{
    MeshContainer::MeshContainer()
    {
    }

    MeshContainer::~MeshContainer()
    {
    }

    void MeshContainer::AddFilePath(std::string filePath)
    {
        auto it = std::find(mFilePathes.begin(), mFilePathes.end(), filePath);
        if (it == mFilePathes.end())
        {
            mFilePathes.push_back(filePath);
        }
        else
        {
            LOGWARNING("Mesh ", filePath, " already added");
        }
    }

    void MeshContainer::ClearFileList()
    {
        mFilePathes.clear();
    }

    void MeshContainer::LoadMeshData()
    {
        mMeshes.clear();
        for (size_t i = mMeshes.size(); i < mFilePathes.size(); ++i)
        {
            Mesh mesh;
            mesh.LoadData(mFilePathes[i]);
            LOGINFO("Loaded mesh data: ", mFilePathes[i]);
            mMeshes.push_back(mesh);

        }
    }

    void MeshContainer::Initialize(const VulkanContext& context)
    {
        std::vector<VertexDescription> verticies;
        std::vector<uint32_t> indicies;
        verticies.clear();
        indicies.clear();

        for (size_t i = 0; i < mMeshes.size(); ++i)
        {
            auto& mesh = mMeshes[i];
            mesh.SetVertexOffset(static_cast<uint32_t>(verticies.size()));
            mesh.SetFirstIndex(static_cast<uint32_t>(indicies.size()));

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
        if (mVertexBuffer == nullptr && mIndexBuffer == nullptr)
        {
            CreateVertexBuffer(context, verticies, mVertexBuffer);
            CreateIndexBuffer(context, indicies, mIndexBuffer);
        }
        else
        {
            CreateVertexBuffer(context, verticies, mAdditionalVertexBuffer);
            CreateIndexBuffer(context, indicies, mAdditionalIndexBuffer);
            mIsSwaped = false;
        }
    }

    void MeshContainer::Update(const VulkanContext& context, Scene::Scene& scene)
    {
        if (mIsSwaped)
        {
            if (mAdditionalVertexBuffer != nullptr)
            {
                mVertexBuffer->DestroyBuffer(context.GetDevice());
                mVertexBuffer = std::move(mAdditionalVertexBuffer);
                mAdditionalVertexBuffer = nullptr;
            }
            if (mAdditionalIndexBuffer != nullptr)
            {
                mIndexBuffer->DestroyBuffer(context.GetDevice());
                mIndexBuffer = std::move(mAdditionalIndexBuffer);
                mAdditionalIndexBuffer = nullptr;
            }
        }
        auto& meshes = scene.GetMeshes();
        if (meshes.size() <= mMeshes.size())
            return;
        for (size_t i = mMeshes.size(); i < meshes.size(); ++i)
        {
            AddFilePath(meshes[i]);
        }
        LoadMeshData();
        Initialize(context);
    }

    void MeshContainer::Cleanup(const VulkanContext& context)
    {
        auto& device = context.GetDevice();
        if (mVertexBuffer != nullptr)
            mVertexBuffer->DestroyBuffer(device);
        if (mIndexBuffer != nullptr)
            mIndexBuffer->DestroyBuffer(device);
        if (mAdditionalVertexBuffer != nullptr)
            mAdditionalVertexBuffer->DestroyBuffer(device);
        if (mAdditionalIndexBuffer != nullptr)
            mAdditionalIndexBuffer->DestroyBuffer(device);
    }

    const VkBuffer& MeshContainer::GetVertexBuffer() const
    {
        if (mIsSwaped)
            return mVertexBuffer->GetNativeBuffer();
        return mAdditionalVertexBuffer->GetNativeBuffer();
    }

    const VkBuffer& MeshContainer::GetIndexBuffer() const
    {
        if (mIsSwaped)
            return mIndexBuffer->GetNativeBuffer();
        return mAdditionalIndexBuffer->GetNativeBuffer();
    }

    const Mesh& MeshContainer::GetMesh(size_t index) const
    {
        return mMeshes[index];
    }

    void MeshContainer::SwapBuffers()
    {
        if (!mIsSwaped)
            mIsSwaped = true;
    }

    void MeshContainer::CreateVertexBuffer(const VulkanContext& context, const std::vector<VertexDescription> verticies, std::unique_ptr<Wrappers::Buffer>& buffer)
    {
        auto& device = context.GetDevice();
        VkDeviceSize size = sizeof(verticies[0]) * verticies.size();

        Wrappers::Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        staginBuffer.CopyDataToBufferMemory(device, verticies.data(), (size_t)size);

        buffer = std::make_unique<Wrappers::Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        device.CopyBufferToBuffer(staginBuffer.GetNativeBuffer(), buffer->GetNativeBuffer(), size);

        staginBuffer.DestroyBuffer(device);
        LOGINFO("Vertex buffer created");
    }

    void MeshContainer::CreateIndexBuffer(const VulkanContext& context, const std::vector<uint32_t> indicies, std::unique_ptr<Wrappers::Buffer>& buffer)
    {
        auto& device = context.GetDevice();
        VkDeviceSize size = sizeof(indicies[0]) * indicies.size();

        Wrappers::Buffer staginBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        staginBuffer.CopyDataToBufferMemory(device, indicies.data(), (size_t)size);

        buffer = std::make_unique<Wrappers::Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        device.CopyBufferToBuffer(staginBuffer.GetNativeBuffer(), buffer->GetNativeBuffer(), size);

        staginBuffer.DestroyBuffer(device);
        LOGINFO("Index buffer created");
    }

}




