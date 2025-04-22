#include "MaterialInstance.hpp"

#include <stdexcept>

namespace Context
{
	MaterialInstance::MaterialInstance(const std::string& path, const Material& material)
		: m_Path(path)
	{
		if (!material.IsValid())
		{
			throw std::runtime_error("Trying to create material instence from invalid material!");
		}
		m_MaterialPath = material.GetMaterialPath();
		m_VertexShaderParameters = material.GetVertexShaderParameters();
		m_FragmentShaderParameters = material.GetFragmentShaderParameters();
	}

	void UpdateMaterialInstenceParameters(const ShaderParameters& materialParams, ShaderParameters& instanceParams)
	{
		auto allMaterialParams = materialParams.GetAll();
		auto allInstanseParams = instanceParams.GetAll();
		for (auto param : allMaterialParams)
		{
			if (allInstanseParams.contains(param.first))
			{
				auto instanceParam = allInstanseParams.find(param.first);
				if (instanceParam->second.type != param.second.type)
				{
					instanceParams.Remove(param.first);
					instanceParams.CreateParam(param.first, param.second.type);
				}
			}
			else
			{
				instanceParams.CreateParam(param.first, param.second.type);
			}
		}
		for (auto param : allInstanseParams)
		{
			if (!materialParams.Has(param.first))
			{
				instanceParams.Remove(param.first);
			}
		}
	}

	void MaterialInstance::UpdateMaterialInstance(const Material& material)
	{
		UpdateMaterialInstenceParameters(material.GetVertexShaderParameters(), m_VertexShaderParameters);
		UpdateMaterialInstenceParameters(material.GetFragmentShaderParameters(), m_FragmentShaderParameters);

	}

	const ShaderParameters& MaterialInstance::GetVertexShaderParameters() const
	{
		return m_VertexShaderParameters;
	}
	ShaderParameters& MaterialInstance::GetVertexShaderParameters()
	{
		return m_VertexShaderParameters;
	}
	const ShaderParameters& MaterialInstance::GetFragmentShaderParameters() const
	{
		return m_FragmentShaderParameters;
	}
	ShaderParameters& MaterialInstance::GetFragmentShaderParameters()
	{
		return m_FragmentShaderParameters;
	}


}

