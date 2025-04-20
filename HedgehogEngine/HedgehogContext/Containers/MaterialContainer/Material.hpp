#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Context
{
    enum class ShaderParamType
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
        Texture,
        Unknown
    };

    struct ShaderParameterValue
    {
        ShaderParamType type;
        std::vector<uint8_t> data;
        std::string texture;

        template<typename T>
        void Set(const T& value)
        {
            data.resize(sizeof(T));
            memcpy(data.data(), &value, sizeof(T));
        }

        template<typename T>
        T Get() const
        {
            T value{};
            memcpy(&value, data.data(), sizeof(T));
            return value;
        }

        void SetTexturePath(const std::string& path) {
            type = ShaderParamType::Texture;
            texture = path;
            data.clear();
        }

        std::string GetTexturePath() const {
            return texture;
        }

    };

    class ShaderParameters
    {
    public:
        void CreateParam(const std::string& name, ShaderParamType type)
        {
            ShaderParameterValue param;
            param.type = type;
            mParameters[name] = param;
        }

        void SetParam(const std::string& name, ShaderParamType type, const void* value, size_t size)
        {
            ShaderParameterValue param;
            param.type = type;
            param.data.resize(size);
            memcpy(param.data.data(), value, size);
            mParameters[name] = param;
        }

        template<typename T>
        void Set(const std::string& name, ShaderParamType type, const T& value)
        {
            SetParam(name, type, &value, sizeof(T));
        }

        void SetTexture(const std::string& name, const std::string& path)
        {
            ShaderParameterValue param;
            param.type = ShaderParamType::Texture;
            param.texture = path;
            mParameters[name] = param;
        }

        template<typename T>
        T Get(const std::string& name) const
        {
            auto it = mParameters.find(name);
            if (it != mParameters.end())
            {
                return it->second.Get<T>();
            }
            return T{};
        }

        std::string GetTexture(const std::string& name) const
        {
            auto it = mParameters.find(name);
            if (it != mParameters.end() && it->second.type == ShaderParamType::Texture)
            {
                return it->second.GetTexturePath();
            }
            return {};
        }

        const std::unordered_map<std::string, ShaderParameterValue>& GetAll() const
        {
            return mParameters;
        }

        bool IsEmpty() const
        {
            return mParameters.empty();
        }

    private:
        std::unordered_map<std::string, ShaderParameterValue> mParameters;

    };


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

	class Material
	{
    public:

        ShaderParameters GetVertexShaderParameters() const;
        ShaderParameters GetFragmentShaderParameters() const;

    private:
        void ParseVertexShaderParemeters(const std::vector<uint32_t>& spirv);
        void ParseFragmentShaderParemeters(const std::vector<uint32_t>& spirv);

    private:

		std::string path;

		std::string vertexShader;
		std::string fragmentShader;

		MaterialSurfaceType materialType;
		CullingType cullingType;

        ShaderParameters mVertexShaderParameters;
        ShaderParameters mFragmentShaderParameters;

	};

}


