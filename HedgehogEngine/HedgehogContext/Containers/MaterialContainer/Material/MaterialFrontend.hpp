#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Context
{
    class ShaderParameters;

   	enum class MaterialSurfaceType
	{
		Opaque,
		Cutoff,
		Transparent
	};

	enum class CullingType
	{
		None, 
		Back, 
		Front
	};

	class MaterialFrontend
	{
    public:
        MaterialFrontend();
        ~MaterialFrontend();
        MaterialFrontend(const std::string& path);

        void SetVertexShader(const std::string& path);
        void SetFragmentShader(const std::string& path);

        const std::string& GetMaterialPath() const;
        ShaderParameters GetVertexShaderParameters() const;
        ShaderParameters GetFragmentShaderParameters() const;

        bool IsValid() const;

        MaterialSurfaceType GetSurfaceType() const;
        void SetSurfaceType(MaterialSurfaceType type);

        CullingType GetCullingType() const;
        void SetCullingType(CullingType type);

        std::string GetVertexShader() const;
        std::string GetFragmentShader() const;

    private:
        void ParseShaderParameters(const std::vector<uint32_t>& spirvData, ShaderParameters& params);

    private:

		std::string m_Path;

		std::string m_VertexShader;
        std::vector<uint32_t> m_VertexShaderSPIRV;
        std::unique_ptr<ShaderParameters> m_VertexShaderParameters;

		std::string m_FragmentShader;
        std::vector<uint32_t> m_FragmentShaderSPIRV;
        std::unique_ptr<ShaderParameters> m_FragmentShaderParameters;


		MaterialSurfaceType m_MaterialType = MaterialSurfaceType::Opaque;
        CullingType m_CullingType = CullingType::None;

	};

}


