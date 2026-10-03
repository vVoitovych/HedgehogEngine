#include "Bindings.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace HedgehogScripting::Bindings
{
    std::string ToAssetPath(std::string path, const char* singular, const char* plural)
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";

        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.starts_with(ASSETS_PREFIX))
            path.erase(0, ASSETS_PREFIX.size());
        if (path.empty())
            throw std::runtime_error(std::string(singular) + " needs a path under assets://");
        if (path.find(':') != std::string::npos || path.front() == '/')
            throw std::runtime_error(std::string(plural) + " load only from assets://, not '" + path + "'");
        size_t start = 0;
        while (start <= path.size())
        {
            const size_t end = std::min(path.find('/', start), path.size());
            if (std::string_view(path).substr(start, end - start) == "..")
                throw std::runtime_error(std::string(singular) + " path may not leave assets:// ('" + path + "')");
            start = end + 1;
        }
        return path;
    }
}
