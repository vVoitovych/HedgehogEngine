#include "IconUpload.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"

namespace Editor
{
    std::unique_ptr<RHI::IRHITexture> UploadIconTexture(const RHI::IRHIDevice& device,
                                                        const ContentLoader::TextureLoader& image)
    {
        const uint32_t width  = static_cast<uint32_t>(image.GetWidth());
        const uint32_t height = static_cast<uint32_t>(image.GetHeight());
        const size_t   size   = static_cast<size_t>(width) * height * 4; // decoded as RGBA

        auto staging = device.CreateBuffer(size, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
        staging->CopyData(image.GetData(), size);

        RHI::TextureDesc desc;
        desc.Width  = width;
        desc.Height = height;
        desc.Format = RHI::Format::R8G8B8A8Srgb;
        desc.Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
        auto texture = device.CreateTexture(desc);

        device.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
        {
            const RHI::TextureBarrier toCopy{ texture.get(), RHI::ResourceState::Undefined, RHI::ResourceState::CopyDst };
            cmd.Barrier({ &toCopy, 1 }, {});
            cmd.CopyBufferToTexture(*staging, *texture);
            const RHI::TextureBarrier toRead{ texture.get(), RHI::ResourceState::CopyDst, RHI::ResourceState::ShaderResource };
            cmd.Barrier({ &toRead, 1 }, {});
        });
        return texture;
    }
}
