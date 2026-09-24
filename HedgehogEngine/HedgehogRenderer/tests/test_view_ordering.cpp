#include "HedgehogRenderer/Views/ViewManager.hpp"
#include "HedgehogRenderer/Views/ViewOrdering.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Renderer;
using namespace RGTest;

namespace
{
    View MakeView(ViewId id, std::vector<std::string> writes, std::vector<std::string> reads = {},
                  int32_t priority = 0)
    {
        View view;
        view.Id            = id;
        view.Desc.Targets  = std::move(writes);
        view.Desc.Reads    = std::move(reads);
        view.Desc.Priority = priority;
        return view;
    }

    std::vector<ViewId> IdsOf(const std::vector<View>& views)
    {
        std::vector<ViewId> ids;
        for (const View& view : views)
            ids.push_back(view.Id);
        return ids;
    }

    std::vector<ViewId> Order(std::vector<View> views, std::vector<DroppedView>& dropped)
    {
        return IdsOf(OrderViews(std::move(views), dropped));
    }
}

TEST_CASE("A view that samples another view's target runs after it, with no priority set")
{
    std::vector<DroppedView> dropped;
    // The reader has the lower ViewId, so without the dependency it would run first.
    CHECK(Order({ MakeView(1, { "main" }, { "panel" }), MakeView(2, { "panel" }) }, dropped)
          == std::vector<ViewId>{ 2, 1 });
    CHECK(dropped.empty());
}

TEST_CASE("Priority orders only views with no dependency between them")
{
    std::vector<DroppedView> dropped;

    SUBCASE("independent views: lower priority first, then lower ViewId")
    {
        CHECK(Order({ MakeView(1, { "a" }, {}, 5), MakeView(2, { "b" }, {}, -1), MakeView(3, { "c" }, {}, 5) }, dropped)
              == std::vector<ViewId>{ 2, 1, 3 });
    }
    SUBCASE("a dependency beats any priority")
    {
        // The reader asks to go first (-10) and the writer last (10): the dependency still wins.
        CHECK(Order({ MakeView(1, { "main" }, { "panel" }, -10), MakeView(2, { "panel" }, {}, 10) }, dropped)
              == std::vector<ViewId>{ 2, 1 });
    }
    CHECK(dropped.empty());
}

TEST_CASE("A chain of three dependent views orders correctly")
{
    std::vector<DroppedView> dropped;
    // Declared back to front, with priorities that would reverse them if they were consulted.
    CHECK(Order({ MakeView(1, { "c" }, { "b" }, -1), MakeView(2, { "b" }, { "a" }, 0), MakeView(3, { "a" }, {}, 1) }, dropped)
          == std::vector<ViewId>{ 3, 2, 1 });
    CHECK(dropped.empty());
}

TEST_CASE("A monitor showing another monitor's feed needs no ordering rule")
{
    std::vector<DroppedView> dropped;
    // Security camera -> lobby monitor texture -> a camera filming the lobby -> main.
    CHECK(Order({ MakeView(1, { "main" }, { "lobbyFeed" }), MakeView(2, { "lobbyFeed" }, { "securityFeed" }),
                  MakeView(3, { "securityFeed" }) }, dropped)
          == std::vector<ViewId>{ 3, 2, 1 });
}

TEST_CASE("A dependency cycle is reported naming both views, and one is dropped instead of hanging")
{
    std::vector<DroppedView> dropped;
    View first  = MakeView(1, { "a" }, { "b" }, 0);
    View second = MakeView(2, { "b" }, { "a" }, 0);
    first.Desc.GraphName  = "mirrorA";
    second.Desc.GraphName = "mirrorB";
    View downstream = MakeView(3, { "main" }, { "a" });

    const std::vector<ViewId> order = Order({ first, second, downstream }, dropped);

    REQUIRE(dropped.size() == 1);
    CHECK(dropped[0].Reason == ViewDropReason::Cycle);
    CHECK(dropped[0].Id == 2); // equal priority: the newest leaves
    CHECK(dropped[0].Message.find("view 1 ('mirrorA')") != std::string::npos);
    CHECK(dropped[0].Message.find("view 2 ('mirrorB')") != std::string::npos);
    CHECK(dropped[0].Message.find("dropping view 2 ('mirrorB') for this frame") != std::string::npos);
    // The survivor and the view behind the cycle still render, in dependency order.
    CHECK(order == std::vector<ViewId>{ 1, 3 });
}

TEST_CASE("Cycle handling: lowest priority leaves, self-reads are cycles, and several cycles all resolve")
{
    SUBCASE("the lower-priority view is the one dropped")
    {
        std::vector<DroppedView> dropped;
        CHECK(Order({ MakeView(1, { "a" }, { "b" }, -3), MakeView(2, { "b" }, { "a" }, 4) }, dropped)
              == std::vector<ViewId>{ 2 });
        REQUIRE(dropped.size() == 1);
        CHECK(dropped[0].Id == 1);
    }
    SUBCASE("a camera that can see its own target")
    {
        std::vector<DroppedView> dropped;
        CHECK(Order({ MakeView(1, { "mirror" }, { "mirror" }), MakeView(2, { "main" }) }, dropped)
              == std::vector<ViewId>{ 2 });
        REQUIRE(dropped.size() == 1);
        CHECK(dropped[0].Message.find("view 1 reads its own target") != std::string::npos);
    }
    SUBCASE("two independent cycles")
    {
        std::vector<DroppedView> dropped;
        const std::vector<ViewId> order = Order({ MakeView(1, { "a" }, { "b" }), MakeView(2, { "b" }, { "a" }),
                                                  MakeView(3, { "c" }, { "d" }), MakeView(4, { "d" }, { "c" }) },
                                                dropped);
        CHECK(dropped.size() == 2);
        CHECK(order.size() == 2);
    }
}

TEST_CASE("The editor's scene, game and result order emerges from the data")
{
    TestDevice           device;
    TestSwapchain        swapchain(1280, 720);
    RenderTargetRegistry targets(device, swapchain);
    REQUIRE(targets.Declare({ "scene", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(640, 480) }).Success);
    REQUIRE(targets.Declare({ "game", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(640, 480) }).Success);
    ViewManager views(targets);

    // Created result-first: nothing about creation order or ids favours the right answer.
    ViewDesc result;
    result.Targets   = { "main" };
    result.Reads     = { "scene", "game" };
    result.GraphName = "result";
    const ViewId resultId = views.CreateView(result);

    ViewDesc scene;
    scene.Targets   = { "scene" };
    scene.GraphName = "scene";
    scene.Camera    = HX::RenderCamera{};
    const ViewId sceneId = views.CreateView(scene);

    // The game view is derived from a camera, redirected into the game panel.
    HX::RenderScene renderScene;
    HX::RenderCamera gameCamera;
    gameCamera.SourceId = 42;
    renderScene.Cameras = { gameCamera };
    views.SetTargetOverride(42, { "game" });

    const std::vector<View>& built = views.BuildViews(renderScene);
    REQUIRE(built.size() == 3);
    CHECK(built[0].Id == sceneId);
    CHECK(built[1].SourceId == 42);
    CHECK(built[2].Id == resultId);
    CHECK(views.GetDroppedViews().empty());
}
