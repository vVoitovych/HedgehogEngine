#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"

#include "doctest/doctest/doctest.h"

#include <ostream> // doctest streams std::string_view values when a check fails
#include <set>
#include <string>

using namespace Renderer;

namespace
{
    // RHI::Format's enumerators run contiguously from Undefined (0) to D32FloatS8Uint.
    constexpr uint32_t DEFINED_FORMAT_COUNT = static_cast<uint32_t>(RHI::Format::D32FloatS8Uint);
}

TEST_CASE("Every format string resolves to exactly one engine format, and every format has one string")
{
    const auto vocabulary = GetFormatVocabulary();
    REQUIRE(vocabulary.size() == DEFINED_FORMAT_COUNT);

    std::set<std::string_view> names;
    std::set<RHI::Format>      values;
    for (const auto& entry : vocabulary)
    {
        CHECK(names.insert(entry.Name).second);
        CHECK(values.insert(entry.Value).second);
        CHECK(entry.Value != RHI::Format::Undefined);

        const std::optional<RHI::Format> resolved = ResolveFormat(entry.Name);
        REQUIRE(resolved.has_value());
        CHECK(*resolved == entry.Value);
    }
}

TEST_CASE("The two-channel half-float format, the BRDF lookup table's, has its name")
{
    const std::optional<RHI::Format> resolved = ResolveFormat("R16G16Float");
    REQUIRE(resolved.has_value());
    CHECK(*resolved == RHI::Format::R16G16Float);
}

TEST_CASE("Unknown or near-miss format strings are rejected, never defaulted")
{
    CHECK_FALSE(ResolveFormat("").has_value());
    CHECK_FALSE(ResolveFormat("Undefined").has_value());
    CHECK_FALSE(ResolveFormat("RGBA16F").has_value());
    CHECK_FALSE(ResolveFormat("d32float").has_value());
    CHECK_FALSE(ResolveFormat(" D32Float").has_value());
    CHECK_FALSE(ResolveFormat("D32Float ").has_value());
}

TEST_CASE("Size policies resolve with their arguments")
{
    SUBCASE("Absolute")
    {
        const auto policy = ResolveSizePolicy("Absolute(2048, 1024)");
        REQUIRE(policy.has_value());
        CHECK(policy->Kind == RGSizePolicyKind::Absolute);
        CHECK(policy->Width == 2048);
        CHECK(policy->Height == 1024);
    }
    SUBCASE("RelativeToResult")
    {
        const auto policy = ResolveSizePolicy("RelativeToResult(0.5)");
        REQUIRE(policy.has_value());
        CHECK(policy->Kind == RGSizePolicyKind::RelativeToResult);
        CHECK(policy->Scale == doctest::Approx(0.5f));
    }
    SUBCASE("RelativeToSwapchain")
    {
        const auto policy = ResolveSizePolicy("RelativeToSwapchain( 1 )");
        REQUIRE(policy.has_value());
        CHECK(policy->Kind == RGSizePolicyKind::RelativeToSwapchain);
        CHECK(policy->Scale == doctest::Approx(1.0f));
    }
}

TEST_CASE("Malformed size policies are rejected")
{
    for (const char* text : {
             "",
             "Absolute",
             "Absolute()",
             "Absolute(1024)",
             "Absolute(1024, 1024, 1)",
             "Absolute(0, 1024)",
             "Absolute(-1, 1024)",
             "Absolute(1024px, 1024)",
             "Absolute(1024,,1024)",
             "Absolute(1024, 1024",
             "absolute(1024, 1024)",
             "RelativeToResult()",
             "RelativeToResult(0)",
             "RelativeToResult(-0.5)",
             "RelativeToResult(half)",
             "RelativeToResult(1e2)",
             "RelativeToResult(0.5, 0.5)",
             "RelativeToView(1.0)",
             " RelativeToResult(1.0)",
         })
    {
        CAPTURE(text);
        CHECK_FALSE(ResolveSizePolicy(text).has_value());
    }
}

TEST_CASE("Flags accept exactly 'true' and 'false'")
{
    CHECK(ResolveFlag("true") == std::optional<bool>(true));
    CHECK(ResolveFlag("false") == std::optional<bool>(false));

    for (const char* text : { "", "True", "FALSE", "1", "0", "yes", "no", "on", "off" })
    {
        CAPTURE(text);
        CHECK_FALSE(ResolveFlag(text).has_value());
    }
}

TEST_CASE("Every format has its vocabulary string back, and Undefined has none")
{
    for (const auto& entry : GetFormatVocabulary())
    {
        CAPTURE(entry.Name);
        const std::optional<std::string_view> name = GetFormatName(entry.Value);
        REQUIRE(name.has_value());
        CHECK(std::string(*name) == std::string(entry.Name));
    }
    CHECK_FALSE(GetFormatName(RHI::Format::Undefined).has_value());
}

TEST_CASE("Size policies are written in the canonical spelling and read back exactly")
{
    CHECK(SizePolicyToString(RGSizePolicy::MakeAbsolute(2048, 1024)) == "Absolute(2048, 1024)");
    CHECK(SizePolicyToString(RGSizePolicy::MakeRelativeToResult(1.0f)) == "RelativeToResult(1.0)");
    CHECK(SizePolicyToString(RGSizePolicy::MakeRelativeToResult(0.5f)) == "RelativeToResult(0.5)");
    CHECK(SizePolicyToString(RGSizePolicy::MakeRelativeToSwapchain(2.0f)) == "RelativeToSwapchain(2.0)");

    for (const RGSizePolicy& policy : {
             RGSizePolicy::MakeAbsolute(1, 1),
             RGSizePolicy::MakeAbsolute(4096, 4096),
             RGSizePolicy::MakeRelativeToResult(0.1f),
             RGSizePolicy::MakeRelativeToResult(1.0f / 3.0f),
             RGSizePolicy::MakeRelativeToSwapchain(0.25f),
             RGSizePolicy::MakeRelativeToSwapchain(1.5f),
         })
    {
        const std::string text = SizePolicyToString(policy);
        CAPTURE(text);
        const std::optional<RGSizePolicy> readBack = ResolveSizePolicy(text);
        REQUIRE(readBack.has_value());
        CHECK(*readBack == policy); // exact, not approximate: the scale's bits survive
    }
}

