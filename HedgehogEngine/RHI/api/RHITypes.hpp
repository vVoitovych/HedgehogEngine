#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RHI
{

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class Format : uint32_t
{
    Undefined,
    R8Unorm,
    R8G8B8A8Unorm,
    R8G8B8A8Srgb,
    B8G8R8A8Unorm,
    B8G8R8A8Srgb,
    R16Float,
    R16G16B16A16Unorm,
    R16G16B16A16Float,
    R32Float,
    R32G32Float,
    R32G32B32Float,
    R32G32B32A32Float,
    D16Unorm,
    D32Float,
    D24UnormS8Uint,
    D32FloatS8Uint,
};

enum class BufferUsage : uint32_t
{
    None          = 0,
    VertexBuffer  = 1 << 0,
    IndexBuffer   = 1 << 1,
    UniformBuffer = 1 << 2,
    StorageBuffer = 1 << 3,
    TransferSrc   = 1 << 4,
    TransferDst   = 1 << 5,
};

inline BufferUsage operator|(BufferUsage lhs, BufferUsage rhs)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline bool operator&(BufferUsage lhs, BufferUsage rhs)
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

enum class TextureUsage : uint32_t
{
    None              = 0,
    Sampled           = 1 << 0,
    ColorAttachment   = 1 << 1,
    DepthStencil      = 1 << 2,
    TransferSrc       = 1 << 3,
    TransferDst       = 1 << 4,
    Storage           = 1 << 5,
};

inline TextureUsage operator|(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline bool operator&(TextureUsage lhs, TextureUsage rhs)
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

enum class ShaderStage : uint32_t
{
    None     = 0,
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    All      = Vertex | Fragment | Compute,
};

inline ShaderStage operator|(ShaderStage lhs, ShaderStage rhs)
{
    return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline bool operator&(ShaderStage lhs, ShaderStage rhs)
{
    return (static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)) != 0;
}

enum class MemoryUsage
{
    GpuOnly,
    CpuToGpu,
    GpuToCpu,
};

enum class IndexType
{
    Uint16,
    Uint32,
};

enum class DescriptorType
{
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage,
    InputAttachment,
};

enum class LoadOp
{
    Load,
    Clear,
    DontCare,
};

enum class StoreOp
{
    Store,
    DontCare,
};

enum class ImageLayout
{
    Undefined,
    General,
    ColorAttachment,
    DepthStencilAttachment,
    DepthStencilReadOnly,
    ShaderReadOnly,
    TransferSrc,
    TransferDst,
    Present,
};

enum class PrimitiveTopology
{
    TriangleList,
    TriangleStrip,
    LineList,
    PointList,
};

enum class CullMode
{
    None,
    Front,
    Back,
};

enum class FillMode
{
    Solid,
    Wireframe,
};

enum class CompareOp
{
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

enum class BlendFactor
{
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcColor,
    OneMinusSrcColor,
};

enum class BlendOp
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class Filter
{
    Nearest,
    Linear,
};

enum class AddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

enum class VertexInputRate
{
    PerVertex,
    PerInstance,
};

// ── POD Structs ───────────────────────────────────────────────────────────────

struct Viewport
{
    float X        = 0.0f;
    float Y        = 0.0f;
    float Width    = 0.0f;
    float Height   = 0.0f;
    float MinDepth = 0.0f;
    float MaxDepth = 1.0f;
};

struct Scissor
{
    int32_t  X      = 0;
    int32_t  Y      = 0;
    uint32_t Width  = 0;
    uint32_t Height = 0;
};

struct ClearColorValue
{
    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float A = 1.0f;
};

struct ClearDepthStencilValue
{
    float    Depth   = 1.0f;
    uint32_t Stencil = 0;
};

struct ClearValue
{
    bool                   IsDepth      = false;
    ClearColorValue        Color        = {};
    ClearDepthStencilValue DepthStencil = {};
};

struct VertexBinding
{
    uint32_t        Binding   = 0;
    uint32_t        Stride    = 0;
    VertexInputRate InputRate = VertexInputRate::PerVertex;
};

struct VertexAttribute
{
    uint32_t    Location = 0;
    uint32_t    Binding  = 0;
    RHI::Format Format   = RHI::Format::Undefined;
    uint32_t    Offset   = 0;
};

struct PushConstantRange
{
    ShaderStage Stages = ShaderStage::All;
    uint32_t    Offset = 0;
    uint32_t    Size   = 0;
};

struct DescriptorBinding
{
    uint32_t       Binding = 0;
    DescriptorType Type    = DescriptorType::UniformBuffer;
    uint32_t       Count   = 1;
    ShaderStage    Stages  = ShaderStage::All;
};

struct PoolSize
{
    DescriptorType Type  = DescriptorType::UniformBuffer;
    uint32_t       Count = 0;
};

struct AttachmentDesc
{
    RHI::Format  Format             = RHI::Format::Undefined;
    RHI::LoadOp  LoadOp             = RHI::LoadOp::Clear;
    RHI::StoreOp StoreOp            = RHI::StoreOp::Store;
    RHI::LoadOp  StencilLoadOp      = RHI::LoadOp::DontCare;
    RHI::StoreOp StencilStoreOp     = RHI::StoreOp::DontCare;
    ImageLayout InitialLayout      = ImageLayout::Undefined;
    ImageLayout FinalLayout        = ImageLayout::ColorAttachment;
};

struct ColorBlendAttachment
{
    bool        BlendEnable      = false;
    BlendFactor SrcColorFactor   = BlendFactor::SrcAlpha;
    BlendFactor DstColorFactor   = BlendFactor::OneMinusSrcAlpha;
    BlendOp     ColorOp          = BlendOp::Add;
    BlendFactor SrcAlphaFactor   = BlendFactor::One;
    BlendFactor DstAlphaFactor   = BlendFactor::Zero;
    BlendOp     AlphaOp          = BlendOp::Add;
};

struct TextureDesc
{
    uint32_t     Width   = 1;
    uint32_t     Height  = 1;
    RHI::Format  Format  = RHI::Format::R8G8B8A8Srgb;
    TextureUsage Usage   = TextureUsage::Sampled;
};

struct SamplerDesc
{
    Filter      MinFilter    = Filter::Linear;
    Filter      MagFilter    = Filter::Linear;
    AddressMode AddressModeU = AddressMode::Repeat;
    AddressMode AddressModeV = AddressMode::Repeat;
    AddressMode AddressModeW = AddressMode::Repeat;
    float       MaxAnisotropy = 16.0f;
};

} // namespace RHI
