#include "HedgehogUI/api/UiHitTest.hpp"

#include <cmath>

namespace HUI
{
    namespace
    {
        constexpr float CROSS_DISTANCE_WEIGHT = 2.0f;

        HM::Vector2 Centre(const HX::UiRect& rect)
        {
            return HM::Vector2(rect.X + rect.Width * 0.5f, rect.Y + rect.Height * 0.5f);
        }
    }

    bool Contains(const HX::UiRect& rect, const HM::Vector2& point)
    {
        return point.x() >= rect.X && point.x() < rect.X + rect.Width && point.y() >= rect.Y &&
               point.y() < rect.Y + rect.Height;
    }

    std::optional<size_t> FindTopmostTarget(std::span<const UiHitTarget> targets, const HM::Vector2& point)
    {
        for (size_t i = targets.size(); i > 0; --i)
        {
            if (Contains(targets[i - 1].Rect, point))
                return i - 1;
        }
        return std::nullopt;
    }

    std::optional<size_t> FindHoverTarget(std::span<const UiHitTarget> targets, const HM::Vector2& point)
    {
        const std::optional<size_t> topmost = FindTopmostTarget(targets, point);
        if (topmost && targets[*topmost].Interactable)
            return topmost;
        return std::nullopt;
    }

    std::optional<size_t> FindNavigationTarget(std::span<const UiHitTarget> targets, size_t from,
                                               UiNavigateDirection direction)
    {
        if (from >= targets.size())
            return std::nullopt;

        const HM::Vector2     origin = Centre(targets[from].Rect);
        std::optional<size_t> best;
        float                 bestScore = 0.0f;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            if (i == from || !targets[i].Interactable)
                continue;

            const HM::Vector2 centre = Centre(targets[i].Rect);
            const float       dx     = centre.x() - origin.x();
            const float       dy     = centre.y() - origin.y();
            float             along  = 0.0f;
            float             across = 0.0f;
            switch (direction)
            {
            case UiNavigateDirection::Up:    along = -dy; across = dx; break;
            case UiNavigateDirection::Down:  along = dy;  across = dx; break;
            case UiNavigateDirection::Left:  along = -dx; across = dy; break;
            case UiNavigateDirection::Right: along = dx;  across = dy; break;
            }
            if (!(along > 0.0f))
                continue;

            const float score = along + CROSS_DISTANCE_WEIGHT * std::abs(across);
            if (!best || score < bestScore)
            {
                best      = i;
                bestScore = score;
            }
        }
        return best;
    }

    std::optional<size_t> FindFirstInteractable(std::span<const UiHitTarget> targets)
    {
        for (size_t i = 0; i < targets.size(); ++i)
        {
            if (targets[i].Interactable)
                return i;
        }
        return std::nullopt;
    }
}
