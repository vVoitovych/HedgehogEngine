#include "doctest/doctest/doctest.h"

#include "HedgehogUI/api/UiHitTest.hpp"

#include <vector>

using namespace HUI;

namespace
{
    UiHitTarget Target(float x, float y, float width, float height, bool interactable = true)
    {
        return { HX::UiRect{ x, y, width, height }, interactable };
    }

    // Buttons 100x50 on a grid with 20-pixel gaps, row by row: index = row * columns + column.
    std::vector<UiHitTarget> Grid(int columns, int rows)
    {
        std::vector<UiHitTarget> targets;
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < columns; ++column)
                targets.push_back(Target(static_cast<float>(column) * 120.0f, static_cast<float>(row) * 70.0f, 100.0f, 50.0f));
        return targets;
    }
}

TEST_CASE("Hit test - a rect contains its top-left edges but not its bottom-right ones")
{
    const HX::UiRect rect{ 10.0f, 20.0f, 30.0f, 40.0f };
    CHECK(Contains(rect, HM::Vector2(10.0f, 20.0f)));
    CHECK(Contains(rect, HM::Vector2(39.9f, 59.9f)));
    CHECK_FALSE(Contains(rect, HM::Vector2(40.0f, 30.0f)));
    CHECK_FALSE(Contains(rect, HM::Vector2(20.0f, 60.0f)));
    CHECK_FALSE(Contains(rect, HM::Vector2(9.9f, 30.0f)));
    CHECK_FALSE(Contains(HX::UiRect{ 10.0f, 10.0f, 0.0f, 0.0f }, HM::Vector2(10.0f, 10.0f)));
}

TEST_CASE("Hit test - the target drawn last is on top")
{
    const std::vector<UiHitTarget> targets = { Target(0, 0, 200, 200), Target(50, 50, 100, 100), Target(300, 0, 50, 50) };

    CHECK(FindTopmostTarget(targets, HM::Vector2(75.0f, 75.0f)) == 1u);
    CHECK(FindTopmostTarget(targets, HM::Vector2(10.0f, 10.0f)) == 0u);
    CHECK(FindTopmostTarget(targets, HM::Vector2(310.0f, 10.0f)) == 2u);
    CHECK_FALSE(FindTopmostTarget(targets, HM::Vector2(250.0f, 250.0f)).has_value());
    CHECK(FindHoverTarget(targets, HM::Vector2(75.0f, 75.0f)) == 1u);
    CHECK_FALSE(FindTopmostTarget({}, HM::Vector2(0.0f, 0.0f)).has_value());
}

TEST_CASE("Hit test - a target that is not interactable blocks those under it")
{
    // A disabled button over an enabled one, and a disabled one beside it.
    const std::vector<UiHitTarget> targets = { Target(0, 0, 100, 100), Target(25, 25, 50, 50, false),
                                               Target(200, 0, 100, 100, false) };

    CHECK(FindTopmostTarget(targets, HM::Vector2(50.0f, 50.0f)) == 1u);
    CHECK_FALSE(FindHoverTarget(targets, HM::Vector2(50.0f, 50.0f)).has_value()); // blocked, not passed through
    CHECK(FindHoverTarget(targets, HM::Vector2(10.0f, 10.0f)) == 0u);
    CHECK_FALSE(FindHoverTarget(targets, HM::Vector2(250.0f, 50.0f)).has_value());
}

TEST_CASE("Navigation - a grid moves to the neighbour in each direction")
{
    // 3x3; the centre is 4.
    const std::vector<UiHitTarget> grid = Grid(3, 3);

    CHECK(FindNavigationTarget(grid, 4, UiNavigateDirection::Right) == 5u);
    CHECK(FindNavigationTarget(grid, 4, UiNavigateDirection::Left) == 3u);
    CHECK(FindNavigationTarget(grid, 4, UiNavigateDirection::Up) == 1u);
    CHECK(FindNavigationTarget(grid, 4, UiNavigateDirection::Down) == 7u);

    // From a corner: along its row and column, nothing past the edges.
    CHECK(FindNavigationTarget(grid, 0, UiNavigateDirection::Right) == 1u);
    CHECK(FindNavigationTarget(grid, 0, UiNavigateDirection::Down) == 3u);
    CHECK_FALSE(FindNavigationTarget(grid, 0, UiNavigateDirection::Left).has_value());
    CHECK_FALSE(FindNavigationTarget(grid, 0, UiNavigateDirection::Up).has_value());
    CHECK_FALSE(FindNavigationTarget(grid, 8, UiNavigateDirection::Right).has_value());
}

TEST_CASE("Navigation - a column moves up and down only")
{
    const std::vector<UiHitTarget> column = Grid(1, 4);

    CHECK(FindNavigationTarget(column, 1, UiNavigateDirection::Down) == 2u);
    CHECK(FindNavigationTarget(column, 1, UiNavigateDirection::Up) == 0u);
    CHECK(FindNavigationTarget(column, 3, UiNavigateDirection::Up) == 2u);
    CHECK_FALSE(FindNavigationTarget(column, 3, UiNavigateDirection::Down).has_value());
    CHECK_FALSE(FindNavigationTarget(column, 1, UiNavigateDirection::Left).has_value());
    CHECK_FALSE(FindNavigationTarget(column, 1, UiNavigateDirection::Right).has_value());
}

TEST_CASE("Navigation - a neighbour in the same row beats a nearer diagonal one")
{
    // From 0: 1 is 150 to the right in the same row; 2 is 100 right and 60 down.
    const std::vector<UiHitTarget> targets = { Target(0, 0, 50, 50), Target(150, 0, 50, 50), Target(100, 60, 50, 50) };
    CHECK(FindNavigationTarget(targets, 0, UiNavigateDirection::Right) == 1u);
}

TEST_CASE("Navigation - targets that are not interactable are skipped")
{
    std::vector<UiHitTarget> grid = Grid(3, 1);
    grid[1].Interactable = false;

    CHECK(FindNavigationTarget(grid, 0, UiNavigateDirection::Right) == 2u);
    CHECK(FindFirstInteractable(grid) == 0u);
    grid[0].Interactable = false;
    CHECK(FindFirstInteractable(grid) == 2u);
    grid[2].Interactable = false;
    CHECK_FALSE(FindFirstInteractable(grid).has_value());
    CHECK_FALSE(FindNavigationTarget(grid, 0, UiNavigateDirection::Right).has_value());
    CHECK_FALSE(FindNavigationTarget(grid, 7, UiNavigateDirection::Right).has_value()); // out of range
}
