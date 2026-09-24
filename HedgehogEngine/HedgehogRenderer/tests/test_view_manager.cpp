#include "HedgehogRenderer/Views/ViewManager.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cstring>
#include <string>
#include <type_traits>

using namespace Renderer;
using namespace RGTest;

namespace
{
    // BuildViews can only read the scene: the source cameras are unreachable for writing.
    static_assert(std::is_same_v<decltype(&ViewManager::BuildViews),
                                 const std::vector<View>& (ViewManager::*)(const HX::RenderScene&)>);

    HX::RenderCamera MakeCamera(uint64_t sourceId, HX::CameraTargetMode mode = HX::CameraTargetMode::Main,
                                const char* targetName = "")
    {
        HX::RenderCamera camera;
        camera.SourceId   = sourceId;
        camera.TargetMode = mode;
        camera.TargetName = targetName;
        camera.Priority   = static_cast<int32_t>(sourceId);
        return camera;
    }

    // Every field, and the matrix bit for bit: the renderer-side source of a derived view.
    bool IdenticalCameras(const HX::RenderCamera& lhs, const HX::RenderCamera& rhs)
    {
        return std::memcmp(lhs.WorldMatrix.GetBuffer(), rhs.WorldMatrix.GetBuffer(), 16 * sizeof(float)) == 0
            && lhs.ProjectionType == rhs.ProjectionType && lhs.Fov == rhs.Fov && lhs.OrthoSize == rhs.OrthoSize
            && lhs.NearPlane == rhs.NearPlane && lhs.FarPlane == rhs.FarPlane && lhs.LayerMask == rhs.LayerMask
            && lhs.TargetMode == rhs.TargetMode && lhs.TargetName == rhs.TargetName
            && lhs.GraphName == rhs.GraphName && lhs.Priority == rhs.Priority && lhs.SourceId == rhs.SourceId;
    }

    // A registry with the swapchain plus "game" and "scene" panels, as the editor will have.
    struct Fixture
    {
        TestDevice           Device;
        TestSwapchain        Swapchain{ 1280, 720 };
        RenderTargetRegistry Targets{ Device, Swapchain };
        ViewManager          Views{ Targets };

        Fixture()
        {
            REQUIRE(Targets.Declare({ "game", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(800, 600) }).Success);
            REQUIRE(Targets.Declare({ "scene", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(640, 480) }).Success);
        }
    };

    bool AnyDropMentions(const ViewManager& views, ViewDropReason reason, const std::string& text)
    {
        for (const DroppedView& dropped : views.GetDroppedViews())
            if (dropped.Reason == reason && dropped.Message.find(text) != std::string::npos)
                return true;
        for (const DroppedView& dropped : views.GetDroppedViews())
            MESSAGE("dropped: " << dropped.Message);
        return false;
    }
}

TEST_CASE("A view is derived from each camera, with its target intent resolved")
{
    Fixture f;
    HX::RenderScene scene;
    scene.Cameras = { MakeCamera(7), MakeCamera(9, HX::CameraTargetMode::Texture, "game") };

    const std::vector<View>& views = f.Views.BuildViews(scene);
    REQUIRE(views.size() == 2);

    CHECK(views[0].Origin == ViewOrigin::Derived);
    CHECK(views[0].SourceId == 7);
    CHECK(views[0].Desc.GraphName == "game");
    CHECK(views[0].Desc.Targets == std::vector<std::string>{ "main" });
    CHECK(views[0].ResolvedTargets.at(0).Texture == &f.Swapchain.GetTexture(0));

    CHECK(views[1].SourceId == 9);
    CHECK(views[1].Desc.Targets == std::vector<std::string>{ "game" });
    CHECK(views[1].ResolvedTargets.at(0).Texture == f.Targets.Resolve("game").Texture);
    CHECK(views[1].ResolvedTargets.at(0).Extent == TargetExtent{ 800, 600 });
    CHECK(f.Views.GetDroppedViews().empty());
}

TEST_CASE("Derived views keep their id across frames and go away with their camera, at end of frame")
{
    Fixture f;
    HX::RenderScene scene;
    scene.Cameras = { MakeCamera(7), MakeCamera(9) };

    const ViewId first = f.Views.BuildViews(scene).at(0).Id;
    f.Views.EndFrame();
    CHECK(f.Views.BuildViews(scene).at(0).Id == first);
    f.Views.EndFrame();

    scene.Cameras = { MakeCamera(9) };
    const std::vector<View>& views = f.Views.BuildViews(scene);
    REQUIRE(views.size() == 1);
    CHECK(views[0].SourceId == 9);
    CHECK(f.Views.GetViewCount() == 2); // camera 7's view is only destroyed at end of frame
    f.Views.EndFrame();
    CHECK(f.Views.GetViewCount() == 1);
}

TEST_CASE("An application view survives reconciliation; only DestroyView removes it, at end of frame")
{
    Fixture f;
    ViewDesc result;
    result.Targets   = { "main" };
    result.GraphName = "result";
    const ViewId id = f.Views.CreateView(result);

    HX::RenderScene empty;
    for (int frame = 0; frame < 3; ++frame)
    {
        const std::vector<View>& views = f.Views.BuildViews(empty);
        REQUIRE(views.size() == 1);
        CHECK(views[0].Id == id);
        CHECK(views[0].Origin == ViewOrigin::Application);
        CHECK_FALSE(views[0].Desc.Camera.has_value());
        f.Views.EndFrame();
    }

    HX::RenderScene withCamera;
    withCamera.Cameras = { MakeCamera(3) };
    CHECK(f.Views.BuildViews(withCamera).size() == 2); // derived views never displace it
    f.Views.EndFrame();

    f.Views.DestroyView(id);
    CHECK(f.Views.GetViewCount() == 2);
    f.Views.EndFrame();
    CHECK(f.Views.GetViewCount() == 1);
    CHECK_FALSE(f.Views.UpdateView(id, result));
}

TEST_CASE("Overriding a derived view's target leaves its source camera byte-identical")
{
    Fixture f;
    HX::RenderScene scene;
    scene.Cameras = { MakeCamera(7) }; // targets "main"
    scene.Cameras[0].WorldMatrix.GetBuffer()[12] = 12.5f;
    const HX::RenderCamera before = scene.Cameras[0];

    // Editor mode: the game camera draws into the game panel instead of the swapchain.
    f.Views.SetTargetOverride(7, { "game" });
    const std::vector<View>& views = f.Views.BuildViews(scene);

    REQUIRE(views.size() == 1);
    CHECK(views[0].Desc.Targets == std::vector<std::string>{ "game" });
    CHECK(views[0].ResolvedTargets.at(0).Texture == f.Targets.Resolve("game").Texture);
    CHECK(IdenticalCameras(scene.Cameras[0], before));
    CHECK(scene.Cameras[0].TargetMode == HX::CameraTargetMode::Main);

    f.Views.EndFrame();
    f.Views.ClearTargetOverride(7);
    CHECK(f.Views.BuildViews(scene).at(0).Desc.Targets == std::vector<std::string>{ "main" });
}

TEST_CASE("Disabled, targetless, unresolvable and zero-area views are dropped at build time")
{
    Fixture f;
    REQUIRE(f.Targets.Declare({ "collapsed", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(0, 300) }).Success);

    ViewDesc disabled;
    disabled.Targets   = { "scene" };
    disabled.IsEnabled = false;
    (void)f.Views.CreateView(disabled);
    (void)f.Views.CreateView(ViewDesc{});

    HX::RenderScene scene;
    scene.Cameras = { MakeCamera(4, HX::CameraTargetMode::Texture, "sceen"),
                      MakeCamera(5, HX::CameraTargetMode::Texture, "collapsed"),
                      MakeCamera(6, HX::CameraTargetMode::Texture, "scene") };

    const std::vector<View>& views = f.Views.BuildViews(scene);
    REQUIRE(views.size() == 1);
    CHECK(views[0].SourceId == 6);

    CHECK(f.Views.GetDroppedViews().size() == 4);
    CHECK(AnyDropMentions(f.Views, ViewDropReason::Disabled, "dropped: disabled"));
    CHECK(AnyDropMentions(f.Views, ViewDropReason::NoTargets, "dropped: it has no targets"));
    CHECK(AnyDropMentions(f.Views, ViewDropReason::UnknownTarget, "(camera 4) ('game') dropped: render target 'sceen' is not declared"));
    CHECK(AnyDropMentions(f.Views, ViewDropReason::ZeroAreaTarget, "render target 'collapsed' has zero area (0x300)"));
}

TEST_CASE("0, 1 and N cameras go through the same build")
{
    for (const uint64_t count : { 0u, 1u, 5u })
    {
        CAPTURE(count);
        Fixture f;
        HX::RenderScene scene;
        for (uint64_t i = 0; i < count; ++i)
            scene.Cameras.push_back(MakeCamera(100 + i));

        const std::vector<View>& views = f.Views.BuildViews(scene);
        CHECK(views.size() == count);
        CHECK(f.Views.GetDroppedViews().empty());
        for (size_t i = 1; i < views.size(); ++i)
            CHECK(views[i - 1].Id < views[i].Id); // ascending ViewId
    }
}
