#include "MeshSync.hpp"

#include "ResourceManager/ResourceManager.hpp"
#include "ResourceManager/ResourceNames.hpp"

#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/Mesh.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/RHITypes.hpp"

namespace Renderer
{
    void MeshSync::Sync(const HedgehogEngine::MeshContainer& container,
                        ResourceManager&                     resourceManager,
                        RHI::IRHIDevice&                     device)
    {
        const size_t totalMeshes = container.GetMeshCount();
        if (totalMeshes <= m_RegisteredMeshCount)
            return;

        for (size_t i = m_RegisteredMeshCount; i < totalMeshes; ++i)
        {
            const auto& mesh     = container.GetMesh(i);
            const auto positions = mesh.GetPositions();
            const auto texCoords = mesh.GetTexCoords();
            const auto normals   = mesh.GetNormals();
            const auto indices   = mesh.GetIndices();

            MeshGeometryInfo geom;
            geom.m_FirstIndex   = mesh.GetFirstIndex();
            geom.m_IndexCount   = mesh.GetIndexCount();
            geom.m_VertexOffset = mesh.GetVertexOffset();
            m_MeshGeometryInfos.push_back(geom);

            for (const auto& p : positions)
            {
                m_CpuPositions.push_back(p.x());
                m_CpuPositions.push_back(p.y());
                m_CpuPositions.push_back(p.z());
            }
            for (const auto& uv : texCoords)
            {
                m_CpuTexCoords.push_back(uv.x());
                m_CpuTexCoords.push_back(uv.y());
            }
            for (const auto& n : normals)
            {
                m_CpuNormals.push_back(n.x());
                m_CpuNormals.push_back(n.y());
                m_CpuNormals.push_back(n.z());
            }
            for (uint32_t idx : indices)
                m_CpuIndices.push_back(idx);
        }

        m_RegisteredMeshCount = totalMeshes;
        m_MeshDataDirty = true;
        FlushMeshUploads(resourceManager, device);
    }

    const MeshGeometryInfo& MeshSync::GetMeshGeometryInfo(size_t meshIndex) const
    {
        return m_MeshGeometryInfos[meshIndex];
    }

    void MeshSync::FlushMeshUploads(ResourceManager& resourceManager, RHI::IRHIDevice& device)
    {
        if (!m_MeshDataDirty)
            return;

        if (resourceManager.HasBuffer(ResourceNames::MESH_POSITIONS))
            device.WaitIdle();

        const size_t posSize = m_CpuPositions.size() * sizeof(float);
        const size_t uvSize  = m_CpuTexCoords.size() * sizeof(float);
        const size_t nrmSize = m_CpuNormals.size()   * sizeof(float);
        const size_t idxSize = m_CpuIndices.size()   * sizeof(uint32_t);

        resourceManager.DestroyBuffer(ResourceNames::MESH_POSITIONS);
        resourceManager.DestroyBuffer(ResourceNames::MESH_TEX_COORDS);
        resourceManager.DestroyBuffer(ResourceNames::MESH_NORMALS);
        resourceManager.DestroyBuffer(ResourceNames::MESH_INDICES);

        resourceManager.CreateBuffer(ResourceNames::MESH_POSITIONS,  device, posSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        resourceManager.CreateBuffer(ResourceNames::MESH_TEX_COORDS, device, uvSize,  RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        resourceManager.CreateBuffer(ResourceNames::MESH_NORMALS,    device, nrmSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        resourceManager.CreateBuffer(ResourceNames::MESH_INDICES,    device, idxSize, RHI::BufferUsage::IndexBuffer,  RHI::MemoryUsage::CpuToGpu);

        resourceManager.GetBuffer(ResourceNames::MESH_POSITIONS).CopyData(m_CpuPositions.data(), posSize);
        resourceManager.GetBuffer(ResourceNames::MESH_TEX_COORDS).CopyData(m_CpuTexCoords.data(), uvSize);
        resourceManager.GetBuffer(ResourceNames::MESH_NORMALS).CopyData(m_CpuNormals.data(),     nrmSize);
        resourceManager.GetBuffer(ResourceNames::MESH_INDICES).CopyData(m_CpuIndices.data(),     idxSize);

        m_MeshDataDirty = false;
    }
}
