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
    R16G16Float,
    R16G16B16A16Unorm,
    R16G16B16A16Float,
    R32Float,
    R32G32Float,
    R32G32B32Float,
    R32G32B32A32Float,
    R32G32B32A32Uint,
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

// A resource's usage at one point in a command stream — what Barrier() transitions between.
// Shared by textures and buffers; the texture-only states (RenderTarget, DepthWrite, DepthRead,
// Present) are simply never used in a BufferBarrier.
enum class ResourceState
{
    Undefined,
    RenderTarget,
    DepthWrite,
    DepthRead,
    ShaderResource,
    UnorderedAccess,
    CopySrc,
    CopyDst,
    Present,
};

enum class TextureType
{
    Texture2D,
    TextureCube,
    Texture2DArray,
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

// Sentinels mirroring Vulkan's VK_REMAINING_MIP_LEVELS / VK_REMAINING_ARRAY_LAYERS: "every
// level/layer from Base.. to the end of the resource". A default-constructed
// TextureSubresourceRange means "the whole resource".
inline constexpr uint32_t REMAINING_MIP_LEVELS   = ~0u;
inline constexpr uint32_t REMAINING_ARRAY_LAYERS = ~0u;

// Mirrors Vulkan's VK_WHOLE_SIZE: a BufferBarrier::Size of this value means "from Offset to
// the end of the buffer".
inline constexpr size_t WHOLE_BUFFER_SIZE = static_cast<size_t>(~0ull);

struct TextureSubresourceRange
{
    uint32_t BaseMipLevel    = 0;
    uint32_t MipLevelCount   = REMAINING_MIP_LEVELS;
    uint32_t BaseArrayLayer  = 0;
    uint32_t ArrayLayerCount = REMAINING_ARRAY_LAYERS;
};

struct TextureDesc
{
    uint32_t     Width   = 1;
    uint32_t     Height  = 1;
    RHI::Format  Format  = RHI::Format::R8G8B8A8Srgb;
    TextureUsage Usage   = TextureUsage::Sampled;

    TextureType  Type        = TextureType::Texture2D;
    uint32_t     MipLevels   = 1;
    uint32_t     ArrayLayers = 1;
};

// One mip level of one array layer (a cube face is a layer) that a buffer copy writes. A Width or
// Height of 0 means the level's whole extent; the buffer's texels are tightly packed from
// BufferOffset.
struct TextureRegion
{
    uint32_t MipLevel     = 0;
    uint32_t ArrayLayer   = 0;
    uint32_t Width        = 0;
    uint32_t Height       = 0;
    size_t   BufferOffset = 0;
};

// The levels of a full mip chain for a width x height texture, down to 1x1.
constexpr uint32_t GetMipLevelCount(uint32_t width, uint32_t height)
{
    uint32_t levels  = 1;
    uint32_t largest = width > height ? width : height;
    while (largest > 1)
    {
        largest >>= 1;
        ++levels;
    }
    return levels;
}

struct SamplerDesc
{
    Filter      MinFilter    = Filter::Linear;
    Filter      MagFilter    = Filter::Linear;
    AddressMode AddressModeU = AddressMode::Repeat;
    AddressMode AddressModeV = AddressMode::Repeat;
    AddressMode AddressModeW = AddressMode::Repeat;
    float       MaxAnisotropy = 16.0f;
    // Set: a comparison sampler (a depth texture sampled as the fraction of texels passing the
    // test against the reference value, filtered when linear), as a shadow map is read.
    std::optional<CompareOp> Compare;
};

} // namespace RHI
