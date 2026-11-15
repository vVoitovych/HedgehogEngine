#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace HedgehogEngine
{
    namespace
    {
        // A parse failure, carrying the message up to ParseParticleEffect.
        struct ParseError
        {
            std::string Message;
        };

        template<typename Enum>
        using EnumNames = std::initializer_list<std::pair<Enum, const char*>>;

        const EnumNames<ParticleShape> SHAPES = { { ParticleShape::Point, "Point" }, { ParticleShape::Sphere, "Sphere" },
                                                  { ParticleShape::Cone, "Cone" }, { ParticleShape::Box, "Box" } };
        const EnumNames<ParticleBlend> BLENDS = { { ParticleBlend::Alpha, "Alpha" }, { ParticleBlend::Additive, "Additive" } };
        const EnumNames<ParticleSpace> SPACES = { { ParticleSpace::Local, "Local" }, { ParticleSpace::World, "World" } };
        const EnumNames<ParticleCollisionMode> COLLISION_MODES = { { ParticleCollisionMode::None, "None" },
                                                                   { ParticleCollisionMode::HeightPlane, "HeightPlane" },
                                                                   { ParticleCollisionMode::DepthBuffer, "DepthBuffer" } };

        template<typename Enum>
        const char* NameOf(EnumNames<Enum> names, Enum value)
        {
            for (const auto& [entry, name] : names)
                if (entry == value)
                    return name;
            return "";
        }

        // ── Reading ──────────────────────────────────────────────────────────────

        std::string Scalar(const YAML::Node& node, const std::string& where)
        {
            if (!node.IsScalar())
                throw ParseError{ where + ": expected a single value" };
            return node.Scalar();
        }

        float Number(const YAML::Node& node, const std::string& where)
        {
            const std::string text = Scalar(node, where);
            try
            {
                const float value = node.as<float>();
                if (!std::isfinite(value))
                    throw ParseError{ where + ": '" + text + "' is not a finite number" };
                return value;
            }
            catch (const YAML::Exception&)
            {
                throw ParseError{ where + ": '" + text + "' is not a number" };
            }
        }

        uint32_t Count(const YAML::Node& node, const std::string& where)
        {
            const float value = Number(node, where);
            if (value < 0.0f || value != std::floor(value) || value > static_cast<float>(std::numeric_limits<uint32_t>::max()))
                throw ParseError{ where + ": '" + node.Scalar() + "' is not a whole number of at least 0" };
            return static_cast<uint32_t>(value);
        }

        bool Bool(const YAML::Node& node, const std::string& where)
        {
            const std::string text = Scalar(node, where);
            try
            {
                return node.as<bool>();
            }
            catch (const YAML::Exception&)
            {
                throw ParseError{ where + ": '" + text + "' is not true or false" };
            }
        }

        template<typename Enum>
        Enum EnumValue(const YAML::Node& node, EnumNames<Enum> names, const std::string& where)
        {
            const std::string text = Scalar(node, where);
            std::string       known;
            for (const auto& [value, name] : names)
            {
                if (text == name)
                    return value;
                known += (known.empty() ? "" : ", ") + std::string(name);
            }
            throw ParseError{ where + ": unknown value '" + text + "' (" + known + ")" };
        }

        // size numbers, as a list.
        std::vector<float> Numbers(const YAML::Node& node, size_t size, const std::string& where)
        {
            if (!node.IsSequence() || node.size() != size)
                throw ParseError{ where + ": expected a list of " + std::to_string(size) + " numbers" };
            std::vector<float> values;
            for (size_t i = 0; i < size; ++i)
                values.push_back(Number(node[i], where));
            return values;
        }

        HM::Vector3 Vector3(const YAML::Node& node, const std::string& where)
        {
            const std::vector<float> v = Numbers(node, 3, where);
            return { v[0], v[1], v[2] };
        }

        HM::Vector4 Vector4(const YAML::Node& node, const std::string& where)
        {
            const std::vector<float> v = Numbers(node, 4, where);
            return { v[0], v[1], v[2], v[3] };
        }

        FloatRange Range(const YAML::Node& node, const std::string& where)
        {
            const std::vector<float> v = Numbers(node, 2, where);
            if (v[0] > v[1])
                throw ParseError{ where + ": Min is above Max" };
            return { v[0], v[1] };
        }

        // The list at node (any length), each entry read by read.
        template<typename Read>
        void ForEach(const YAML::Node& node, const std::string& where, Read read)
        {
            if (!node.IsSequence())
                throw ParseError{ where + ": expected a list" };
            for (size_t i = 0; i < node.size(); ++i)
                read(node[i], where + "[" + std::to_string(i) + "]");
        }

        ParticleCurve Curve(const YAML::Node& node, const std::string& where)
        {
            ParticleCurve curve;
            ForEach(node, where, [&](const YAML::Node& key, const std::string& at)
            {
                const std::vector<float> v = Numbers(key, 2, at);
                curve.Keys.push_back({ v[0], v[1] });
            });
            return curve;
        }

        ParticleGradient Gradient(const YAML::Node& node, const std::string& where)
        {
            ParticleGradient gradient;
            ForEach(node, where, [&](const YAML::Node& key, const std::string& at)
            {
                const std::vector<float> v = Numbers(key, 5, at);
                gradient.Keys.push_back({ v[0], { v[1], v[2], v[3], v[4] } });
            });
            return gradient;
        }

        // Every key of node is one of keys.
        void CheckKeys(const YAML::Node& node, std::initializer_list<const char*> keys, const std::string& where)
        {
            if (!node.IsMap())
                throw ParseError{ where + ": expected a map" };
            for (const auto& entry : node)
            {
                const std::string key = entry.first.Scalar();
                if (std::none_of(keys.begin(), keys.end(), [&](const char* known) { return key == known; }))
                    throw ParseError{ where + ": unknown key '" + key + "'" };
            }
        }

        ParticleBurst ReadBurst(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node, { "Time", "Count", "Cycles", "Interval" }, where);
            ParticleBurst burst;
            if (const YAML::Node n = node["Time"])     burst.Time     = Number(n, where + ".Time");
            if (const YAML::Node n = node["Count"])    burst.Count    = Count(n, where + ".Count");
            if (const YAML::Node n = node["Cycles"])   burst.Cycles   = Count(n, where + ".Cycles");
            if (const YAML::Node n = node["Interval"]) burst.Interval = Number(n, where + ".Interval");
            return burst;
        }

        ParticleCollision ReadCollision(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node, { "Mode", "PlaneHeight", "Bounce", "LifetimeLoss" }, where);
            ParticleCollision collision;
            if (const YAML::Node n = node["Mode"])         collision.Mode         = EnumValue(n, COLLISION_MODES, where + ".Mode");
            if (const YAML::Node n = node["PlaneHeight"])  collision.PlaneHeight  = Number(n, where + ".PlaneHeight");
            if (const YAML::Node n = node["Bounce"])       collision.Bounce       = Number(n, where + ".Bounce");
            if (const YAML::Node n = node["LifetimeLoss"]) collision.LifetimeLoss = Number(n, where + ".LifetimeLoss");
            return collision;
        }

        ParticleSubEmitter ReadSubEmitter(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node, { "Emitter", "Count" }, where);
            if (!node["Emitter"])
                throw ParseError{ where + ": Emitter is missing" };
            ParticleSubEmitter sub;
            sub.Emitter = Count(node["Emitter"], where + ".Emitter");
            if (const YAML::Node n = node["Count"])
                sub.Count = Count(n, where + ".Count");
            return sub;
        }

        ParticleEmitter ReadEmitter(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node,
                      { "Name", "MaxParticles", "Rate", "Bursts", "Duration", "Loop", "Shape", "Radius", "Angle", "BoxSize",
                        "EmitFromSurface", "Lifetime", "StartSpeed", "StartSize", "StartRotation", "StartColor", "Gravity",
                        "Drag", "SizeOverLife", "SpeedOverLife", "ColorOverLife", "Texture", "SheetTiles", "SheetFps",
                        "Blend", "SimulationSpace", "SoftDistance", "Collision", "SubEmitters" },
                      where);
            if (!node["Name"])
                throw ParseError{ where + ": Name is missing" };

            ParticleEmitter emitter;
            emitter.Name = Scalar(node["Name"], where + ".Name");
            const std::string at = "Emitter '" + emitter.Name + "'";
            const auto field = [&](const char* key) { return at + "." + key; };

            if (const YAML::Node n = node["MaxParticles"])    emitter.MaxParticles    = Count(n, field("MaxParticles"));
            if (const YAML::Node n = node["Rate"])            emitter.Rate            = Number(n, field("Rate"));
            if (const YAML::Node n = node["Bursts"])
                ForEach(n, field("Bursts"), [&](const YAML::Node& b, const std::string& w) { emitter.Bursts.push_back(ReadBurst(b, w)); });
            if (const YAML::Node n = node["Duration"])        emitter.Duration        = Number(n, field("Duration"));
            if (const YAML::Node n = node["Loop"])            emitter.Loop            = Bool(n, field("Loop"));
            if (const YAML::Node n = node["Shape"])           emitter.Shape           = EnumValue(n, SHAPES, field("Shape"));
            if (const YAML::Node n = node["Radius"])          emitter.Radius          = Number(n, field("Radius"));
            if (const YAML::Node n = node["Angle"])           emitter.Angle           = Number(n, field("Angle"));
            if (const YAML::Node n = node["BoxSize"])         emitter.BoxSize         = Vector3(n, field("BoxSize"));
            if (const YAML::Node n = node["EmitFromSurface"]) emitter.EmitFromSurface = Bool(n, field("EmitFromSurface"));
            if (const YAML::Node n = node["Lifetime"])        emitter.Lifetime        = Range(n, field("Lifetime"));
            if (const YAML::Node n = node["StartSpeed"])      emitter.StartSpeed      = Range(n, field("StartSpeed"));
            if (const YAML::Node n = node["StartSize"])       emitter.StartSize       = Range(n, field("StartSize"));
            if (const YAML::Node n = node["StartRotation"])   emitter.StartRotation   = Range(n, field("StartRotation"));
            if (const YAML::Node n = node["StartColor"])      emitter.StartColor      = Vector4(n, field("StartColor"));
            if (const YAML::Node n = node["Gravity"])         emitter.Gravity         = Vector3(n, field("Gravity"));
            if (const YAML::Node n = node["Drag"])            emitter.Drag            = Number(n, field("Drag"));
            if (const YAML::Node n = node["SizeOverLife"])    emitter.SizeOverLife    = Curve(n, field("SizeOverLife"));
            if (const YAML::Node n = node["SpeedOverLife"])   emitter.SpeedOverLife   = Curve(n, field("SpeedOverLife"));
            if (const YAML::Node n = node["ColorOverLife"])   emitter.ColorOverLife   = Gradient(n, field("ColorOverLife"));
            if (const YAML::Node n = node["Texture"])         emitter.Texture         = Scalar(n, field("Texture"));
            if (const YAML::Node n = node["SheetTiles"])
            {
                if (!n.IsSequence() || n.size() != 2)
                    throw ParseError{ field("SheetTiles") + ": expected a list of 2 numbers" };
                emitter.SheetTilesX = Count(n[0], field("SheetTiles"));
                emitter.SheetTilesY = Count(n[1], field("SheetTiles"));
            }
            if (const YAML::Node n = node["SheetFps"])        emitter.SheetFps        = Number(n, field("SheetFps"));
            if (const YAML::Node n = node["Blend"])           emitter.Blend           = EnumValue(n, BLENDS, field("Blend"));
            if (const YAML::Node n = node["SimulationSpace"]) emitter.SimulationSpace = EnumValue(n, SPACES, field("SimulationSpace"));
            if (const YAML::Node n = node["SoftDistance"])    emitter.SoftDistance    = Number(n, field("SoftDistance"));
            if (const YAML::Node n = node["Collision"])       emitter.Collision       = ReadCollision(n, field("Collision"));
            if (const YAML::Node n = node["SubEmitters"])
                ForEach(n, field("SubEmitters"),
                        [&](const YAML::Node& s, const std::string& w) { emitter.SubEmitters.push_back(ReadSubEmitter(s, w)); });
            return emitter;
        }

        // ── Writing ──────────────────────────────────────────────────────────────

        void WriteNumbers(YAML::Emitter& out, std::initializer_list<float> values)
        {
            out << YAML::Flow << YAML::BeginSeq;
            for (const float value : values)
                out << value;
            out << YAML::EndSeq;
        }

        void WriteCurve(YAML::Emitter& out, const char* key, const ParticleCurve& curve)
        {
            if (curve.Keys.empty())
                return;
            out << YAML::Key << key << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (const ParticleCurveKey& k : curve.Keys)
                WriteNumbers(out, { k.Time, k.Value });
            out << YAML::EndSeq;
        }

        void WriteEmitter(YAML::Emitter& out, const ParticleEmitter& e)
        {
            const ParticleEmitter d; // the defaults, which are left out
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << e.Name;
            const auto number = [&](const char* key, float value, float fallback)
            {
                if (value != fallback)
                    out << YAML::Key << key << YAML::Value << value;
            };
            const auto range = [&](const char* key, const FloatRange& value, const FloatRange& fallback)
            {
                if (value != fallback)
                {
                    out << YAML::Key << key << YAML::Value;
                    WriteNumbers(out, { value.Min, value.Max });
                }
            };

            if (e.MaxParticles != d.MaxParticles)
                out << YAML::Key << "MaxParticles" << YAML::Value << e.MaxParticles;
            number("Rate", e.Rate, d.Rate);
            if (!e.Bursts.empty())
            {
                out << YAML::Key << "Bursts" << YAML::Value << YAML::BeginSeq;
                for (const ParticleBurst& burst : e.Bursts)
                {
                    const ParticleBurst db;
                    out << YAML::Flow << YAML::BeginMap;
                    out << YAML::Key << "Time" << YAML::Value << burst.Time;
                    out << YAML::Key << "Count" << YAML::Value << burst.Count;
                    if (burst.Cycles != db.Cycles)
                        out << YAML::Key << "Cycles" << YAML::Value << burst.Cycles;
                    if (burst.Interval != db.Interval)
                        out << YAML::Key << "Interval" << YAML::Value << burst.Interval;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
            }
            number("Duration", e.Duration, d.Duration);
            if (e.Loop != d.Loop)
                out << YAML::Key << "Loop" << YAML::Value << e.Loop;
            if (e.Shape != d.Shape)
                out << YAML::Key << "Shape" << YAML::Value << NameOf(SHAPES, e.Shape);
            number("Radius", e.Radius, d.Radius);
            number("Angle", e.Angle, d.Angle);
            if (e.BoxSize != d.BoxSize)
            {
                out << YAML::Key << "BoxSize" << YAML::Value;
                WriteNumbers(out, { e.BoxSize.x(), e.BoxSize.y(), e.BoxSize.z() });
            }
            if (e.EmitFromSurface != d.EmitFromSurface)
                out << YAML::Key << "EmitFromSurface" << YAML::Value << e.EmitFromSurface;
            range("Lifetime", e.Lifetime, d.Lifetime);
            range("StartSpeed", e.StartSpeed, d.StartSpeed);
            range("StartSize", e.StartSize, d.StartSize);
            range("StartRotation", e.StartRotation, d.StartRotation);
            if (e.StartColor != d.StartColor)
            {
                out << YAML::Key << "StartColor" << YAML::Value;
                WriteNumbers(out, { e.StartColor.x(), e.StartColor.y(), e.StartColor.z(), e.StartColor.w() });
            }
            if (e.Gravity != d.Gravity)
            {
                out << YAML::Key << "Gravity" << YAML::Value;
                WriteNumbers(out, { e.Gravity.x(), e.Gravity.y(), e.Gravity.z() });
            }
            number("Drag", e.Drag, d.Drag);
            WriteCurve(out, "SizeOverLife", e.SizeOverLife);
            WriteCurve(out, "SpeedOverLife", e.SpeedOverLife);
            if (!e.ColorOverLife.Keys.empty())
            {
                out << YAML::Key << "ColorOverLife" << YAML::Value << YAML::Flow << YAML::BeginSeq;
                for (const ParticleGradientKey& k : e.ColorOverLife.Keys)
                    WriteNumbers(out, { k.Time, k.Color.x(), k.Color.y(), k.Color.z(), k.Color.w() });
                out << YAML::EndSeq;
            }
            if (e.Texture != d.Texture)
                out << YAML::Key << "Texture" << YAML::Value << e.Texture;
            if (e.SheetTilesX != d.SheetTilesX || e.SheetTilesY != d.SheetTilesY)
                out << YAML::Key << "SheetTiles" << YAML::Value << YAML::Flow << YAML::BeginSeq << e.SheetTilesX
                    << e.SheetTilesY << YAML::EndSeq;
            number("SheetFps", e.SheetFps, d.SheetFps);
            if (e.Blend != d.Blend)
                out << YAML::Key << "Blend" << YAML::Value << NameOf(BLENDS, e.Blend);
            if (e.SimulationSpace != d.SimulationSpace)
                out << YAML::Key << "SimulationSpace" << YAML::Value << NameOf(SPACES, e.SimulationSpace);
            number("SoftDistance", e.SoftDistance, d.SoftDistance);
            if (e.Collision != d.Collision)
            {
                const ParticleCollision& c  = e.Collision;
                const ParticleCollision& dc = d.Collision;
                out << YAML::Key << "Collision" << YAML::Value << YAML::Flow << YAML::BeginMap;
                out << YAML::Key << "Mode" << YAML::Value << NameOf(COLLISION_MODES, c.Mode);
                if (c.PlaneHeight != dc.PlaneHeight)
                    out << YAML::Key << "PlaneHeight" << YAML::Value << c.PlaneHeight;
                if (c.Bounce != dc.Bounce)
                    out << YAML::Key << "Bounce" << YAML::Value << c.Bounce;
                if (c.LifetimeLoss != dc.LifetimeLoss)
                    out << YAML::Key << "LifetimeLoss" << YAML::Value << c.LifetimeLoss;
                out << YAML::EndMap;
            }
            if (!e.SubEmitters.empty())
            {
                out << YAML::Key << "SubEmitters" << YAML::Value << YAML::BeginSeq;
                for (const ParticleSubEmitter& sub : e.SubEmitters)
                {
                    out << YAML::Flow << YAML::BeginMap;
                    out << YAML::Key << "Emitter" << YAML::Value << sub.Emitter;
                    if (sub.Count != ParticleSubEmitter{}.Count)
                        out << YAML::Key << "Count" << YAML::Value << sub.Count;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
            }
            out << YAML::EndMap;
        }
    }

    ParticleEffectReadResult ParseParticleEffect(std::string_view text)
    {
        ParticleEffectReadResult result;
        try
        {
            const YAML::Node root = YAML::Load(std::string(text));
            if (!root.IsMap())
                throw ParseError{ "the file is not a map" };
            CheckKeys(root, { "Version", "Emitters" }, "the file");
            if (!root["Version"])
                throw ParseError{ "Version is missing" };
            if (Scalar(root["Version"], "Version") != std::to_string(PARTICLE_EFFECT_FILE_VERSION))
                throw ParseError{ "Version: this build reads version " + std::to_string(PARTICLE_EFFECT_FILE_VERSION)
                                  + ", not " + root["Version"].Scalar() };
            if (!root["Emitters"])
                throw ParseError{ "Emitters is missing" };

            ParticleEffect effect;
            ForEach(root["Emitters"], "Emitters",
                    [&](const YAML::Node& node, const std::string& where) { effect.Emitters.push_back(ReadEmitter(node, where)); });

            const std::vector<std::string> problems = ValidateParticleEffect(effect);
            if (!problems.empty())
                throw ParseError{ problems.front() };
            result.Effect = std::move(effect);
        }
        catch (const ParseError& error)
        {
            result.Error = error.Message;
        }
        catch (const YAML::Exception& error)
        {
            result.Error = std::string("the file is not valid YAML: ") + error.what();
        }
        return result;
    }

    std::string WriteParticleEffect(const ParticleEffect& effect)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << PARTICLE_EFFECT_FILE_VERSION;
        out << YAML::Key << "Emitters" << YAML::Value << YAML::BeginSeq;
        for (const ParticleEmitter& emitter : effect.Emitters)
            WriteEmitter(out, emitter);
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return std::string(out.c_str()) + "\n";
    }
}
