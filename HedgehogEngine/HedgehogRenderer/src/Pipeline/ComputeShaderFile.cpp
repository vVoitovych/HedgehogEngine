#include "ComputeShaderFile.hpp"

#include <yaml-cpp/yaml.h>

namespace Renderer
{
    ComputeShaderFile ParseComputeShaderFile(const std::string& text, const std::string& virtualPath)
    {
        ComputeShaderFile file;
        const auto refuse = [&](const std::string& why)
        {
            file = {};
            file.Error = virtualPath + ": " + why;
            return file;
        };

        YAML::Node root;
        try
        {
            root = YAML::Load(text);
        }
        catch (const YAML::Exception& e)
        {
            return refuse(std::string("does not parse: ") + e.what());
        }
        if (!root.IsMap())
            return refuse("is not a map");

        const YAML::Node layout = root["pipeline_layout"];
        if (!layout || !layout.IsScalar())
            return refuse("has no pipeline_layout");
        file.LayoutPath = layout.as<std::string>();

        const YAML::Node stages = root["shaders"];
        if (!stages || !stages.IsSequence() || stages.size() == 0)
            return refuse("has no shaders");
        for (const YAML::Node& stage : stages)
        {
            const YAML::Node name = stage["stage"];
            const YAML::Node path = stage["path"];
            if (!name || !name.IsScalar() || !path || !path.IsScalar())
                return refuse("has a shader without a stage and a path");
            if (name.as<std::string>() != "compute")
                return refuse("has a " + name.as<std::string>() + " stage; a compute shader has only a compute stage");
            if (!file.ShaderPath.empty())
                return refuse("has more than one compute stage");
            file.ShaderPath = path.as<std::string>();
        }
        return file;
    }
}
