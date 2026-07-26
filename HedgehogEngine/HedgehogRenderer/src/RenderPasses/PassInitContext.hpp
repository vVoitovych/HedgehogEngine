#pragma once

namespace RHI
{
    class IRHIDevice;
}

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    class PassResourceCache;

    // Dependencies a pass needs to construct its shared, immutable resources (render pass,
    // pipeline, descriptor-set layouts, shader modules — see PassResourceCache). Bundled by
    // const-reference rather than passed as five separate constructor parameters so a new
    // dependency doesn't ripple through every pass's constructor signature. Deliberately carries
    // no ResourceManager reference: the render-target format a pass's immutable render pass
    // object needs is a fixed renderer-wide convention (currently RHI::Format::R16G16B16A16Unorm
    // for colour, IRHIDevice::GetPreferredDepthFormat() for depth — see ResourceManager.cpp),
    // not something the immutable half of a pass should query per instance. See
    // workflow/current-plan.md, "Pass instance sharing".
    struct PassInitContext
    {
        RHI::IRHIDevice&                  Device;
        const FS::FileSystemManager&      FileSystem;
        const HedgehogSettings::Settings& Settings;
        HR::ResourceRegistry&             Registry;
        PassResourceCache&                Cache;
    };
}
