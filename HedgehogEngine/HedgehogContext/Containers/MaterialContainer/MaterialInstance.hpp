#pragma once

#include "Material.hpp"

#include <string>

namespace Context
{
   
    class MaterialInstance
    {
    public:
        MaterialInstance(const std::string& path, const Material& material);

        void UpdateMaterialInstance(const Material& material);

        const ShaderParameters& GetVertexShaderParameters() const;
        ShaderParameters& GetVertexShaderParameters();
        const ShaderParameters& GetFragmentShaderParameters() const;
        ShaderParameters& GetFragmentShaderParameters();

    private:
        std::string m_MaterialPath;
        std::string m_Path;
        ShaderParameters m_VertexShaderParameters;
        ShaderParameters m_FragmentShaderParameters;

    };

}




