#pragma once

#include "NodeConfig.hpp"
#include "RHI/api/RHITypes.hpp"

#include <string>

namespace Renderer
{
    class ResourceManager;

    RHI::LoadOp      ParseLoadOp(const std::string& s);
    RHI::StoreOp     ParseStoreOp(const std::string& s);
    RHI::ImageLayout ParseImageLayout(const std::string& s);

    // Converts an AttachmentConfig to a fully-populated RHI::AttachmentDesc.
    // Reads the texture format from the ResourceManager using cfg.m_Resource.
    RHI::AttachmentDesc ToAttachmentDesc(const AttachmentConfig& cfg,
                                         const ResourceManager&  rm);

} // namespace Renderer
