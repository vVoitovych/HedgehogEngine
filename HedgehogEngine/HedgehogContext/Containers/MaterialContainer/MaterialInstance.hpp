#pragma once

#include "Material.hpp"

#include <string>

namespace Context
{
   
    class MaterilInstance
    {
    public:
        MaterilInstance(const std::string& material) : mMaterial(material) {}


        const ShaderParameters& GetVertexShaderParameters() const;
        ShaderParameters& GetVertexShaderParameters();
        const ShaderParameters& GetFragmentShaderParameters() const;
        ShaderParameters& GetFragmentShaderParameters();
        const ShaderParameters& GetTesselationShaderParameters() const;
        ShaderParameters& GetTesselationShaderParameters();
        const ShaderParameters& GetGeometryShaderParameters() const;
        ShaderParameters& GetGeometryShaderParameters();
    private:
        std::string mMaterial;

        ShaderParameters mVertexShaderParameters;
        ShaderParameters mTesselationShaderParameters;
        ShaderParameters mGeometryShaderParameters;
        ShaderParameters mFragmentShaderParameters;

    };

}




