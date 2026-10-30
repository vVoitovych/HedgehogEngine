#pragma once

#include <string>

// The part of a frame the Profiler window's timeline shows, and the ticks of its ruler, worked out
// without ImGui so EditorTest covers it.
namespace Editor
{
    // The shortest span the timeline zooms to, in nanoseconds.
    inline constexpr double MIN_TIMELINE_SPAN_NS = 2000.0;

    // From Begin to End, fractions of the frame's span; the whole frame by default.
    struct TimelineView
    {
        double Begin = 0.0;
        double End   = 1.0;

        [[nodiscard]] double Span() const { return End - Begin; }
    };

    // Zooms in by factor (out for a factor below 1) about anchor, a fraction across the view, which
    // keeps the time under it in place; the span stays between MIN_TIMELINE_SPAN_NS of the frame
    // and the whole frame, and the view inside the frame.
    void ZoomTimeline(TimelineView& view, double anchor, double factor, double frameNs);

    // Moves the view by delta, a fraction of its own span (positive later), keeping it inside the
    // frame.
    void PanTimeline(TimelineView& view, double delta);

    // The ruler's step in milliseconds: the smallest 1, 2 or 5 times a power of ten that leaves at
    // least minPixels between ticks over a span of spanMs drawn width pixels wide.
    [[nodiscard]] double ChooseTickStep(double spanMs, float width, float minPixels);

    // A tick's label: the time in milliseconds from the frame's start in ms for a step of at least
    // 1 ms, else in us, with as many decimals as the step needs ("4 ms", "250 us", "1.5 us").
    [[nodiscard]] std::string FormatTickLabel(double timeMs, double stepMs);
}
