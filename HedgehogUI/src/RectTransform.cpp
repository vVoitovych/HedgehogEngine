#include "HedgehogUI/api/RectTransform.hpp"

#include <cmath>

namespace HUI
{
    namespace
    {
        struct Span
        {
            float Min  = 0.0f;
            float Size = 0.0f;
        };

        // One axis of ResolveRect.
        Span ResolveAxis(float parentMin, float parentSize, float anchorMin, float anchorMax, float pivot, float offset,
                         float size)
        {
            const float resolvedSize = parentSize * (anchorMax - anchorMin) + size;
            const float reference    = parentMin + parentSize * (anchorMin + (anchorMax - anchorMin) * pivot);
            return { reference + offset - pivot * resolvedSize, resolvedSize };
        }
    }

    HX::UiRect ResolveRect(const RectTransform& transform, const HX::UiRect& parent)
    {
        const Span x = ResolveAxis(parent.X, parent.Width, transform.AnchorMin.x(), transform.AnchorMax.x(),
                                   transform.Pivot.x(), transform.Offset.x(), transform.Size.x());
        const Span y = ResolveAxis(parent.Y, parent.Height, transform.AnchorMin.y(), transform.AnchorMax.y(),
                                   transform.Pivot.y(), transform.Offset.y(), transform.Size.y());
        return { x.Min, y.Min, x.Size, y.Size };
    }

    float ComputeCanvasScale(const CanvasScaler& scaler, const HM::Vector2& targetSize)
    {
        if (scaler.Mode == CanvasScaleMode::ConstantPixelSize)
            return 1.0f;

        const HM::Vector2& reference = scaler.ReferenceResolution;
        if (!(reference.x() > 0.0f && reference.y() > 0.0f && targetSize.x() > 0.0f && targetSize.y() > 0.0f))
            return 1.0f;

        // Blending in log space keeps the scale symmetric: 2x wider and 2x shorter at 0.5 is 1.
        const float logWidth  = std::log2(targetSize.x() / reference.x());
        const float logHeight = std::log2(targetSize.y() / reference.y());
        const float match     = std::fmin(std::fmax(scaler.MatchWidthOrHeight, 0.0f), 1.0f);
        return std::exp2(logWidth + (logHeight - logWidth) * match);
    }

    HX::UiRect CanvasRect(const HM::Vector2& targetSize, float scale)
    {
        return { 0.0f, 0.0f, targetSize.x() / scale, targetSize.y() / scale };
    }

    HX::UiRect ToPixels(const HX::UiRect& rect, float scale)
    {
        return { rect.X * scale, rect.Y * scale, rect.Width * scale, rect.Height * scale };
    }
}
