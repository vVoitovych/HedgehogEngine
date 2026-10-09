#include "EnvironmentResources.hpp"

#include "ContentLoader/api/EnvironmentBake.hpp"
#include "ContentLoader/api/HalfFloat.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <vector>

namespace Renderer
{
    namespace
    {
        // One texture's texels, uploaded region by region (mip, layer) from one staging buffer and
        // left ready to sample.
        struct UploadRegion
        {
            const std::vector<uint16_t>* Texels = nullptr;
            RHI::TextureRegion           Region;
        };

        std::unique_ptr<RHI::IRHITexture> Upload(RHI::IRHIDevice& device, const RHI::TextureDesc& desc,
                                                 std::vector<UploadRegion>& regions)
        {
            size_t total = 0;
            for (UploadRegion& region : regions)
            {
                region.Region.BufferOffset = total;
                total += region.Texels->size() * sizeof(uint16_t);
            }
            auto staging = device.CreateBuffer(total, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
            for (const UploadRegion& region : regions)
                staging->CopyData(region.Texels->data(), region.Texels->size() * sizeof(uint16_t), region.Region.BufferOffset);

            auto texture = device.CreateTexture(desc);
            device.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
            {
                const RHI::TextureBarrier toCopy{ texture.get(), RHI::ResourceState::Undefined, RHI::ResourceState::CopyDst };
                cmd.Barrier({ &toCopy, 1 }, {});
                for (const UploadRegion& region : regions)
                    cmd.CopyBufferToTexture(*staging, *texture, region.Region);
                const RHI::TextureBarrier toRead{ texture.get(), RHI::ResourceState::CopyDst, RHI::ResourceState::ShaderResource };
                cmd.Barrier({ &toRead, 1 }, {});
            });
            return texture;
        }

        RHI::TextureDesc MakeCubeDesc(uint32_t faceSize, uint32_t mipCount)
        {
            RHI::TextureDesc desc;
            desc.Width       = faceSize;
            desc.Height      = faceSize;
            desc.Format      = RHI::Format::R16G16B16A16Float;
            desc.Usage       = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
            desc.Type        = RHI::TextureType::TextureCube;
            desc.MipLevels   = mipCount;
            desc.ArrayLayers = ContentLoader::CUBE_FACE_COUNT;
            return desc;
        }

        std::unique_ptr<RHI::IRHITexture> UploadCube(RHI::IRHIDevice& device, const ContentLoader::BakedEnvironment& baked)
        {
            std::vector<UploadRegion> regions;
            for (uint32_t mip = 0; mip < baked.MipCount; ++mip)
            {
                for (uint32_t face = 0; face < ContentLoader::CUBE_FACE_COUNT; ++face)
                {
                    UploadRegion region;
                    region.Texels            = &baked.Mips[mip][face];
                    region.Region.MipLevel   = mip;
                    region.Region.ArrayLayer = face;
                    regions.push_back(region);
                }
            }
            return Upload(device, MakeCubeDesc(baked.FaceSize, baked.MipCount), regions);
        }
    }

    EnvironmentResources::EnvironmentResources(RHI::IRHIDevice& device)
    {
        // Black: an environment that adds nothing, whatever the uniform says.
        const std::vector<uint16_t> black(4, ContentLoader::FloatToHalf(0.0f));
        std::vector<UploadRegion>   faces;
        for (uint32_t face = 0; face < ContentLoader::CUBE_FACE_COUNT; ++face)
        {
            UploadRegion region;
            region.Texels            = &black;
            region.Region.ArrayLayer = face;
            faces.push_back(region);
        }
        m_BlackCube = Upload(device, MakeCubeDesc(1, 1), faces);

        const ContentLoader::BrdfLut lut = ContentLoader::ComputeBrdfLut(BRDF_LUT_SIZE, BRDF_LUT_SAMPLES);
        RHI::TextureDesc             lutDesc;
        lutDesc.Width  = lut.Size;
        lutDesc.Height = lut.Size;
        lutDesc.Format = RHI::Format::R16G16Float;
        lutDesc.Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
        std::vector<UploadRegion> lutRegion(1);
        lutRegion[0].Texels = &lut.Texels;
        m_BrdfLut           = Upload(device, lutDesc, lutRegion);

        m_Current.Radiance = m_BlackCube.get();
        m_Current.BrdfLut  = m_BrdfLut.get();
    }

    EnvironmentResources::~EnvironmentResources() = default;

    const EnvironmentResources::BakedMap& EnvironmentResources::FindOrBake(const std::string& path, RHI::IRHIDevice& device,
                                                                            const FS::FileSystemManager& fileSystem)
    {
        const auto found = m_Baked.find(path);
        if (found != m_Baked.end())
            return found->second;

        // A map that does not bake has logged its error; it is remembered, so it is not tried again.
        BakedMap   map;
        const auto baked = ContentLoader::BakeEnvironment(path, fileSystem);
        if (baked)
        {
            map.Radiance = UploadCube(device, *baked);
            map.Sh       = baked->Sh;
            map.MipCount = baked->MipCount;
        }
        return m_Baked.emplace(path, std::move(map)).first->second;
    }

    void EnvironmentResources::Sync(const HX::RenderEnvironment& environment, RHI::IRHIDevice& device,
                                    const FS::FileSystemManager& fileSystem)
    {
        m_Current          = ForwardEnvironment{};
        m_Current.Radiance = m_BlackCube.get();
        m_Current.BrdfLut  = m_BrdfLut.get();
        if (!environment.Present || environment.MapPath.empty())
            return;

        const BakedMap& map = FindOrBake(environment.MapPath, device, fileSystem);
        if (!map.Radiance)
            return;
        m_Current.Uniform  = MakeEnvironmentUniform(map.Sh, map.MipCount, environment.Intensity, environment.RotationDegrees);
        m_Current.Radiance   = map.Radiance.get();
        m_Current.ShowSkybox = environment.ShowSkybox;
    }
}
