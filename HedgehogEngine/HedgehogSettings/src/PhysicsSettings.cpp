#include "HedgehogSettings/api/PhysicsSettings.hpp"

#include "PhysicsSettingsYaml.hpp"

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <string>

namespace HedgehogSettings
{
    bool PhysicsSettings::Collides(uint32_t a, uint32_t b) const
    {
        if (a >= LAYER_COUNT || b >= LAYER_COUNT)
            return false;
        return (CollisionMasks[a] >> b & 1u) != 0;
    }

    void PhysicsSettings::SetCollides(uint32_t a, uint32_t b, bool collide)
    {
        if (a >= LAYER_COUNT || b >= LAYER_COUNT)
            return;
        const auto set = [collide](uint16_t& row, uint32_t bit) {
            if (collide)
                row = static_cast<uint16_t>(row | 1u << bit);
            else
                row = static_cast<uint16_t>(row & ~(1u << bit));
        };
        set(CollisionMasks[a], b);
        set(CollisionMasks[b], a);
    }

    std::string PhysicsSettings::GetLayerDisplayName(uint32_t layer) const
    {
        if (layer >= LAYER_COUNT)
            return "<invalid layer>";
        return LayerNames[layer].empty() ? "Layer " + std::to_string(layer) : LayerNames[layer];
    }

    namespace
    {
        // A layer index as written in the file, or nothing (with a warning naming where) for one
        // that is not a number of 0 to 15.
        bool ReadLayerIndex(const YAML::Node& node, const char* where, uint32_t& out)
        {
            int64_t layer = -1;
            if (node.IsScalar() && YAML::convert<int64_t>::decode(node, layer) && layer >= 0 &&
                layer < static_cast<int64_t>(PhysicsSettings::LAYER_COUNT))
            {
                out = static_cast<uint32_t>(layer);
                return true;
            }
            const std::string value = node.IsScalar() ? node.Scalar() : std::string("<not a value>");
            LOGWARNING("[Settings] physics." + std::string(where) + ": '" + value + "' is not a layer of 0 to " +
                       std::to_string(PhysicsSettings::LAYER_COUNT - 1) + "; it is ignored.");
            return false;
        }

        void ReadGravity(const YAML::Node& node, PhysicsSettings& settings)
        {
            std::array<float, 3> gravity{};
            bool                 valid = node.IsSequence() && node.size() == 3;
            for (size_t axis = 0; valid && axis < 3; ++axis)
                valid = YAML::convert<float>::decode(node[axis], gravity[axis]) && std::isfinite(gravity[axis]);
            if (valid)
                settings.Gravity = gravity;
            else
                LOGWARNING("[Settings] physics.gravity is not three finite numbers; it keeps its value.");
        }

        void ReadLayers(const YAML::Node& node, PhysicsSettings& settings)
        {
            if (!node.IsMap())
            {
                LOGWARNING("[Settings] physics.layers is not a map of layer to name; it is ignored.");
                return;
            }
            for (const auto& entry : node)
            {
                uint32_t layer = 0;
                if (ReadLayerIndex(entry.first, "layers", layer))
                    settings.LayerNames[layer] = entry.second.IsScalar() ? entry.second.Scalar() : std::string();
            }
        }

        void ReadCollisions(const YAML::Node& node, PhysicsSettings& settings)
        {
            if (!node.IsMap())
            {
                LOGWARNING("[Settings] physics.collisions is not a map of layer to layers; it is ignored.");
                return;
            }
            std::array<uint16_t, PhysicsSettings::LAYER_COUNT> masks = settings.CollisionMasks;
            for (const auto& entry : node)
            {
                uint32_t row = 0;
                if (!ReadLayerIndex(entry.first, "collisions", row))
                    continue;
                if (!entry.second.IsSequence())
                {
                    LOGWARNING("[Settings] physics.collisions." + std::to_string(row) +
                               " is not a list of layers; it is ignored.");
                    continue;
                }
                uint16_t mask = 0;
                for (const YAML::Node& other : entry.second)
                {
                    uint32_t column = 0;
                    if (ReadLayerIndex(other, "collisions", column))
                        mask = static_cast<uint16_t>(mask | 1u << column);
                }
                masks[row] = mask;
            }

            // A pair the two rows disagree on keeps colliding, as it does by default.
            for (uint32_t a = 0; a < PhysicsSettings::LAYER_COUNT; ++a)
                for (uint32_t b = a + 1; b < PhysicsSettings::LAYER_COUNT; ++b)
                {
                    const bool ab = (masks[a] >> b & 1u) != 0;
                    const bool ba = (masks[b] >> a & 1u) != 0;
                    if (ab == ba)
                        continue;
                    LOGWARNING("[Settings] physics.collisions: layers " + std::to_string(a) + " and " +
                               std::to_string(b) + " list each other on one side only; they collide.");
                    masks[a] = static_cast<uint16_t>(masks[a] | 1u << b);
                    masks[b] = static_cast<uint16_t>(masks[b] | 1u << a);
                }
            settings.CollisionMasks = masks;
        }

        void ReadWorkerThreads(const YAML::Node& node, PhysicsSettings& settings)
        {
            int32_t threads = 0;
            if (node.IsScalar() && YAML::convert<int32_t>::decode(node, threads) &&
                threads >= PhysicsSettings::AUTO_WORKER_THREADS)
                settings.WorkerThreads = threads;
            else
                LOGWARNING("[Settings] physics.worker_threads is not a number of -1 or more; it keeps its value.");
        }
    }

    void ReadPhysicsSettings(const YAML::Node& section, PhysicsSettings& settings)
    {
        if (!section.IsMap())
        {
            LOGWARNING("[Settings] physics is not a map; the physics settings keep their values.");
            return;
        }
        if (const YAML::Node node = section["gravity"])
            ReadGravity(node, settings);
        if (const YAML::Node node = section["layers"])
            ReadLayers(node, settings);
        if (const YAML::Node node = section["collisions"])
            ReadCollisions(node, settings);
        if (const YAML::Node node = section["worker_threads"])
            ReadWorkerThreads(node, settings);
    }

    void WritePhysicsSettings(YAML::Emitter& out, const PhysicsSettings& settings)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "gravity" << YAML::Value << YAML::Flow << YAML::BeginSeq;
        for (const float axis : settings.Gravity)
            out << axis;
        out << YAML::EndSeq;

        // Only named layers, as the render layers are written.
        out << YAML::Key << "layers" << YAML::Value << YAML::BeginMap;
        for (uint32_t layer = 0; layer < PhysicsSettings::LAYER_COUNT; ++layer)
            if (!settings.LayerNames[layer].empty())
                out << YAML::Key << layer << YAML::Value << settings.LayerNames[layer];
        out << YAML::EndMap;

        out << YAML::Key << "collisions" << YAML::Value << YAML::BeginMap;
        for (uint32_t row = 0; row < PhysicsSettings::LAYER_COUNT; ++row)
        {
            out << YAML::Key << row << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (uint32_t column = 0; column < PhysicsSettings::LAYER_COUNT; ++column)
                if (settings.Collides(row, column))
                    out << column;
            out << YAML::EndSeq;
        }
        out << YAML::EndMap;

        out << YAML::Key << "worker_threads" << YAML::Value << settings.WorkerThreads;
        out << YAML::EndMap;
    }
}
