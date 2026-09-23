#include "HedgehogRenderer/Graph/GraphBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

TEST_CASE("AddOutputSlot returns the slot's index, in declaration order")
{
    GraphBuilder builder;

    const uint32_t colorSlot = builder.AddOutputSlot(
        "color", RHI::Format::R16G16B16A16Float, RGSizePolicy::MakeRelativeToResult(1.0f));
    const uint32_t depthSlot = builder.AddOutputSlot(
        "depth", RHI::Format::D32Float, RGSizePolicy::MakeRelativeToResult(1.0f));

    CHECK(colorSlot == 0);
    CHECK(depthSlot == 1);

    const auto& slots = builder.GetDescription().OutputSlots;
    REQUIRE(slots.size() == 2);
    CHECK(slots[colorSlot].Format == RHI::Format::R16G16B16A16Float);
    CHECK(slots[depthSlot].Format == RHI::Format::D32Float);
}

TEST_CASE("Each output slot carries its own format and size policy, independent of its name")
{
    GraphBuilder builder;
    builder.AddOutputSlot("bloom", RHI::Format::R16G16B16A16Float, RGSizePolicy::MakeRelativeToResult(0.5f));

    const RGOutputSlot& slot = builder.GetDescription().OutputSlots[0];
    CHECK(slot.Name == "bloom");
    CHECK(slot.Format == RHI::Format::R16G16B16A16Float);
    CHECK(slot.Size.Kind == RGSizePolicyKind::RelativeToResult);
    CHECK(slot.Size.Scale == doctest::Approx(0.5f));
}

TEST_CASE("Two slots may share a name — binding is by index, names are diagnostics only")
{
    GraphBuilder builder;
    const uint32_t first  = builder.AddOutputSlot("target", RHI::Format::R8G8B8A8Unorm, {});
    const uint32_t second = builder.AddOutputSlot("target", RHI::Format::D32Float, {});

    CHECK(first != second);
    const auto& slots = builder.GetDescription().OutputSlots;
    CHECK(slots[first].Format != slots[second].Format);
}
