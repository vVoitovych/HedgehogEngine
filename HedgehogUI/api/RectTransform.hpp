#pragma once

#include "HedgehogExtract/api/UiDrawList.hpp"

#include "HedgehogMath/api/Vector.hpp"

// Rect layout for game UI, as plain data and free functions. Rects are in canvas units, origin at
// the top-left, y down; a canvas scale maps them to the target's pixels.
namespace HUI
{
    // Where a UI element sits inside its parent's rect.
    //
    // AnchorMin and AnchorMax are fractions of the parent rect ((0, 0) its top-left, (1, 1) its
    // bottom-right). Equal anchors pin the element to one point; on an axis where they differ, it
    // stretches with the parent. The element's size is the anchors' span plus Size, so with pinned
    // anchors Size is the size itself, and with stretched ones it is how much bigger (or, negative,
    // smaller) than the span it is. Pivot is the point of the element (in fractions of its own rect)
    // placed at the anchors' reference point (the anchors' span interpolated by Pivot) plus Offset.
    struct RectTransform
    {
        HM::Vector2 AnchorMin = HM::Vector2(0.5f, 0.5f);
        HM::Vector2 AnchorMax = HM::Vector2(0.5f, 0.5f);
        HM::Vector2 Pivot     = HM::Vector2(0.5f, 0.5f);
        HM::Vector2 Offset    = HM::Vector2(0.0f, 0.0f);
        HM::Vector2 Size      = HM::Vector2(100.0f, 100.0f);
    };

    // The element's rect inside parent. Resolve a tree top-down: each child against its parent's result.
    [[nodiscard]] HX::UiRect ResolveRect(const RectTransform& transform, const HX::UiRect& parent);

    enum class CanvasScaleMode
    {
        ConstantPixelSize,   // one canvas unit is one pixel at any target size
        ScaleWithTargetSize, // the canvas is laid out at ReferenceResolution and scaled to the target
    };

    struct CanvasScaler
    {
        CanvasScaleMode Mode                = CanvasScaleMode::ConstantPixelSize;
        HM::Vector2     ReferenceResolution = HM::Vector2(1920.0f, 1080.0f);
        // ScaleWithTargetSize only: 0 scales by the width ratio, 1 by the height ratio, values in
        // between blend the two logarithmically (0.5 keeps a 2x wider and 2x shorter target at 1).
        float           MatchWidthOrHeight  = 0.0f;
    };

    // Pixels per canvas unit for a target of targetSize pixels. A bad reference resolution or
    // target size gives 1.
    [[nodiscard]] float ComputeCanvasScale(const CanvasScaler& scaler, const HM::Vector2& targetSize);

    // The root rect children resolve against: the whole target, in canvas units.
    [[nodiscard]] HX::UiRect CanvasRect(const HM::Vector2& targetSize, float scale);

    // A rect in canvas units, in the target's pixels.
    [[nodiscard]] HX::UiRect ToPixels(const HX::UiRect& rect, float scale);
}
