#include "MaterialInstance.hpp"

namespace Context
{



	const ShaderParameters& MaterilInstance::GetVertexShaderParameters() const
	{
		return mVertexShaderParameters;
	}
	ShaderParameters& MaterilInstance::GetVertexShaderParameters()
	{
		return mVertexShaderParameters;
	}
	const ShaderParameters& MaterilInstance::GetFragmentShaderParameters() const
	{
		return mFragmentShaderParameters;
	}
	ShaderParameters& MaterilInstance::GetFragmentShaderParameters()
	{
		return mFragmentShaderParameters;
	}
	const ShaderParameters& MaterilInstance::GetTesselationShaderParameters() const
	{
		return mTesselationShaderParameters;
	}
	ShaderParameters& MaterilInstance::GetTesselationShaderParameters()
	{
		return mTesselationShaderParameters;
	}
	const ShaderParameters& MaterilInstance::GetGeometryShaderParameters() const
	{
		return mGeometryShaderParameters;
	}
	ShaderParameters& MaterilInstance::GetGeometryShaderParameters()
	{
		return mGeometryShaderParameters;
	}

}

