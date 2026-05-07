#include "NodeConfigUtils.hpp"

#include "ResourceManager/ResourceManager.hpp"

#include "RHI/api/IRHITexture.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace Renderer
{
    RHI::LoadOp ParseLoadOp(const std::string& s)
    {
        if (s == "Load")     return RHI::LoadOp::Load;
        if (s == "Clear")    return RHI::LoadOp::Clear;
        if (s == "DontCare") return RHI::LoadOp::DontCare;
        LOGERROR("NodeConfigUtils: unknown LoadOp '", s, "'");
        assert(false && "Unknown LoadOp in .rq file");
        return RHI::LoadOp::DontCare;
    }

    RHI::StoreOp ParseStoreOp(const std::string& s)
    {
        if (s == "Store")    return RHI::StoreOp::Store;
        if (s == "DontCare") return RHI::StoreOp::DontCare;
        LOGERROR("NodeConfigUtils: unknown StoreOp '", s, "'");
        assert(false && "Unknown StoreOp in .rq file");
        return RHI::StoreOp::DontCare;
    }

    RHI::ImageLayout ParseImageLayout(const std::string& s)
    {
        if (s == "Undefined")              return RHI::ImageLayout::Undefined;
        if (s == "General")                return RHI::ImageLayout::General;
        if (s == "ColorAttachment")        return RHI::ImageLayout::ColorAttachment;
        if (s == "DepthStencilAttachment") return RHI::ImageLayout::DepthStencilAttachment;
        if (s == "DepthStencilReadOnly")   return RHI::ImageLayout::DepthStencilReadOnly;
        if (s == "ShaderReadOnly")         return RHI::ImageLayout::ShaderReadOnly;
        if (s == "TransferSrc")            return RHI::ImageLayout::TransferSrc;
        if (s == "TransferDst")            return RHI::ImageLayout::TransferDst;
        if (s == "Present")                return RHI::ImageLayout::Present;
        LOGERROR("NodeConfigUtils: unknown ImageLayout '", s, "'");
        assert(false && "Unknown ImageLayout in .rq file");
        return RHI::ImageLayout::Undefined;
    }

    RHI::AttachmentDesc ToAttachmentDesc(const AttachmentConfig& cfg, const ResourceManager& rm)
    {
        RHI::AttachmentDesc desc;
        desc.m_Format        = rm.GetTexture(cfg.m_Resource).GetFormat();
        desc.m_LoadOp        = ParseLoadOp(cfg.m_LoadOp);
        desc.m_StoreOp       = ParseStoreOp(cfg.m_StoreOp);
        desc.m_InitialLayout = ParseImageLayout(cfg.m_InitialLayout);
        desc.m_FinalLayout   = ParseImageLayout(cfg.m_FinalLayout);
        return desc;
    }

} // namespace Renderer
