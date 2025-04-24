#pragma once

#include <string>
#include <unordered_map>

namespace Context
{
    enum class ShaderParamType
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
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

    };

    class ShaderParameters
    {
    public:
        void CreateParam(const std::string& name, ShaderParamType type)
        {
            ShaderParameterValue param;
            param.type = type;
            m_Parameters[name] = param;
        }

        void SetParam(const std::string& name, ShaderParamType type, const void* value, size_t size)
        {
            ShaderParameterValue param;
            param.type = type;
            param.data.resize(size);
            memcpy(param.data.data(), value, size);
            m_Parameters[name] = param;
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
            m_Parameters[name] = param;
        }

        template<typename T>
        T Get(const std::string& name) const
        {
            auto it = m_Parameters.find(name);
            if (it != m_Parameters.end())
            {
                return it->second.Get<T>();
            }
            return T{};
        }

        bool Has(const std::string& name) const
        {
            return m_Parameters.contains(name);
        }

        void Remove(const std::string& name)
        {
            if (m_Parameters.contains(name))
            {
                m_Parameters.erase(name);
            }
        }

        std::string GetTexture(const std::string& name) const
        {
            auto it = m_Parameters.find(name);
            if (it != m_Parameters.end() && it->second.type == ShaderParamType::Texture)
            {
                return it->second.texture;
            }
            return {};
        }

        const std::unordered_map<std::string, ShaderParameterValue>& GetAll() const
        {
            return m_Parameters;
        }

        bool IsEmpty() const
        {
            return m_Parameters.empty();
        }

    private:
        std::unordered_map<std::string, ShaderParameterValue> m_Parameters;

    };

}



