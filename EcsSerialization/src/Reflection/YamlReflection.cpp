#include "api/Reflection/YamlReflection.hpp"

#include <cstring>
#include <string>

namespace
{
    YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector2& v)
    {
        out << YAML::Flow << YAML::BeginSeq << v.x() << v.y() << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
    {
        out << YAML::Flow << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector4& v)
    {
        out << YAML::Flow << YAML::BeginSeq << v.x() << v.y() << v.z() << v.w() << YAML::EndSeq;
        return out;
    }
}

namespace Reflection
{
    void YamlSerializeComponent(YAML::Emitter& out, void* comp, std::span<const PropertyDescriptor> props)
    {
        for (const auto& prop : props)
        {
            if (prop.type == TypeTag::Raw)                       continue;
            if (HasFlag(prop.flags, PropertyFlags::Hidden))      continue;

            switch (prop.type)
            {
            case TypeTag::UInt:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<uint32_t>(comp, prop);
                break;
            case TypeTag::Int:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<int32_t>(comp, prop);
                break;
            case TypeTag::Float:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<float>(comp, prop);
                break;
            case TypeTag::Double:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<double>(comp, prop);
                break;
            case TypeTag::Bool:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<bool>(comp, prop);
                break;
            case TypeTag::String:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<std::string>(comp, prop);
                break;
            case TypeTag::Vec2:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<HM::Vector2>(comp, prop);
                break;
            case TypeTag::Vec3:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<HM::Vector3>(comp, prop);
                break;
            case TypeTag::Vec4:
                out << YAML::Key << prop.name << YAML::Value << *FieldPtr<HM::Vector4>(comp, prop);
                break;
            case TypeTag::Enum:
                {
                    int32_t val = 0;
                    std::memcpy(&val, prop.accessor(comp), sizeof(int32_t));
                    out << YAML::Key << prop.name << YAML::Value << val;
                }
                break;
            default:
                break;
            }
        }
    }

    void YamlDeserializeComponent(void* comp, const YAML::Node& node, std::span<const PropertyDescriptor> props)
    {
        for (const auto& prop : props)
        {
            if (prop.type == TypeTag::Raw)                       continue;
            if (HasFlag(prop.flags, PropertyFlags::Hidden))      continue;
            if (!node[prop.name])                                continue;

            const YAML::Node& field = node[prop.name];

            switch (prop.type)
            {
            case TypeTag::UInt:
                *FieldPtr<uint32_t>(comp, prop) = field.as<uint32_t>();
                break;
            case TypeTag::Int:
                *FieldPtr<int32_t>(comp, prop) = field.as<int32_t>();
                break;
            case TypeTag::Float:
                *FieldPtr<float>(comp, prop) = field.as<float>();
                break;
            case TypeTag::Double:
                *FieldPtr<double>(comp, prop) = field.as<double>();
                break;
            case TypeTag::Bool:
                *FieldPtr<bool>(comp, prop) = field.as<bool>();
                break;
            case TypeTag::String:
                *FieldPtr<std::string>(comp, prop) = field.as<std::string>();
                break;
            case TypeTag::Vec2:
                {
                    auto& v = *FieldPtr<HM::Vector2>(comp, prop);
                    v.x() = field[0].as<float>();
                    v.y() = field[1].as<float>();
                }
                break;
            case TypeTag::Vec3:
                {
                    auto& v = *FieldPtr<HM::Vector3>(comp, prop);
                    v.x() = field[0].as<float>();
                    v.y() = field[1].as<float>();
                    v.z() = field[2].as<float>();
                }
                break;
            case TypeTag::Vec4:
                {
                    auto& v = *FieldPtr<HM::Vector4>(comp, prop);
                    v.x() = field[0].as<float>();
                    v.y() = field[1].as<float>();
                    v.z() = field[2].as<float>();
                    v.w() = field[3].as<float>();
                }
                break;
            case TypeTag::Enum:
                {
                    int32_t val = field.as<int32_t>();
                    std::memcpy(prop.accessor(comp), &val, sizeof(int32_t));
                }
                break;
            default:
                break;
            }
        }
    }
}
