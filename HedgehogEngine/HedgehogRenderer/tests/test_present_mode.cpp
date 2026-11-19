#include "RHI/api/PresentMode.hpp"

#include "doctest/doctest/doctest.h"

#include <array>

namespace
{

using RHI::PresentMode;
using RHI::SurfacePresentMode;

constexpr std::array ALL     = { SurfacePresentMode::Fifo, SurfacePresentMode::Mailbox, SurfacePresentMode::Immediate };
constexpr std::array REORDER = { SurfacePresentMode::Immediate, SurfacePresentMode::Mailbox, SurfacePresentMode::Fifo };
constexpr std::array NO_MAIL = { SurfacePresentMode::Fifo, SurfacePresentMode::Immediate };
constexpr std::array FIFO    = { SurfacePresentMode::Fifo };

static_assert(RHI::ChoosePresentMode(ALL, PresentMode::NoVSync) == SurfacePresentMode::Mailbox);

} // namespace

TEST_CASE("VSync always chooses Fifo")
{
    CHECK(RHI::ChoosePresentMode(ALL, PresentMode::VSync) == SurfacePresentMode::Fifo);
    CHECK(RHI::ChoosePresentMode(FIFO, PresentMode::VSync) == SurfacePresentMode::Fifo);
    CHECK(RHI::ChoosePresentMode({}, PresentMode::VSync) == SurfacePresentMode::Fifo);
}

TEST_CASE("NoVSync prefers Mailbox, then Immediate, then Fifo")
{
    CHECK(RHI::ChoosePresentMode(ALL, PresentMode::NoVSync) == SurfacePresentMode::Mailbox);
    CHECK(RHI::ChoosePresentMode(REORDER, PresentMode::NoVSync) == SurfacePresentMode::Mailbox);
    CHECK(RHI::ChoosePresentMode(NO_MAIL, PresentMode::NoVSync) == SurfacePresentMode::Immediate);
    CHECK(RHI::ChoosePresentMode(FIFO, PresentMode::NoVSync) == SurfacePresentMode::Fifo);
    CHECK(RHI::ChoosePresentMode({}, PresentMode::NoVSync) == SurfacePresentMode::Fifo);
}
