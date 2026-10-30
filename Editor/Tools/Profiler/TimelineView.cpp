#include "TimelineView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Editor
{
    namespace
    {
        // The view's span kept, moved back inside the frame.
        void KeepInside(TimelineView& view)
        {
            const double span = std::min(view.Span(), 1.0);
            view.Begin        = std::clamp(view.Begin, 0.0, 1.0 - span);
            view.End          = view.Begin + span;
        }
    }

    void ZoomTimeline(TimelineView& view, double anchor, double factor, double frameNs)
    {
        if (!(factor > 0.0) || !std::isfinite(factor))
            return;
        const double minSpan = frameNs > MIN_TIMELINE_SPAN_NS ? MIN_TIMELINE_SPAN_NS / frameNs : 1.0;
        const double span    = std::clamp(view.Span() / factor, minSpan, 1.0);
        const double at      = view.Begin + std::clamp(anchor, 0.0, 1.0) * view.Span();
        view.Begin           = at - std::clamp(anchor, 0.0, 1.0) * span;
        view.End             = view.Begin + span;
        KeepInside(view);
    }

    void PanTimeline(TimelineView& view, double delta)
    {
        const double shift = delta * view.Span();
        view.Begin += shift;
        view.End += shift;
        KeepInside(view);
    }

    double ChooseTickStep(double spanMs, float width, float minPixels)
    {
        if (!(spanMs > 0.0) || !(width > 0.0f))
            return 1.0;
        const double minStep = spanMs * static_cast<double>(minPixels) / static_cast<double>(width);
        double       power   = std::pow(10.0, std::floor(std::log10(minStep)));
        for (;;)
        {
            for (const double multiple : { 1.0, 2.0, 5.0 })
            {
                if (multiple * power >= minStep * (1.0 - 1e-9))
                    return multiple * power;
            }
            power *= 10.0;
        }
    }

    std::string FormatTickLabel(double timeMs, double stepMs)
    {
        const bool   inMs     = stepMs >= 1.0;
        const double value    = inMs ? timeMs : timeMs * 1000.0;
        const double step     = inMs ? stepMs : stepMs * 1000.0;
        const int    decimals = std::max(0, -static_cast<int>(std::floor(std::log10(step) + 1e-9)));
        char         text[32];
        std::snprintf(text, sizeof(text), "%.*f %s", decimals, value, inMs ? "ms" : "us");
        return text;
    }
}
