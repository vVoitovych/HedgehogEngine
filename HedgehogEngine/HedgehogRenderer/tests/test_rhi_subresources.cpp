#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include "RHI/api/RHITypes.hpp"

#include <string>
#include <vector>

TEST_CASE("A full mip chain runs down to 1x1 whatever the aspect")
{
    CHECK(RHI::GetMipLevelCount(1, 1) == 1);
    CHECK(RHI::GetMipLevelCount(2, 1) == 2);
    CHECK(RHI::GetMipLevelCount(1, 2) == 2);
    CHECK(RHI::GetMipLevelCount(3, 5) == 3);
    CHECK(RHI::GetMipLevelCount(256, 256) == 9);
    CHECK(RHI::GetMipLevelCount(1024, 512) == 11);
    CHECK(RHI::GetMipLevelCount(2048, 1) == 12);
    static_assert(RHI::GetMipLevelCount(4096, 4096) == 13);
}

TEST_CASE("A texture region defaults to the whole of mip 0, layer 0, from the buffer's start")
{
    const RHI::TextureRegion region;
    CHECK(region.MipLevel == 0);
    CHECK(region.ArrayLayer == 0);
    CHECK(region.Width == 0);
    CHECK(region.Height == 0);
    CHECK(region.BufferOffset == 0);
}

TEST_CASE("A sampler compares only when asked")
{
    RHI::SamplerDesc plain;
    CHECK_FALSE(plain.Compare.has_value());

    RHI::SamplerDesc shadow;
    shadow.Compare = RHI::CompareOp::LessOrEqual;
    REQUIRE(shadow.Compare.has_value());
    CHECK(*shadow.Compare == RHI::CompareOp::LessOrEqual);
}

TEST_CASE("The recording command list records region copies and mipmap generation in order")
{
    RHI::TextureDesc desc;
    desc.Type        = RHI::TextureType::TextureCube;
    desc.Width       = 64;
    desc.Height      = 64;
    desc.ArrayLayers = 6;
    desc.MipLevels   = RHI::GetMipLevelCount(64, 64);
    RGTest::TestTexture cube(desc);
    RGTest::TestBuffer  staging{ 64 * 64 * 8 * 6 };

    RGTest::RecordingCommandList cmd;
    // Mip 2 of the +Z face (layer 4), 16x16 texels of 8 bytes, after four whole faces of them.
    RHI::TextureRegion region;
    region.MipLevel     = 2;
    region.ArrayLayer   = 4;
    region.BufferOffset = 4 * 16 * 16 * 8;
    cmd.CopyBufferToTexture(staging, cube, region);
    cmd.GenerateMipmaps(cube);

    REQUIRE(cmd.RegionCopies.size() == 1);
    CHECK(cmd.RegionCopies[0].Texture == &cube);
    CHECK(cmd.RegionCopies[0].Region.MipLevel == 2);
    CHECK(cmd.RegionCopies[0].Region.ArrayLayer == 4);
    CHECK(cmd.RegionCopies[0].Region.BufferOffset == 4 * 16 * 16 * 8);
    CHECK(cmd.MipmapGenerations == std::vector<const RHI::IRHITexture*>{ &cube });
    CHECK(cmd.Commands == std::vector<std::string>{ "copy 2 4", "mipmaps" });
}

TEST_CASE("The recording command list records query resets and timestamps, and a pool reads only what was written")
{
    RGTest::TestDevice device;
    const std::unique_ptr<RHI::IRHIQueryPool> pool = device.CreateTimestampQueryPool(4);
    REQUIRE(pool != nullptr);
    CHECK(pool->GetCount() == 4);

    RGTest::RecordingCommandList cmd;
    cmd.ResetQueries(*pool, 0, 2);
    cmd.WriteTimestamp(*pool, 0);
    cmd.WriteTimestamp(*pool, 1);
    CHECK(cmd.Commands == std::vector<std::string>{ "reset queries 0 2", "timestamp 0", "timestamp 1" });

    auto& test = static_cast<RGTest::TestQueryPool&>(*pool);
    uint64_t ticks[2] = {};
    CHECK_FALSE(pool->ReadTimestamps(0, ticks));
    test.SetTimestamp(0, 100);
    CHECK_FALSE(pool->ReadTimestamps(0, ticks)); // the second is not written yet
    test.SetTimestamp(1, 350);
    REQUIRE(pool->ReadTimestamps(0, ticks));
    CHECK(ticks[0] == 100);
    CHECK(ticks[1] == 350);
    CHECK_FALSE(pool->ReadTimestamps(3, ticks)); // past the pool

    cmd.ResetQueries(*pool, 0, 2);
    CHECK_FALSE(pool->ReadTimestamps(0, ticks));
}
