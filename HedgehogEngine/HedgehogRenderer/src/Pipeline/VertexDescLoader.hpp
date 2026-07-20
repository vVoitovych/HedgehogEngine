#pragma once

#include "RHI/api/RHITypes.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Renderer
{

struct VertexFileDesc
{
    std::vector<RHI::VertexBinding>   Bindings;
    std::vector<RHI::VertexAttribute> Attributes;
};

class VertexDescLoader
{
public:
    // Load and parse a .vdes file.
    // virtualPath must be a virtual path, e.g. "engine://HedgehogEngine/.../PositionOnly.vdes".
    static VertexFileDesc Load(const std::string& virtualPath,
                               const FS::FileSystemManager& fileSystem);

private:
    static RHI::Format          ParseFormat(const std::string& s);
    static RHI::VertexInputRate ParseInputRate(const std::string& s);
};

} // namespace Renderer
