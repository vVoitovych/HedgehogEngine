#include "HedgehogRenderer/Targets/RenderTargetRegistry.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Renderer;
using namespace RGTest;

namespace
{
    RenderTargetDesc AbsoluteTarget(const char* name, uint32_t width, uint32_t height)
    {
        return { name, RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(width, height) };
    }

    TargetExtent ExtentOf(const RenderTargetRegistry& registry, const char* name)
    {
        return registry.Resolve(name).Extent;
    }

    bool Mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

TEST_CASE("'main' resolves to the swapchain's current image; an unknown name fails, naming it")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);

    registry.BeginFrame(1, 0, 1);
    const ResolvedRenderTarget main = registry.Resolve("main");
    CHECK(main.Status == RenderTargetStatus::Ok);
    CHECK(main.Texture == &swapchain.GetTexture(1));
    CHECK(main.Format == RHI::Format::B8G8R8A8Srgb);
    CHECK(main.Extent == TargetExtent{ 1280, 720 });

    const ResolvedRenderTarget missing = registry.Resolve("sceen");
    CHECK(missing.Status == RenderTargetStatus::Unknown);
    CHECK(missing.Texture == nullptr);
    CHECK(Mentions(missing.Message, "render target 'sceen' is not declared"));
    registry.EndFrame();
}

TEST_CASE("A declared target resolves to its own texture")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);

    REQUIRE(registry.Declare(AbsoluteTarget("scene", 640, 360)).Success);
    const ResolvedRenderTarget scene = registry.Resolve("scene");
    REQUIRE(scene.Status == RenderTargetStatus::Ok);
    REQUIRE(scene.Texture != nullptr);
    CHECK(scene.Texture->GetWidth() == 640);
    CHECK(scene.Texture->GetDesc().Usage
          == (RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferSrc));
}

TEST_CASE("Invalid declarations are rejected with the target named")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);
    REQUIRE(registry.Declare(AbsoluteTarget("scene", 64, 64)).Success);

    CHECK(Mentions(registry.Declare(AbsoluteTarget("main", 64, 64)).Message, "reserved for the swapchain"));
    CHECK(Mentions(registry.Declare(AbsoluteTarget("scene", 64, 64)).Message, "render target 'scene' is already declared"));
    CHECK(Mentions(registry.Declare({ "bloom", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeRelativeToResult(0.5f) })
                       .Message, "render target 'bloom' cannot use RelativeToResult"));
    CHECK(Mentions(registry.Declare({ "blank", RHI::Format::Undefined, RGSizePolicy::MakeAbsolute(8, 8) }).Message,
                   "render target 'blank' has no format"));
    CHECK(Mentions(registry.RequestResize("nowhere", 8, 8).Message, "render target 'nowhere' is not declared"));
}

TEST_CASE("All three size policies resolve, including after a swapchain resize")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);

    REQUIRE(registry.Declare(AbsoluteTarget("scene", 800, 600)).Success);
    REQUIRE(registry.Declare({ "half", RHI::Format::R16Float, RGSizePolicy::MakeRelativeToSwapchain(0.5f) }).Success);
    CHECK(ExtentOf(registry, "scene") == TargetExtent{ 800, 600 });
    CHECK(ExtentOf(registry, "half") == TargetExtent{ 640, 360 });

    // Graph resources: RelativeToResult scales the view's target, RelativeToSwapchain the swapchain.
    CHECK(registry.ResolveGraphResourceSize(RGSizePolicy::MakeRelativeToResult(0.25f), "scene").Extent
          == TargetExtent{ 200, 150 });
    CHECK(registry.ResolveGraphResourceSize(RGSizePolicy::MakeRelativeToSwapchain(1.0f), "scene").Extent
          == TargetExtent{ 1280, 720 });
    CHECK(registry.ResolveGraphResourceSize(RGSizePolicy::MakeAbsolute(2048, 2048), "scene").Extent
          == TargetExtent{ 2048, 2048 });
    CHECK(registry.ResolveGraphResourceSize(RGSizePolicy::MakeRelativeToResult(1.0f), "nowhere").Status
          == RenderTargetStatus::Unknown);

    swapchain.Resize(1920, 1080);
    registry.NotifySwapchainResized();
    registry.BeginFrame(1, 0, 0);
    registry.EndFrame();

    CHECK(ExtentOf(registry, "main") == TargetExtent{ 1920, 1080 });
    CHECK(ExtentOf(registry, "half") == TargetExtent{ 960, 540 });
    CHECK(ExtentOf(registry, "scene") == TargetExtent{ 800, 600 }); // Absolute: unaffected
    CHECK(registry.Resolve("half").Texture->GetWidth() == 960);
    CHECK(registry.ResolveGraphResourceSize(RGSizePolicy::MakeRelativeToResult(0.5f), "main").Extent
          == TargetExtent{ 960, 540 });
}

TEST_CASE("A resize is applied at end of frame, and the old texture outlives the frames still using it")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);
    REQUIRE(registry.Declare(AbsoluteTarget("scene", 800, 600)).Success);
    const RHI::IRHITexture* original = registry.Resolve("scene").Texture;

    // Frame 5: the editor drags the panel mid-frame.
    registry.BeginFrame(5, 3, 0);
    REQUIRE(registry.RequestResize("scene", 1024, 768).Success);

    // Nothing changes until the frame ends: every Resolve in this frame sees the same texture.
    CHECK(registry.Resolve("scene").Texture == original);
    CHECK(ExtentOf(registry, "scene") == TargetExtent{ 800, 600 });

    registry.EndFrame();
    CHECK(ExtentOf(registry, "scene") == TargetExtent{ 1024, 768 });
    CHECK(registry.Resolve("scene").Texture != original);

    // The 800x600 texture was last used by frame 5; it survives until frame 5's fence signals.
    CHECK(registry.GetRetiredTextureCount() == 1);
    CHECK(device.m_TexturesDestroyed == 0);

    registry.BeginFrame(6, 4, 1); // frame 5 may still be on the GPU
    registry.EndFrame();
    CHECK(device.m_TexturesDestroyed == 0);

    registry.BeginFrame(7, 5, 0); // frame 5 has completed
    CHECK(device.m_TexturesDestroyed == 1);
    CHECK(registry.GetRetiredTextureCount() == 0);
    registry.EndFrame();
}

TEST_CASE("A zero-area target is reported as such and never allocated")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);

    SUBCASE("declared at zero area")
    {
        REQUIRE(registry.Declare(AbsoluteTarget("collapsed", 0, 240)).Success);
        const ResolvedRenderTarget target = registry.Resolve("collapsed");
        CHECK(target.Status == RenderTargetStatus::ZeroArea);
        CHECK(target.Texture == nullptr);
        CHECK(Mentions(target.Message, "render target 'collapsed' has zero area (0x240)"));
        CHECK(device.m_TexturesCreated == 0);
    }
    SUBCASE("an editor panel collapses")
    {
        REQUIRE(registry.Declare(AbsoluteTarget("scene", 800, 600)).Success);
        registry.BeginFrame(1, 0, 0);
        REQUIRE(registry.RequestResize("scene", 0, 0).Success);
        registry.EndFrame();

        CHECK(registry.Resolve("scene").Status == RenderTargetStatus::ZeroArea);
        CHECK(device.m_TexturesCreated == 1); // nothing new allocated for 0x0
        CHECK(registry.ResolveGraphResourceSize(RGSizePolicy::MakeRelativeToResult(0.5f), "scene").Status
              == RenderTargetStatus::ZeroArea);
    }
    SUBCASE("the window is minimized")
    {
        REQUIRE(registry.Declare({ "half", RHI::Format::R16Float, RGSizePolicy::MakeRelativeToSwapchain(0.5f) }).Success);
        swapchain.Resize(0, 0);
        registry.NotifySwapchainResized();
        registry.BeginFrame(1, 0, 0);
        registry.EndFrame();

        CHECK(registry.Resolve("main").Status == RenderTargetStatus::ZeroArea);
        CHECK(registry.Resolve("main").Texture == nullptr);
        CHECK(registry.Resolve("half").Status == RenderTargetStatus::ZeroArea);
    }
}

TEST_CASE("Only Absolute targets are resized directly")
{
    TestDevice    device;
    TestSwapchain swapchain(1280, 720);
    RenderTargetRegistry registry(device, swapchain);
    REQUIRE(registry.Declare({ "half", RHI::Format::R16Float, RGSizePolicy::MakeRelativeToSwapchain(0.5f) }).Success);

    const RenderTargetResult result = registry.RequestResize("half", 10, 10);
    CHECK_FALSE(result.Success);
    CHECK(Mentions(result.Message, "render target 'half' follows the swapchain"));
}
