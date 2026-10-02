#include "doctest/doctest/doctest.h"

#include "HedgehogUI/api/RectTransform.hpp"

#include <cmath>

using namespace HUI;

namespace
{
    bool Near(const HX::UiRect& a, const HX::UiRect& b)
    {
        constexpr float EPSILON = 1e-3f;
        return std::abs(a.X - b.X) < EPSILON && std::abs(a.Y - b.Y) < EPSILON &&
               std::abs(a.Width - b.Width) < EPSILON && std::abs(a.Height - b.Height) < EPSILON;
    }

    RectTransform Pinned(HM::Vector2 anchor, HM::Vector2 pivot, HM::Vector2 offset, HM::Vector2 size)
    {
        return { anchor, anchor, pivot, offset, size };
    }

    const HX::UiRect SMALL = { 0.0f, 0.0f, 800.0f, 600.0f };
    const HX::UiRect LARGE = { 0.0f, 0.0f, 1920.0f, 1080.0f };
}

TEST_CASE("Layout - pinned anchors place the pivot at the anchor point plus the offset")
{
    // The default: centred, pivot in the middle.
    RectTransform centred;
    centred.Size = HM::Vector2(100.0f, 50.0f);
    CHECK(Near(ResolveRect(centred, SMALL), { 350.0f, 275.0f, 100.0f, 50.0f }));
    centred.Offset = HM::Vector2(10.0f, -20.0f);
    CHECK(Near(ResolveRect(centred, SMALL), { 360.0f, 255.0f, 100.0f, 50.0f }));

    // Top-left corner: the same rect at any canvas size.
    const RectTransform topLeft = Pinned({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 20.0f, 30.0f }, { 200.0f, 40.0f });
    CHECK(Near(ResolveRect(topLeft, SMALL), { 20.0f, 30.0f, 200.0f, 40.0f }));
    CHECK(Near(ResolveRect(topLeft, LARGE), { 20.0f, 30.0f, 200.0f, 40.0f }));

    // Bottom-right corner: follows the corner as the canvas grows.
    const RectTransform bottomRight = Pinned({ 1.0f, 1.0f }, { 1.0f, 1.0f }, { -10.0f, -10.0f }, { 100.0f, 100.0f });
    CHECK(Near(ResolveRect(bottomRight, SMALL), { 690.0f, 490.0f, 100.0f, 100.0f }));
    CHECK(Near(ResolveRect(bottomRight, LARGE), { 1810.0f, 970.0f, 100.0f, 100.0f }));
}

TEST_CASE("Layout - stretched anchors follow the parent, with Size added to their span")
{
    // Fill the parent with a 10-unit margin.
    const RectTransform fill = { { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f }, { -20.0f, -20.0f } };
    CHECK(Near(ResolveRect(fill, SMALL), { 10.0f, 10.0f, 780.0f, 580.0f }));
    CHECK(Near(ResolveRect(fill, LARGE), { 10.0f, 10.0f, 1900.0f, 1060.0f }));

    // A bar along the bottom: stretched in x, pinned to the bottom edge in y.
    const RectTransform bar = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.5f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 50.0f } };
    CHECK(Near(ResolveRect(bar, SMALL), { 0.0f, 550.0f, 800.0f, 50.0f }));
    CHECK(Near(ResolveRect(bar, LARGE), { 0.0f, 1030.0f, 1920.0f, 50.0f }));

    // The right half, offset by its pivot's shift.
    const RectTransform half = { { 0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 5.0f, 0.0f }, { 0.0f, 0.0f } };
    CHECK(Near(ResolveRect(half, SMALL), { 405.0f, 0.0f, 400.0f, 600.0f }));
}

TEST_CASE("Layout - a tree resolves top-down, each child inside its parent's rect")
{
    RectTransform panel;
    panel.Size                 = HM::Vector2(100.0f, 50.0f);
    const HX::UiRect panelRect = ResolveRect(panel, SMALL);

    const RectTransform icon = Pinned({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 5.0f, 5.0f }, { 10.0f, 10.0f });
    CHECK(Near(ResolveRect(icon, panelRect), { 355.0f, 280.0f, 10.0f, 10.0f }));

    const RectTransform inset = { { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f }, { -4.0f, -4.0f } };
    CHECK(Near(ResolveRect(inset, panelRect), { 352.0f, 277.0f, 96.0f, 46.0f }));
}

TEST_CASE("Layout - canvas scale modes")
{
    CanvasScaler constant;
    CHECK(ComputeCanvasScale(constant, { 960.0f, 540.0f }) == 1.0f);
    CHECK(ComputeCanvasScale(constant, { 3840.0f, 2160.0f }) == 1.0f);

    CanvasScaler scaled;
    scaled.Mode                = CanvasScaleMode::ScaleWithTargetSize;
    scaled.ReferenceResolution = HM::Vector2(1920.0f, 1080.0f);
    CHECK(ComputeCanvasScale(scaled, { 960.0f, 540.0f }) == doctest::Approx(0.5f));
    CHECK(ComputeCanvasScale(scaled, { 3840.0f, 2160.0f }) == doctest::Approx(2.0f));

    // A target of another aspect: by width, by height, and halfway in log space.
    CHECK(ComputeCanvasScale(scaled, { 1920.0f, 540.0f }) == doctest::Approx(1.0f));
    scaled.MatchWidthOrHeight = 1.0f;
    CHECK(ComputeCanvasScale(scaled, { 1920.0f, 540.0f }) == doctest::Approx(0.5f));
    scaled.MatchWidthOrHeight = 0.5f;
    CHECK(ComputeCanvasScale(scaled, { 3840.0f, 540.0f }) == doctest::Approx(1.0f));

    // Nothing sensible to scale by.
    CHECK(ComputeCanvasScale(scaled, { 0.0f, 540.0f }) == 1.0f);
    scaled.ReferenceResolution = HM::Vector2(0.0f, 1080.0f);
    CHECK(ComputeCanvasScale(scaled, { 960.0f, 540.0f }) == 1.0f);
}

TEST_CASE("Layout - a reference-resolution canvas lays out at the reference size and scales to the target")
{
    CanvasScaler scaler;
    scaler.Mode                = CanvasScaleMode::ScaleWithTargetSize;
    scaler.ReferenceResolution = HM::Vector2(1920.0f, 1080.0f);
    const RectTransform corner = Pinned({ 1.0f, 1.0f }, { 1.0f, 1.0f }, { -10.0f, -10.0f }, { 100.0f, 100.0f });

    for (const float width : { 960.0f, 1920.0f, 3840.0f })
    {
        CAPTURE(width);
        const HM::Vector2 target(width, width * 9.0f / 16.0f);
        const float       scale  = ComputeCanvasScale(scaler, target);
        const HX::UiRect  canvas = CanvasRect(target, scale);
        CHECK(Near(canvas, { 0.0f, 0.0f, 1920.0f, 1080.0f }));

        // Always 10 reference units from the corner and 100 across: proportional on screen.
        const float k = width / 1920.0f;
        CHECK(Near(ToPixels(ResolveRect(corner, canvas), scale), { 1810.0f * k, 970.0f * k, 100.0f * k, 100.0f * k }));
    }
}
