#pragma once

#include "PropertyDescriptor.hpp"
#include "../EcsSerializationApi.hpp"

#include "yaml-cpp/yaml.h"

#include <span>

namespace Reflection
{
    ECS_SERIALIZATION_API void YamlSerializeComponent(
        YAML::Emitter&                      out,
        void*                               comp,
        std::span<const PropertyDescriptor> props);

    ECS_SERIALIZATION_API void YamlDeserializeComponent(
        void*                               comp,
        const YAML::Node&                   node,
        std::span<const PropertyDescriptor> props);
}
