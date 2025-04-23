#pragma once

#include "Material.hpp"

#include <string>

namespace Context
{
   
    class MaterialInstance
    {
    public:
        MaterialInstance();
        MaterialInstance(const std::string& path, const Material& material);

        void UpdateMaterialInstance(const Material& material);

        const ShaderParameters& GetVertexShaderParameters() const;
        ShaderParameters& GetVertexShaderParameters();
        const ShaderParameters& GetFragmentShaderParameters() const;
        ShaderParameters& GetFragmentShaderParameters();

        std::string GetPath() const;
        std::string GetMaterialPath() const;
        void SetPath(const std::string& path);
        void SetMaterialPath(const std::string& path);
    private:
        std::string m_Path;
        std::string m_MaterialPath;
        ShaderParameters m_VertexShaderParameters;
        ShaderParameters m_FragmentShaderParameters;

    };

}




