#pragma once

#include "HedgehogRenderer/api/RHI/RHITypes.hpp"

#include "volk.h"
#include "vk_mem_alloc.h"

#include <cassert>

// Conversion utilities between RHI abstract types and Vulkan API types.
// All functions are inline so this header has no corresponding .cpp.

namespace RHI::VulkanTypes
{

// ── Format ───────────────────────────────────────────────────────────────────

inline VkFormat ToVkFormat(Format format)
{
    switch (format)
    {
        case Format::Undefined:         return VK_FORMAT_UNDEFINED;
        case Format::R8Unorm:           return VK_FORMAT_R8_UNORM;
        case Format::R8G8B8A8Unorm:     return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8Srgb:      return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::B8G8R8A8Unorm:     return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8Srgb:      return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::R16G16B16A16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32Float:          return VK_FORMAT_R32_SFLOAT;
        case Format::R32G32B32Float:    return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32B32A32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::D16Unorm:          return VK_FORMAT_D16_UNORM;
        case Format::D32Float:          return VK_FORMAT_D32_SFLOAT;
        case Format::D24UnormS8Uint:    return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32FloatS8Uint:    return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:                        assert(false && "Unknown RHI format"); return VK_FORMAT_UNDEFINED;
    }
}

inline Format FromVkFormat(VkFormat vkFormat)
{
    switch (vkFormat)
    {
        case VK_FORMAT_UNDEFINED:            return Format::Undefined;
        case VK_FORMAT_R8_UNORM:             return Format::R8Unorm;
        case VK_FORMAT_R8G8B8A8_UNORM:       return Format::R8G8B8A8Unorm;
        case VK_FORMAT_R8G8B8A8_SRGB:        return Format::R8G8B8A8Srgb;
        case VK_FORMAT_B8G8R8A8_UNORM:       return Format::B8G8R8A8Unorm;
        case VK_FORMAT_B8G8R8A8_SRGB:        return Format::B8G8R8A8Srgb;
        case VK_FORMAT_R16G16B16A16_SFLOAT:  return Format::R16G16B16A16Float;
        case VK_FORMAT_R32_SFLOAT:           return Format::R32Float;
        case VK_FORMAT_R32G32B32_SFLOAT:     return Format::R32G32B32Float;
        case VK_FORMAT_R32G32B32A32_SFLOAT:  return Format::R32G32B32A32Float;
        case VK_FORMAT_D16_UNORM:            return Format::D16Unorm;
        case VK_FORMAT_D32_SFLOAT:           return Format::D32Float;
        case VK_FORMAT_D24_UNORM_S8_UINT:    return Format::D24UnormS8Uint;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:   return Format::D32FloatS8Uint;
        default:                             return Format::Undefined;
    }
}

inline bool IsDepthFormat(Format format)
{
    return format == Format::D16Unorm
        || format == Format::D32Float
        || format == Format::D24UnormS8Uint
        || format == Format::D32FloatS8Uint;
}

inline VkImageAspectFlags GetAspectMask(Format format)
{
    if (format == Format::D24UnormS8Uint || format == Format::D32FloatS8Uint)
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    if (IsDepthFormat(format))
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// ── Buffer usage ──────────────────────────────────────────────────────────────

inline VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage)
{
    VkBufferUsageFlags flags = 0;
    if (usage & BufferUsage::VertexBuffer)  flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage & BufferUsage::IndexBuffer)   flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage & BufferUsage::UniformBuffer) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage & BufferUsage::StorageBuffer) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage & BufferUsage::TransferSrc)   flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & BufferUsage::TransferDst)   flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

// ── Texture usage ─────────────────────────────────────────────────────────────

inline VkImageUsageFlags ToVkImageUsage(TextureUsage usage)
{
    VkImageUsageFlags flags = 0;
    if (usage & TextureUsage::Sampled)           flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage & TextureUsage::ColorAttachment)   flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage & TextureUsage::DepthStencil)      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage & TextureUsage::TransferSrc)       flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage & TextureUsage::TransferDst)       flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage & TextureUsage::Storage)           flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    return flags;
}

// ── Memory usage ─────────────────────────────────────────────────────────────

inline VmaMemoryUsage ToVmaMemoryUsage(MemoryUsage usage)
{
    switch (usage)
    {
        case MemoryUsage::GpuOnly:  return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryUsage::CpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryUsage::GpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:                    assert(false && "Unknown MemoryUsage"); return VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

// ── Shader stage ─────────────────────────────────────────────────────────────

inline VkShaderStageFlags ToVkShaderStage(ShaderStage stage)
{
    VkShaderStageFlags flags = 0;
    if (stage & ShaderStage::Vertex)   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage & ShaderStage::Fragment) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage & ShaderStage::Compute)  flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

inline VkShaderStageFlagBits ToVkShaderStageBit(ShaderStage stage)
{
    if (stage == ShaderStage::Vertex)   return VK_SHADER_STAGE_VERTEX_BIT;
    if (stage == ShaderStage::Fragment) return VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage == ShaderStage::Compute)  return VK_SHADER_STAGE_COMPUTE_BIT;
    assert(false && "Unsupported single ShaderStage value");
    return VK_SHADER_STAGE_VERTEX_BIT;
}

// ── Descriptor type ───────────────────────────────────────────────────────────

inline VkDescriptorType ToVkDescriptorType(DescriptorType type)
{
    switch (type)
    {
        case DescriptorType::UniformBuffer:       return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:       return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler:return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageImage:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::InputAttachment:     return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default: assert(false && "Unknown DescriptorType"); return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

// ── Image layout ─────────────────────────────────────────────────────────────

inline VkImageLayout ToVkLayout(ImageLayout layout)
{
    switch (layout)
    {
        case ImageLayout::Undefined:              return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::General:                return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::ColorAttachment:        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilReadOnly:   return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ImageLayout::ShaderReadOnly:         return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout::TransferSrc:            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::TransferDst:            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ImageLayout::Present:                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default: assert(false && "Unknown ImageLayout"); return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

// ── Load / store ops ──────────────────────────────────────────────────────────

inline VkAttachmentLoadOp ToVkLoadOp(LoadOp op)
{
    switch (op)
    {
        case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default: assert(false && "Unknown LoadOp"); return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

inline VkAttachmentStoreOp ToVkStoreOp(StoreOp op)
{
    switch (op)
    {
        case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default: assert(false && "Unknown StoreOp"); return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

// ── Primitive topology ────────────────────────────────────────────────────────

inline VkPrimitiveTopology ToVkTopology(PrimitiveTopology topology)
{
    switch (topology)
    {
        case PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default: assert(false && "Unknown PrimitiveTopology"); return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

// ── Rasterization ─────────────────────────────────────────────────────────────

inline VkCullModeFlags ToVkCullMode(CullMode mode)
{
    switch (mode)
    {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        default: assert(false && "Unknown CullMode"); return VK_CULL_MODE_BACK_BIT;
    }
}

inline VkPolygonMode ToVkFillMode(FillMode mode)
{
    switch (mode)
    {
        case FillMode::Solid:      return VK_POLYGON_MODE_FILL;
        case FillMode::Wireframe:  return VK_POLYGON_MODE_LINE;
        default: assert(false && "Unknown FillMode"); return VK_POLYGON_MODE_FILL;
    }
}

// ── Depth compare ─────────────────────────────────────────────────────────────

inline VkCompareOp ToVkCompareOp(CompareOp op)
{
    switch (op)
    {
        case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:           return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
        default: assert(false && "Unknown CompareOp"); return VK_COMPARE_OP_LESS;
    }
}

// ── Blend ────────────────────────────────────────────────────────────────────

inline VkBlendFactor ToVkBlendFactor(BlendFactor factor)
{
    switch (factor)
    {
        case BlendFactor::Zero:             return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:              return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        default: assert(false && "Unknown BlendFactor"); return VK_BLEND_FACTOR_ONE;
    }
}

inline VkBlendOp ToVkBlendOp(BlendOp op)
{
    switch (op)
    {
        case BlendOp::Add:             return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:             return VK_BLEND_OP_MIN;
        case BlendOp::Max:             return VK_BLEND_OP_MAX;
        default: assert(false && "Unknown BlendOp"); return VK_BLEND_OP_ADD;
    }
}

// ── Vertex input rate ─────────────────────────────────────────────────────────

inline VkVertexInputRate ToVkInputRate(VertexInputRate rate)
{
    switch (rate)
    {
        case VertexInputRate::PerVertex:   return VK_VERTEX_INPUT_RATE_VERTEX;
        case VertexInputRate::PerInstance: return VK_VERTEX_INPUT_RATE_INSTANCE;
        default: assert(false && "Unknown VertexInputRate"); return VK_VERTEX_INPUT_RATE_VERTEX;
    }
}

// ── Index type ────────────────────────────────────────────────────────────────

inline VkIndexType ToVkIndexType(IndexType type)
{
    switch (type)
    {
        case IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: assert(false && "Unknown IndexType"); return VK_INDEX_TYPE_UINT32;
    }
}

// ── Sampler ───────────────────────────────────────────────────────────────────

inline VkFilter ToVkFilter(Filter filter)
{
    switch (filter)
    {
        case Filter::Nearest: return VK_FILTER_NEAREST;
        case Filter::Linear:  return VK_FILTER_LINEAR;
        default: assert(false && "Unknown Filter"); return VK_FILTER_LINEAR;
    }
}

inline VkSamplerAddressMode ToVkAddressMode(AddressMode mode)
{
    switch (mode)
    {
        case AddressMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: assert(false && "Unknown AddressMode"); return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

// ── Barrier helpers (synchronization2) ───────────────────────────────────────

// Derives appropriate stage masks and access masks for a layout transition.
inline void FillBarrierStages(VkImageMemoryBarrier2& barrier,
                               ImageLayout             oldLayout,
                               ImageLayout             newLayout)
{
    // Source stage / access — what was the previous operation?
    switch (oldLayout)
    {
        case ImageLayout::Undefined:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask = 0;
            break;
        case ImageLayout::TransferDst:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            break;
        case ImageLayout::ColorAttachment:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case ImageLayout::DepthStencilAttachment:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case ImageLayout::ShaderReadOnly:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            break;
        case ImageLayout::Present:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrier.srcAccessMask = 0;
            break;
        default:
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
            break;
    }

    // Destination stage / access — what does the next operation need?
    switch (newLayout)
    {
        case ImageLayout::TransferDst:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            break;
        case ImageLayout::TransferSrc:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            break;
        case ImageLayout::ShaderReadOnly:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            break;
        case ImageLayout::ColorAttachment:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case ImageLayout::DepthStencilAttachment:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                  | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            break;
        case ImageLayout::Present:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrier.dstAccessMask = 0;
            break;
        default:
            barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            break;
    }
}

} // namespace RHI::VulkanTypes
