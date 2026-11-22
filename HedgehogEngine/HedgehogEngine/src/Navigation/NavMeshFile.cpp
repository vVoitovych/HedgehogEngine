#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"

#include <array>
#include <cstring>

namespace HedgehogEngine
{
    namespace
    {
        constexpr std::array<uint8_t, 4> MAGIC = { 'H', 'N', 'A', 'V' };

        // The settings' fields in file order, so writing and reading cannot disagree.
        template <typename Settings, typename Visit>
        void VisitSettings(Settings& s, Visit&& visit)
        {
            visit(s.CellSize);
            visit(s.CellHeight);
            visit(s.AgentRadius);
            visit(s.AgentHeight);
            visit(s.AgentMaxClimb);
            visit(s.AgentMaxSlope);
            visit(s.RegionMinSize);
            visit(s.RegionMergeSize);
            visit(s.EdgeMaxLength);
            visit(s.EdgeMaxError);
            visit(s.VertsPerPoly);
            visit(s.DetailSampleDistance);
            visit(s.DetailSampleMaxError);
        }

        // Every value is 4 bytes, written in the machine's order: the engine runs on little-endian x64 only.
        template <typename T>
        void Put(std::vector<uint8_t>& out, T value)
        {
            static_assert(sizeof(T) == 4);
            const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
            out.insert(out.end(), bytes, bytes + sizeof(T));
        }

        struct Reader
        {
            std::span<const uint8_t> Bytes;
            size_t                   Offset = 0;

            template <typename T>
            bool Get(T& value)
            {
                static_assert(sizeof(T) == 4);
                if (Bytes.size() - Offset < sizeof(T))
                    return false;
                std::memcpy(&value, Bytes.data() + Offset, sizeof(T));
                Offset += sizeof(T);
                return true;
            }
        };

        NavMeshFileReadResult Refuse(std::string error)
        {
            NavMeshFileReadResult result;
            result.Error = std::move(error);
            return result;
        }
    }

    std::vector<uint8_t> WriteNavMeshFile(const HN::NavMeshBuildSettings& settings, std::span<const uint8_t> data)
    {
        std::vector<uint8_t> out(MAGIC.begin(), MAGIC.end());
        Put(out, NAVMESH_FILE_VERSION);
        VisitSettings(settings, [&](auto value) { Put(out, value); });
        Put(out, static_cast<uint32_t>(data.size()));
        out.insert(out.end(), data.begin(), data.end());
        return out;
    }

    NavMeshFileReadResult ReadNavMeshFile(std::span<const uint8_t> bytes)
    {
        if (bytes.size() < MAGIC.size() || !std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin()))
            return Refuse("it is not a navmesh file (no HNAV magic)");

        Reader   reader{ bytes, MAGIC.size() };
        uint32_t version = 0;
        if (!reader.Get(version))
            return Refuse("it ends before its version");
        if (version != NAVMESH_FILE_VERSION)
            return Refuse("it is version " + std::to_string(version) + ", but this build reads version " +
                          std::to_string(NAVMESH_FILE_VERSION));

        NavMeshFileReadResult result;
        bool                  complete = true;
        VisitSettings(result.Settings, [&](auto& value) { complete = complete && reader.Get(value); });
        uint32_t size = 0;
        if (!complete || !reader.Get(size))
            return Refuse("it ends inside its build settings");
        const size_t remaining = bytes.size() - reader.Offset;
        if (remaining < size)
            return Refuse("it ends " + std::to_string(size - remaining) + " bytes before its navmesh data does");
        if (remaining > size)
            return Refuse("it has " + std::to_string(remaining - size) + " bytes past its navmesh data");

        result.Data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(reader.Offset), bytes.end());
        return result;
    }
}
