#include "MaterialInstance.hpp"
#include "ShaderParameters.hpp"

#include <stdexcept>

namespace Context
{
	MaterialInstance::MaterialInstance()
	{
	}

	MaterialInstance::MaterialInstance(const std::string& path, const MaterialFrontend& material)
		: m_Path(path)
	{
		if (!material.IsValid())
		{
			throw std::runtime_error("Trying to create material instence from invalid material!");
		}
		m_MaterialPath = material.GetMaterialPath();
		m_VertexShaderParameters = std::make_unique<ShaderParameters>(material.GetVertexShaderParameters());
		m_FragmentShaderParameters = std::make_unique<ShaderParameters>(material.GetFragmentShaderParameters());
	}

	void UpdateMaterialInstenceParameters(const ShaderParameters& materialParams, std::unique_ptr<ShaderParameters>& instanceParams)
	{
		auto allMaterialParams = materialParams.GetAll();
		auto allInstanseParams = instanceParams->GetAll();
		for (auto param : allMaterialParams)
		{
			if (allInstanseParams.contains(param.first))
			{
				auto instanceParam = allInstanseParams.find(param.first);
				if (instanceParam->second.type != param.second.type)
				{
					instanceParams->Remove(param.first);
					instanceParams->CreateParam(param.first, param.second.type);
				}
			}
			else
			{
				instanceParams->CreateParam(param.first, param.second.type);
			}
		}
		for (auto param : allInstanseParams)
		{
			if (!materialParams.Has(param.first))
			{
				instanceParams->Remove(param.first);
			}
		}
	}

	void MaterialInstance::UpdateMaterialInstance(const MaterialFrontend& material)
	{
		UpdateMaterialInstenceParameters(material.GetVertexShaderParameters(), m_VertexShaderParameters);
		UpdateMaterialInstenceParameters(material.GetFragmentShaderParameters(), m_FragmentShaderParameters);

	}

	const ShaderParameters& MaterialInstance::GetVertexShaderParameters() const
	{
		return *m_VertexShaderParameters;
	}
	ShaderParameters& MaterialInstance::GetVertexShaderParameters()
	{
		return *m_VertexShaderParameters;
	}
	const ShaderParameters& MaterialInstance::GetFragmentShaderParameters() const
	{
		return *m_FragmentShaderParameters;
	}
	ShaderParameters& MaterialInstance::GetFragmentShaderParameters()
	{
		return *m_FragmentShaderParameters;
	}

	std::string MaterialInstance::GetPath() const
	{
		return m_Path;
	}

	std::string MaterialInstance::GetMaterialPath() const
	{
		return m_MaterialPath;
	}

	void MaterialInstance::SetPath(const std::string& path)
	{
		m_Path = path;
	}

	void MaterialInstance::SetMaterialPath(const std::string& path)
	{
		m_MaterialPath = path;
	}


}

