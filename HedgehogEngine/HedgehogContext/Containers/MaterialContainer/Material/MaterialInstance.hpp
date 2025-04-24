#pragma once

#include "MaterialFrontend.hpp"

#include <string>
#include <memory>

namespace Context
{
    class ShaderParameters;

    class MaterialInstance
    {
    public:
        MaterialInstance();
        MaterialInstance(const std::string& path, const MaterialFrontend& material);

        void UpdateMaterialInstance(const MaterialFrontend& material);

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
        std::unique_ptr<ShaderParameters> m_VertexShaderParameters;
        std::unique_ptr<ShaderParameters> m_FragmentShaderParameters;

    };

}




