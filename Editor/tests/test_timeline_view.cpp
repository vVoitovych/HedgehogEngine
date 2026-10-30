#include "Tools/Profiler/TimelineView.hpp"

#include "doctest/doctest/doctest.h"

using namespace Editor;

namespace
{
    constexpr double FRAME_NS = 10.0e6; // a 10 ms frame
}

TEST_CASE("Timeline view - zooming keeps the time under the anchor in place and stays inside the frame")
{
    TimelineView view;
    ZoomTimeline(view, 0.25, 4.0, FRAME_NS);
    CHECK(view.Begin == doctest::Approx(0.1875));
    CHECK(view.Span() == doctest::Approx(0.25));
    // The time under the anchor before (0.25 of the frame) is still under it.
    CHECK(view.Begin + 0.25 * view.Span() == doctest::Approx(0.25));

    // Zooming out past the whole frame shows the whole frame.
    ZoomTimeline(view, 0.5, 0.01, FRAME_NS);
    CHECK(view.Begin == doctest::Approx(0.0));
    CHECK(view.End == doctest::Approx(1.0));

    // Zooming in at the right edge keeps the view inside the frame.
    ZoomTimeline(view, 1.0, 10.0, FRAME_NS);
    CHECK(view.End == doctest::Approx(1.0));
    CHECK(view.Span() == doctest::Approx(0.1));

    // No closer than MIN_TIMELINE_SPAN_NS, and a bad factor changes nothing.
    ZoomTimeline(view, 0.5, 1.0e9, FRAME_NS);
    CHECK(view.Span() * FRAME_NS == doctest::Approx(MIN_TIMELINE_SPAN_NS));
    const TimelineView before = view;
    ZoomTimeline(view, 0.5, 0.0, FRAME_NS);
    ZoomTimeline(view, 0.5, -2.0, FRAME_NS);
    CHECK(view.Begin == before.Begin);
    CHECK(view.End == before.End);
}

TEST_CASE("Timeline view - panning moves by fractions of the view and stops at the frame's ends")
{
    TimelineView view{ 0.4, 0.6 };
    PanTimeline(view, 0.5);
    CHECK(view.Begin == doctest::Approx(0.5));
    CHECK(view.End == doctest::Approx(0.7));

    PanTimeline(view, 10.0);
    CHECK(view.Begin == doctest::Approx(0.8));
    CHECK(view.End == doctest::Approx(1.0));

    PanTimeline(view, -100.0);
    CHECK(view.Begin == doctest::Approx(0.0));
    CHECK(view.End == doctest::Approx(0.2));

    // The whole frame cannot move.
    TimelineView whole;
    PanTimeline(whole, 0.3);
    CHECK(whole.Begin == doctest::Approx(0.0));
    CHECK(whole.End == doctest::Approx(1.0));
}

TEST_CASE("Timeline ruler - steps of 1, 2 or 5 times a power of ten, labelled in ms or us")
{
    // 10 ms over 1000 px with 80 px between ticks: at least 0.8 ms, so 1 ms.
    CHECK(ChooseTickStep(10.0, 1000.0f, 80.0f) == doctest::Approx(1.0));
    CHECK(ChooseTickStep(10.0, 300.0f, 80.0f) == doctest::Approx(5.0));
    CHECK(ChooseTickStep(10.0, 500.0f, 80.0f) == doctest::Approx(2.0));
    CHECK(ChooseTickStep(0.05, 1000.0f, 80.0f) == doctest::Approx(0.005));
    CHECK(ChooseTickStep(0.002, 800.0f, 80.0f) == doctest::Approx(0.0002));
    // Exactly a step's worth of pixels takes that step.
    CHECK(ChooseTickStep(1.0, 100.0f, 20.0f) == doctest::Approx(0.2));
    CHECK(ChooseTickStep(0.0, 100.0f, 20.0f) == doctest::Approx(1.0));

    CHECK(FormatTickLabel(4.0, 1.0) == "4 ms");
    CHECK(FormatTickLabel(20.0, 10.0) == "20 ms");
    CHECK(FormatTickLabel(0.25, 0.05) == "250 us");
    CHECK(FormatTickLabel(1.2346, 0.005) == "1235 us"); // rounded to the step's precision
    CHECK(FormatTickLabel(0.0015, 0.0005) == "1.5 us");
    CHECK(FormatTickLabel(0.0012, 0.0002) == "1.2 us");
}
