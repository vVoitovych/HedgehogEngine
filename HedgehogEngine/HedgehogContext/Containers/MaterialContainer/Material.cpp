#include "Material.hpp"

#include "ThirdParty/SPIRV-Reflect/SPIRV-Reflect/spirv_reflect.h"
#include "Logger/Logger.hpp"

#include <vector>
#include <unordered_map>

namespace Context
{
    std::string GetTypeName(const SpvReflectBlockVariable& member)
    {
        uint32_t width = member.numeric.scalar.width;
        uint32_t components = member.numeric.vector.component_count;
        uint32_t rows = member.numeric.matrix.row_count;
        uint32_t cols = member.numeric.matrix.column_count;

        std::string typeStr;

        if (cols > 1 && rows > 1)
        {
            typeStr = "mat" + std::to_string(cols) + "x" + std::to_string(rows);
        }

        else if (components > 1)
        {
            typeStr = "vec" + std::to_string(components);
        }

        else
        {
            typeStr = "float";
        }

        if (member.array.dims_count > 0)
        {
            for (uint32_t i = 0; i < member.array.dims_count; ++i)
            {
                typeStr += "[" + std::to_string(member.array.dims[i]) + "]";
            }
        }

        return typeStr;
    }

    ShaderParamType TypeNameToType(const std::string& type)
    {
        std::unordered_map<std::string, ShaderParamType> dictionary =
        {
            {"float", ShaderParamType::Float},
            {"vec2", ShaderParamType::Vec2},
            {"vec3", ShaderParamType::Vec3},
            {"vec4", ShaderParamType::Vec4},
            {"mat3x3", ShaderParamType::Mat3},
            {"mat4x4", ShaderParamType::Mat4},
            {"texture", ShaderParamType::Texture}
        };
        if (dictionary.contains(type))
        {
            return dictionary[type];
        }
        else
        {
            return ShaderParamType::Unknown;
        }
    }

    void ParseShaderParameters(const std::vector<uint32_t>& spirvData, ShaderParameters& params)
    {
        if (spirvData.empty())
            return;

        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(spirvData.size() * sizeof(uint32_t), spirvData.data(), &module);

        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            LOGERROR("Failed to create SPIRV reflection module");
            return;
        }

        uint32_t count = 0;
        spvReflectEnumerateDescriptorBindings(&module, &count, nullptr);
        std::vector<SpvReflectDescriptorBinding*> bindings(count);
        spvReflectEnumerateDescriptorBindings(&module, &count, bindings.data());

        for (const auto* binding : bindings)
        {
            if (strcmp(binding->name, "ubo") == 0)
                continue;

            LOGVERBOSE("Name: ", binding->name, ", Binding: ", binding->binding, ", Set: ", binding->set, ", DescriptorType: ", binding->descriptor_type);

            if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
            {
                params.CreateParam(binding->name, ShaderParamType::Texture);
            }

            if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                const SpvReflectBlockVariable& block = binding->block;

                for (uint32_t i = 0; i < block.member_count; ++i)
                {
                    const SpvReflectBlockVariable& member = block.members[i];
                    auto type = GetTypeName(member);
                    auto paramType = TypeNameToType(type);
                    LOGVERBOSE("\tMember: ", member.name, ", Offset: ", member.offset, ", Size: ", member.size, ", Type: ", type);
                    if (paramType != ShaderParamType::Unknown)
                    {
                        params.CreateParam(member.name, paramType);
                    }
                }
            }
        }

        spvReflectDestroyShaderModule(&module);
    }

    void Material::ParseVertexShaderParemeters(const std::vector<uint32_t>& spirv)
    {
        ParseShaderParameters(spirv, mVertexShaderParameters);
    }

    void Material::ParseFragmentShaderParemeters(const std::vector<uint32_t>& spirv)
    {
        ParseShaderParameters(spirv, mFragmentShaderParameters);
    }

    ShaderParameters Material::GetVertexShaderParameters() const
    {
        return mVertexShaderParameters;
    }
    ShaderParameters Material::GetFragmentShaderParameters() const
    {
        return mFragmentShaderParameters;
    }

}

