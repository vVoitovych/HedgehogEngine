#include "doctest/doctest/doctest.h"

#include "Docking/DockLayout.hpp"

#include <array>

using namespace Editor;

namespace
{
    constexpr float MENU_BAR_HEIGHT = 22.0f;

    [[nodiscard]] DockLayoutState MakeDefaultLayout()
    {
        DockLayoutState layout;
        layout.InitDefaults();
        return layout;
    }

    [[nodiscard]] float Right(const DockRect& rect) { return rect.X + rect.Width; }
    [[nodiscard]] float Bottom(const DockRect& rect) { return rect.Y + rect.Height; }
    [[nodiscard]] float Area(const DockRect& rect) { return rect.Width * rect.Height; }

    [[nodiscard]] bool Overlap(const DockRect& a, const DockRect& b)
    {
        return a.X < Right(b) && b.X < Right(a) && a.Y < Bottom(b) && b.Y < Bottom(a);
    }

    [[nodiscard]] std::array<DockRect, 8> AllRects(const DockGeometry& geometry)
    {
        return { geometry.Toolbar, geometry.Left, geometry.Center, geometry.Bottom, geometry.Right,
                 geometry.LeftSplitter, geometry.BottomSplitter, geometry.RightSplitter };
    }

    void CheckNonNegative(const DockGeometry& geometry)
    {
        for (const DockRect& rect : AllRects(geometry))
        {
            CHECK(rect.Width >= 0.0f);
            CHECK(rect.Height >= 0.0f);
        }
    }
}

TEST_CASE("The default layout at 1920x1080 matches the reference arrangement")
{
    const DockGeometry geometry = ComputeDockGeometry(MakeDefaultLayout(), 1920.0f, 1080.0f, MENU_BAR_HEIGHT);

    SUBCASE("the toolbar runs the full width under the menu bar")
    {
        CHECK(geometry.Toolbar.X == 0.0f);
        CHECK(geometry.Toolbar.Y == MENU_BAR_HEIGHT);
        CHECK(geometry.Toolbar.Width == 1920.0f);
        CHECK(geometry.Toolbar.Height == DOCK_TOOLBAR_HEIGHT);
    }

    SUBCASE("the Inspector's area is full height below the toolbar, at the right edge")
    {
        CHECK(geometry.Right.Y == Bottom(geometry.Toolbar));
        CHECK(Bottom(geometry.Right) == 1080.0f);
        CHECK(Right(geometry.Right) == 1920.0f);
        CHECK(geometry.Right.Width == 340.0f);
    }

    SUBCASE("Left and Center share the upper band, and Bottom spans under both from the left edge")
    {
        CHECK(geometry.Left.X == 0.0f);
        CHECK(geometry.Left.Width == 280.0f);
        CHECK(geometry.Left.Y == geometry.Center.Y);
        CHECK(geometry.Left.Height == geometry.Center.Height);
        CHECK(geometry.Bottom.X == 0.0f);
        CHECK(Right(geometry.Bottom) == Right(geometry.Center));
        CHECK(geometry.Bottom.Height == 240.0f);
        CHECK(Bottom(geometry.Bottom) == 1080.0f);
    }

    SUBCASE("no two rectangles overlap and together they tile the window below the menu bar")
    {
        const auto rects = AllRects(geometry);
        float      area  = 0.0f;
        for (size_t i = 0; i < rects.size(); ++i)
        {
            area += Area(rects[i]);
            for (size_t j = i + 1; j < rects.size(); ++j)
            {
                CAPTURE(i);
                CAPTURE(j);
                CHECK_FALSE(Overlap(rects[i], rects[j]));
            }
        }
        CHECK(area == doctest::Approx(1920.0f * (1080.0f - MENU_BAR_HEIGHT)));
    }

    SUBCASE("the splitters sit between the areas they resize")
    {
        CHECK(geometry.LeftSplitter.X == Right(geometry.Left));
        CHECK(Right(geometry.LeftSplitter) == geometry.Center.X);
        CHECK(geometry.LeftSplitter.Height == geometry.Left.Height);
        CHECK(geometry.BottomSplitter.Y == Bottom(geometry.Center));
        CHECK(Bottom(geometry.BottomSplitter) == geometry.Bottom.Y);
        CHECK(geometry.BottomSplitter.Width == geometry.Bottom.Width);
        CHECK(geometry.RightSplitter.X == Right(geometry.Bottom));
        CHECK(Right(geometry.RightSplitter) == geometry.Right.X);
        CHECK(geometry.RightSplitter.Height == geometry.Right.Height);
    }

    SUBCASE("GetAreaRect names each docked area and nothing for Floating")
    {
        CHECK(GetAreaRect(geometry, DockArea::Left).Width == geometry.Left.Width);
        CHECK(GetAreaRect(geometry, DockArea::TopCenter).Y == geometry.Toolbar.Y);
        CHECK(GetAreaRect(geometry, DockArea::Center).X == geometry.Center.X);
        CHECK(GetAreaRect(geometry, DockArea::Bottom).Y == geometry.Bottom.Y);
        CHECK(GetAreaRect(geometry, DockArea::Right).X == geometry.Right.X);
        CHECK(Area(GetAreaRect(geometry, DockArea::Floating)) == 0.0f);
    }
}

TEST_CASE("A small window keeps every area at its minimum and Center at its minimum width")
{
    DockLayoutState layout = MakeDefaultLayout();
    layout.LeftWidth    = 10.0f;
    layout.RightWidth   = 5000.0f;
    layout.BottomHeight = 5000.0f;

    const DockGeometry geometry = ComputeDockGeometry(layout, 640.0f, 480.0f, MENU_BAR_HEIGHT);
    CheckNonNegative(geometry);
    CHECK(geometry.Left.Width == DOCK_MIN_AREA_SIZE);
    CHECK(geometry.Center.Width >= DOCK_MIN_CENTER_WIDTH);
    CHECK(geometry.Center.Height >= DOCK_MIN_AREA_SIZE);
    CHECK(geometry.Bottom.Height >= DOCK_MIN_AREA_SIZE);
    CHECK(Right(geometry.Right) == doctest::Approx(640.0f));
    CHECK(Bottom(geometry.Bottom) == doctest::Approx(480.0f));
}

TEST_CASE("An oversized saved width is clamped so Center keeps its minimum width")
{
    DockLayoutState layout = MakeDefaultLayout();
    layout.LeftWidth = 5000.0f;

    const DockGeometry geometry = ComputeDockGeometry(layout, 1920.0f, 1080.0f, MENU_BAR_HEIGHT);
    CHECK(geometry.Center.Width == doctest::Approx(DOCK_MIN_CENTER_WIDTH));
    CHECK(geometry.Right.Width == 340.0f);
    CHECK(Right(geometry.Right) == doctest::Approx(1920.0f));
}

TEST_CASE("A window too small for the minimums never gives a negative size")
{
    for (const float size : { 100.0f, 10.0f, 0.0f })
    {
        CAPTURE(size);
        const DockGeometry geometry = ComputeDockGeometry(MakeDefaultLayout(), size, size, MENU_BAR_HEIGHT);
        CheckNonNegative(geometry);
    }
}
