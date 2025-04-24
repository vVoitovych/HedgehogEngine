#include "MaterialSerializer.hpp"
#include "ShaderParameters.hpp"

#include "../MaterialData.hpp"
#include "MaterialFrontend.hpp"
#include "MaterialInstance.hpp"
#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"
#include "Logger/Logger.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>
namespace YAML {

	template<>
	struct convert<HM::Vector2>
	{
		static Node encode(const HM::Vector2& rhs)
		{
			Node node;
			node.push_back(rhs.x());
			node.push_back(rhs.y());
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Vector2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x() = node[0].as<float>();
			rhs.y() = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<HM::Vector3>
	{
		static Node encode(const HM::Vector3& rhs)
		{
			Node node;
			node.push_back(rhs.x());
			node.push_back(rhs.y());
			node.push_back(rhs.z());
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Vector3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x() = node[0].as<float>();
			rhs.y() = node[1].as<float>();
			rhs.z() = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<HM::Vector4>
	{
		static Node encode(const HM::Vector4& rhs)
		{
			Node node;
			node.push_back(rhs.x());
			node.push_back(rhs.y());
			node.push_back(rhs.z());
			node.push_back(rhs.w());
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Vector4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x() = node[0].as<float>();
			rhs.y() = node[1].as<float>();
			rhs.z() = node[2].as<float>();
			rhs.w() = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<HM::Matrix4x4>
	{
		static Node encode(const HM::Matrix4x4& rhs)
		{
			Node node;
			node.push_back(rhs[0][0]);
			node.push_back(rhs[0][1]);
			node.push_back(rhs[0][2]);
			node.push_back(rhs[0][3]);

			node.push_back(rhs[1][0]);
			node.push_back(rhs[1][1]);
			node.push_back(rhs[1][2]);
			node.push_back(rhs[1][3]);

			node.push_back(rhs[2][0]);
			node.push_back(rhs[2][1]);
			node.push_back(rhs[2][2]);
			node.push_back(rhs[2][3]);

			node.push_back(rhs[3][0]);
			node.push_back(rhs[3][1]);
			node.push_back(rhs[3][2]);
			node.push_back(rhs[3][3]);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Matrix4x4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs[0][0] = node[0].as<float>();
			rhs[0][1] = node[1].as<float>();
			rhs[0][2] = node[2].as<float>();
			rhs[0][3] = node[3].as<float>();

			rhs[1][0] = node[4].as<float>();
			rhs[1][1] = node[5].as<float>();
			rhs[1][2] = node[6].as<float>();
			rhs[1][3] = node[7].as<float>();

			rhs[2][0] = node[8].as<float>();
			rhs[2][1] = node[9].as<float>();
			rhs[2][2] = node[10].as<float>();
			rhs[2][3] = node[11].as<float>();

			rhs[3][0] = node[12].as<float>();
			rhs[3][1] = node[13].as<float>();
			rhs[3][2] = node[14].as<float>();
			rhs[3][3] = node[15].as<float>();
			return true;
		}
	};
}

namespace Context
{
	void MaterialSerializer::Serialize(MaterialData& material, std::string materialPath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << static_cast<size_t>(material.type);
		out << YAML::Key << "BaseColor" << YAML::Value << material.baseColor;
		out << YAML::Key << "Transparency" << YAML::Value << material.transparency;
		out << YAML::EndMap;

		std::ofstream fout(materialPath);
		fout << out.c_str();
	}

	void MaterialSerializer::Deserialize(MaterialData& material, std::string materialPath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(materialPath);
		}
		catch (YAML::ParserException e)
		{
			LOGERROR("Failed to load scene: ", materialPath, " with error: ", e.what());
			return;
		}
		material.path = ContentLoader::GetAssetRelativetlyPath(materialPath);
		material.type =	static_cast<MaterialType>(data["Type"].as<size_t>());
		material.baseColor = data["BaseColor"].as<std::string>();
		material.transparency = data["Transparency"].as<float>();
	}

	void MaterialSerializer::Serialize(MaterialFrontend& material, std::string materialPath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << static_cast<size_t>(material.GetSurfaceType());
		out << YAML::Key << "Culling" << YAML::Value << static_cast<size_t>(material.GetCullingType());
		out << YAML::Key << "VertexShader" << YAML::Value << material.GetVertexShader();
		out << YAML::Key << "FragmentShader" << YAML::Value << material.GetFragmentShader();
		out << YAML::EndMap;

		std::ofstream fout(materialPath);
		fout << out.c_str();
	}

	void MaterialSerializer::Deserialize(MaterialFrontend& material, std::string materialPath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(materialPath);
		}
		catch (YAML::ParserException e)
		{
			LOGERROR("Failed to load scene: ", materialPath, " with error: ", e.what());
			return;
		}
		std::string vertexShader = data["VertexShader"].as<std::string>();
		std::string fragmentShader = data["FragmentShader"].as<std::string>();
		MaterialSurfaceType materialType = static_cast<MaterialSurfaceType>(data["Type"].as<size_t>());
		CullingType cullingType = static_cast<CullingType>(data["Culling"].as<size_t>());

		material.SetCullingType(cullingType);
		material.SetSurfaceType(materialType);
		material.SetVertexShader(vertexShader);
		material.SetFragmentShader(fragmentShader);
	}

	template <size_t componentCount, typename elementType>
	inline YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector<componentCount, elementType>& vec) 
	{
		out << YAML::BeginSeq;
		for (size_t i = 0; i < componentCount; ++i) 
		{
			out << vec[i];
		}
		out << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Matrix4x4& mat)
	{
		out << YAML::BeginSeq;
		for (size_t i = 0; i < 4; ++i)
		{
			out << mat[i];
		}
		out << YAML::EndSeq;
		return out;
	}

	void SerializeShaderParameter(YAML::Emitter& out, const ShaderParameterValue& param)
	{
		out << YAML::BeginMap;

		out << YAML::Key << "Type" << YAML::Value << static_cast<size_t>(param.type);
		switch (param.type)
		{
		case ShaderParamType::Float:
			out << YAML::Key << "Data" << YAML::Value << param.Get<float>();
			break;
		case ShaderParamType::Vec2:
			out << YAML::Key << "Data" << YAML::Value << param.Get<HM::Vector2>();
			break;
		case ShaderParamType::Vec3:
			out << YAML::Key << "Data" << YAML::Value << param.Get<HM::Vector3>();
			break;
		case ShaderParamType::Vec4:
			out << YAML::Key << "Data" << YAML::Value << param.Get<HM::Vector4>();
			break;
		case ShaderParamType::Mat4:
			out << YAML::Key << "Data" << YAML::Value << param.Get<HM::Matrix4x4>();
			break;
		case ShaderParamType::Texture:
			out << YAML::Key << "Texture" << YAML::Value << param.texture;
			break;
		default:
			break;
		}

		out << YAML::EndMap;
	}

	void MaterialSerializer::Serialize(MaterialInstance& materialInstance, std::string materialInstancePath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "MaterialPath" << YAML::Value << materialInstance.GetMaterialPath();

		auto vertexParams = materialInstance.GetVertexShaderParameters().GetAll();
		out << YAML::Key << "VertexShaderParameters";
		out << YAML::BeginMap;
		for (auto param : vertexParams)
		{
			out << YAML::Key << param.first;
			SerializeShaderParameter(out, param.second);
		}
		out << YAML::EndMap;

		auto fragmentParams = materialInstance.GetFragmentShaderParameters().GetAll();
		out << YAML::Key << "FragmentShaderParameters";
		for (auto param : fragmentParams)
		{
			out << YAML::Key << param.first;
			SerializeShaderParameter(out, param.second);
		}
		out << YAML::EndMap;

		out << YAML::EndMap;
		std::ofstream fout(materialInstancePath);
		fout << out.c_str();

	}

	void DeserialiseShaderParameters(YAML::Node& vertexNodes, ShaderParameters& params)
	{
		for (const auto& pair : vertexNodes)
		{
			std::string name = pair.first.as<std::string>();
			auto param = pair.second;
			ShaderParamType type = static_cast<ShaderParamType>(param["Type"].as<size_t>());
			switch (type)
			{
			case Context::ShaderParamType::Float:
			{
				if (params.Has(name))
				{
					float data = param["Data"].as<float>();
					params.SetParam(name, type, &data, sizeof(float));
				}
				break;
			}
			case Context::ShaderParamType::Vec2:
			{
				if (params.Has(name))
				{
					HM::Vector2 data = param["Data"].as<HM::Vector2>();
					params.SetParam(name, type, &data, sizeof(HM::Vector2));
				}
				break;
			}
			case Context::ShaderParamType::Vec3:
			{
				if (params.Has(name))
				{
					HM::Vector3 data = param["Data"].as<HM::Vector3>();
					params.SetParam(name, type, &data, sizeof(HM::Vector3));
				}
				break;
			}
			case Context::ShaderParamType::Vec4:
			{
				if (params.Has(name))
				{
					HM::Vector4 data = param["Data"].as<HM::Vector4>();
					params.SetParam(name, type, &data, sizeof(HM::Vector4));
				}
				break;
			}
			case Context::ShaderParamType::Mat4:
			{
				if (params.Has(name))
				{
					HM::Matrix4x4 data = param["Data"].as<HM::Matrix4x4>();
					params.SetParam(name, type, &data, sizeof(HM::Matrix4x4));
				}
				break;
			}
			case Context::ShaderParamType::Texture:
			{
				if (params.Has(name))
				{
					std::string texture = param["Texture"].as<std::string>();
					params.SetTexture(name, texture);
				}
				break;
			}
			case Context::ShaderParamType::Unknown:
				break;
			default:
				break;
			}
		}
	}

	void MaterialSerializer::Deserialize(MaterialInstance& materialInstance, std::string materialInstancePath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(materialInstancePath);
		}
		catch (YAML::ParserException e)
		{
			LOGERROR("Failed to load scene: ", materialInstancePath, " with error: ", e.what());
			return;
		}

		materialInstance.SetMaterialPath(data["MaterialPath"].as<std::string>());
		{
			auto vertexNodes = data["VertexShaderParameters"];
			auto vertexShaderParam = materialInstance.GetVertexShaderParameters();
			DeserialiseShaderParameters(vertexNodes, vertexShaderParam);
		}
		{
			auto fragmentNodes = data["FragmentShaderParameters"];
			auto fragmentShaderParam = materialInstance.GetFragmentShaderParameters();
			DeserialiseShaderParameters(fragmentNodes, fragmentShaderParam);
		}

	}

}





