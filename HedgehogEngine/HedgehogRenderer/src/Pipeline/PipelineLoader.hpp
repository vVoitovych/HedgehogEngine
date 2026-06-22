#pragma once

#include "RHI/api/RHITypes.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Renderer
{

// Parsed content of a single .pl pipeline file.
// m_DescriptorSets[i] holds the binding list for descriptor set i,
// in the order they appear in the file (position = set index in the pipeline layout).
struct PipelineFileDesc
{
    std::vector<std::vector<RHI::DescriptorBinding>> m_DescriptorSets;
    std::vector<RHI::PushConstantRange>              m_PushConstants;
};

class PipelineLoader
{
public:
    // Load and parse a .pl file.
    // virtualPath must be a virtual path, e.g. "engine://HedgehogEngine/.../ForwardPass.pl".
    static PipelineFileDesc Load(const std::string& virtualPath,
                                 const FS::FileSystemManager& fileSystem);

    // Build DescriptorPool sizes from a single set's bindings.
    // Each binding contributes (binding.m_Count * maxSets) descriptors of its type.
    // Bindings of the same type are merged into one PoolSize entry.
    static std::vector<RHI::PoolSize> MakePoolSizes(
        const std::vector<RHI::DescriptorBinding>& bindings,
        uint32_t maxSets);

private:
    static RHI::DescriptorType ParseDescriptorType(const std::string& s);
    static RHI::ShaderStage    ParseShaderStage(const std::string& s);
};

} // namespace Renderer
