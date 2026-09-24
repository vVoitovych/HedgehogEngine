#include "HedgehogRenderer/Graph/ResourcePool.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;
using namespace RGTest;

namespace
{
    RHI::TextureDesc MakeDesc(uint32_t width, uint32_t height, RHI::Format format = RHI::Format::R8G8B8A8Unorm)
    {
        RHI::TextureDesc desc;
        desc.Width  = width;
        desc.Height = height;
        desc.Format = format;
        desc.Usage  = RHI::TextureUsage::ColorAttachment;
        return desc;
    }
}

TEST_CASE("Two descriptors that agree on every field hash and compare equal")
{
    const PooledTextureKey a = PooledTextureKey::FromDesc(MakeDesc(1920, 1080));
    const PooledTextureKey b = PooledTextureKey::FromDesc(MakeDesc(1920, 1080));

    CHECK(a == b);
    CHECK(PooledTextureKeyHash{}(a) == PooledTextureKeyHash{}(b));
}

TEST_CASE("Descriptors that differ in any single field compare unequal")
{
    const PooledTextureKey base = PooledTextureKey::FromDesc(MakeDesc(1920, 1080));

    CHECK(base != PooledTextureKey::FromDesc(MakeDesc(1280, 1080)));                              // width
    CHECK(base != PooledTextureKey::FromDesc(MakeDesc(1920, 720)));                                // height
    CHECK(base != PooledTextureKey::FromDesc(MakeDesc(1920, 1080, RHI::Format::R16G16B16A16Float))); // format

    RHI::TextureDesc differentUsage = MakeDesc(1920, 1080);
    differentUsage.Usage = RHI::TextureUsage::DepthStencil;
    CHECK(base != PooledTextureKey::FromDesc(differentUsage));

    RHI::TextureDesc differentMips = MakeDesc(1920, 1080);
    differentMips.MipLevels = 4;
    CHECK(base != PooledTextureKey::FromDesc(differentMips));
}

TEST_CASE("Acquire creates a fresh texture when the pool has nothing retired yet")
{
    TestDevice device;
    ResourcePool pool(device);

    RHI::IRHITexture* texture = pool.Acquire(MakeDesc(1920, 1080));
    REQUIRE(texture != nullptr);
    CHECK(device.m_TexturesCreated == 1);
    CHECK(pool.LiveCount() == 1);
    CHECK(pool.RetiredCount() == 0);
}

TEST_CASE("RetireFrame makes an acquired texture available for reuse instead of destroying it")
{
    TestDevice device;
    ResourcePool pool(device);

    RHI::IRHITexture* first = pool.Acquire(MakeDesc(1920, 1080));
    pool.RetireFrame();
    CHECK(pool.LiveCount() == 0);
    CHECK(pool.RetiredCount() == 1);

    RHI::IRHITexture* second = pool.Acquire(MakeDesc(1920, 1080));
    CHECK(second == first);              // the very same object, not a new allocation
    CHECK(device.m_TexturesCreated == 1); // still only one texture ever created
}

TEST_CASE("A retired texture is never handed out for an incompatible descriptor")
{
    TestDevice device;
    ResourcePool pool(device);

    RHI::IRHITexture* hdColor = pool.Acquire(MakeDesc(1920, 1080));
    pool.RetireFrame();

    RHI::IRHITexture* depth = pool.Acquire(MakeDesc(1920, 1080, RHI::Format::D32Float));
    CHECK(depth != hdColor);
    CHECK(device.m_TexturesCreated == 2); // the incompatible request had to create a new one

    // The original 1920x1080 color texture is still sitting retired, untouched.
    CHECK(pool.RetiredCount() == 1);
    CHECK(pool.LiveCount() == 1);
}

TEST_CASE("Two textures Acquire()'d in the same frame with the same descriptor are distinct instances")
{
    TestDevice device;
    ResourcePool pool(device);

    // Neither has been retired, so the pool must not hand out the same physical texture twice
    // to two live requests — that would alias one texture as two different graph resources.
    RHI::IRHITexture* a = pool.Acquire(MakeDesc(1920, 1080));
    RHI::IRHITexture* b = pool.Acquire(MakeDesc(1920, 1080));

    CHECK(a != b);
    CHECK(device.m_TexturesCreated == 2);
    CHECK(pool.LiveCount() == 2);
}

TEST_CASE("A texture that stays retired across multiple RetireFrame calls remains reusable")
{
    TestDevice device;
    ResourcePool pool(device);

    RHI::IRHITexture* first = pool.Acquire(MakeDesc(512, 512));
    pool.RetireFrame();
    pool.RetireFrame(); // nothing acquired in between — still just the one retired entry
    pool.RetireFrame();

    RHI::IRHITexture* reused = pool.Acquire(MakeDesc(512, 512));
    CHECK(reused == first);
    CHECK(device.m_TexturesCreated == 1);
}
