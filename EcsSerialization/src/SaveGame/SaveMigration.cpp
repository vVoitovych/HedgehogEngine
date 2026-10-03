#include "EcsSerialization/api/SaveGame/SaveMigration.hpp"

#include "Logger/api/Logger.hpp"

#include <exception>
#include <utility>

namespace EcsSerialization
{
    void SaveMigrationRegistry::Register(int fromVersion, SaveMigrationStep step)
    {
        if (fromVersion < 1)
        {
            LOGERROR("[Save] A migration from version " + std::to_string(fromVersion) +
                     " is ignored: versions start at 1.");
            return;
        }
        m_Steps[fromVersion] = std::move(step);
    }

    bool SaveMigrationRegistry::Has(int fromVersion) const { return m_Steps.contains(fromVersion); }

    std::string SaveMigrationRegistry::Migrate(SaveGameFile& save, int from, int to) const
    {
        for (int version = from; version < to; ++version)
        {
            const auto step = m_Steps.find(version);
            if (step == m_Steps.end() || !step->second)
                continue;

            std::string error;
            try
            {
                error = step->second(save);
            }
            catch (const std::exception& e)
            {
                error = e.what();
            }
            if (!error.empty())
                return "version " + std::to_string(version) + " to " + std::to_string(version + 1) + ": " + error;
        }
        return {};
    }
}
