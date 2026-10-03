#pragma once

#include "HedgehogExtract/api/UiDrawList.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstddef>
#include <optional>
#include <span>

// Pointer hit testing and directional focus navigation over a frame's UI elements, as plain data and
// free functions. Rects are in the target's pixels, origin at the top-left, y down.
namespace HUI
{
    // An element that takes the pointer: listed in draw order, so a later one is on top. An element
    // that is not Interactable still blocks the pointer from what lies under it.
    struct UiHitTarget
    {
        HX::UiRect Rect;
        bool       Interactable = true;
    };

    enum class UiNavigateDirection
    {
        Up,
        Down,
        Left,
        Right
    };

    // Whether point lies in rect: its left and top edges included, its right and bottom ones not.
    [[nodiscard]] bool Contains(const HX::UiRect& rect, const HM::Vector2& point);

    // The topmost target under point, interactable or not; nullopt when point is over none.
    [[nodiscard]] std::optional<size_t> FindTopmostTarget(std::span<const UiHitTarget> targets,
                                                          const HM::Vector2&           point);

    // The topmost target under point when it is interactable; nullopt when there is none or the
    // topmost one is not interactable (it blocks those under it).
    [[nodiscard]] std::optional<size_t> FindHoverTarget(std::span<const UiHitTarget> targets, const HM::Vector2& point);

    // The interactable target focus moves to from targets[from] in direction: of those whose centre
    // lies strictly that way from from's centre, the one with the least distance along the direction
    // plus twice the distance across it (so a neighbour in the same row or column beats a nearer
    // diagonal one), the first in draw order on a tie. nullopt when there is none or from is out of range.
    [[nodiscard]] std::optional<size_t> FindNavigationTarget(std::span<const UiHitTarget> targets, size_t from,
                                                             UiNavigateDirection direction);

    // The first interactable target in draw order, where focus starts when nothing has it.
    [[nodiscard]] std::optional<size_t> FindFirstInteractable(std::span<const UiHitTarget> targets);
}
