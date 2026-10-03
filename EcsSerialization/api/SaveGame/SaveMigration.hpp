#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"
#include "EcsSerialization/api/SaveGame/SaveGameFile.hpp"

#include <functional>
#include <map>
#include <string>

namespace EcsSerialization
{
    // One version step of a save's data: rewrites, in place, the sections of a save made at
    // version `from` into what version `from + 1` reads. Returns an empty string on success, else
    // why it failed.
    using SaveMigrationStep = std::function<std::string(SaveGameFile& save)>;

    // Save migrations by version step, applied in order. A version without a step needs none:
    // its data reads as the next version's (a game bumps its data version for a script state
    // change its scripts' OnMigrate handles, with no C++ step).
    class SaveMigrationRegistry
    {
    public:
        // The step from fromVersion (at least 1) to fromVersion + 1; registering one again
        // replaces it. A version below 1 is logged and ignored.
        ECS_SERIALIZATION_API void Register(int fromVersion, SaveMigrationStep step);

        [[nodiscard]] ECS_SERIALIZATION_API bool Has(int fromVersion) const;

        // Applies the steps from `from` to `to` in order on save, which the caller should own
        // and discard on failure. Empty on success, else "version <n> to <n + 1>: <why>" for the
        // first step that fails (an exception thrown by a step is a failure too); no later step
        // runs. Nothing to do when from >= to.
        [[nodiscard]] ECS_SERIALIZATION_API std::string Migrate(SaveGameFile& save, int from, int to) const;

    private:
        std::map<int, SaveMigrationStep> m_Steps;
    };
}
