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
    float m_X        = 0.0f;
    float m_Y        = 0.0f;
    float m_Width    = 0.0f;
    float m_Height   = 0.0f;
    float m_MinDepth = 0.0f;
    float m_MaxDepth = 1.0f;
};

struct Scissor
{
    int32_t  m_X      = 0;
    int32_t  m_Y      = 0;
    uint32_t m_Width  = 0;
    uint32_t m_Height = 0;
};

struct ClearColorValue
{
    float m_R = 0.0f;
    float m_G = 0.0f;
    float m_B = 0.0f;
    float m_A = 1.0f;
};

struct ClearDepthStencilValue
{
    float    m_Depth   = 1.0f;
    uint32_t m_Stencil = 0;
};

// A single clear value; fill m_Color or m_DepthStencil depending on attachment type.
struct ClearValue
{
    ClearColorValue        m_Color        = {};
    ClearDepthStencilValue m_DepthStencil = {};
};

struct VertexBinding
{
    uint32_t        m_Binding   = 0;
    uint32_t        m_Stride    = 0;
    VertexInputRate m_InputRate = VertexInputRate::PerVertex;
};

struct VertexAttribute
{
    uint32_t m_Location = 0;
    uint32_t m_Binding  = 0;
    Format   m_Format   = Format::Undefined;
    uint32_t m_Offset   = 0;
};

struct PushConstantRange
{
    ShaderStage m_Stages = ShaderStage::All;
    uint32_t    m_Offset = 0;
    uint32_t    m_Size   = 0;
};

struct DescriptorBinding
{
    uint32_t       m_Binding = 0;
    DescriptorType m_Type    = DescriptorType::UniformBuffer;
    uint32_t       m_Count   = 1;
    ShaderStage    m_Stages  = ShaderStage::All;
};

struct PoolSize
{
    DescriptorType m_Type  = DescriptorType::UniformBuffer;
    uint32_t       m_Count = 0;
};

struct AttachmentDesc
{
    Format      m_Format             = Format::Undefined;
    LoadOp      m_LoadOp             = LoadOp::Clear;
    StoreOp     m_StoreOp            = StoreOp::Store;
    LoadOp      m_StencilLoadOp      = LoadOp::DontCare;
    StoreOp     m_StencilStoreOp     = StoreOp::DontCare;
    ImageLayout m_InitialLayout      = ImageLayout::Undefined;
    ImageLayout m_FinalLayout        = ImageLayout::ColorAttachment;
};

struct ColorBlendAttachment
{
    bool        m_BlendEnable      = false;
    BlendFactor m_SrcColorFactor   = BlendFactor::SrcAlpha;
    BlendFactor m_DstColorFactor   = BlendFactor::OneMinusSrcAlpha;
    BlendOp     m_ColorOp          = BlendOp::Add;
    BlendFactor m_SrcAlphaFactor   = BlendFactor::One;
    BlendFactor m_DstAlphaFactor   = BlendFactor::Zero;
    BlendOp     m_AlphaOp          = BlendOp::Add;
};

struct TextureDesc
{
    uint32_t     m_Width   = 1;
    uint32_t     m_Height  = 1;
    Format       m_Format  = Format::R8G8B8A8Srgb;
    TextureUsage m_Usage   = TextureUsage::Sampled;
};

struct SamplerDesc
{
    Filter      m_MinFilter    = Filter::Linear;
    Filter      m_MagFilter    = Filter::Linear;
    AddressMode m_AddressModeU = AddressMode::Repeat;
    AddressMode m_AddressModeV = AddressMode::Repeat;
    AddressMode m_AddressModeW = AddressMode::Repeat;
    float       m_MaxAnisotropy = 16.0f;
};

} // namespace RHI
