#pragma once

#include <string>

namespace HedgehogEngine
{
    /// Emitted by SaveGameManager once a save has been loaded: the world restored and every
    /// registered section handed its data.
    struct GameLoadedEvent
    {
        std::string Slot;
    };
}
