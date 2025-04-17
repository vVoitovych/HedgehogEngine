#pragma once

#include <vector>
#include <unordered_map>
#include <string>

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
        Int,
        Bool,
        Unknown
    };

    struct ShaderParameterValue 
    {
        ShaderParamType type;
        std::vector<uint8_t> data; 

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
    };

    class ShaderParameters 
    {
    public:
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

    class MaterilInstance
    {
    public:

    private:
        ShaderParameters mVertexShaderParameters;
        ShaderParameters mTesselationShaderParameters;
        ShaderParameters mGeometryShaderParameters;
        ShaderParameters mFragmentShaderParameters;

    };

}




