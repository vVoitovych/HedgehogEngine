#include "PassResourceCache.hpp"

namespace Renderer
{
    void PassResourceCache::Cleanup(RHI::IRHIDevice& device)
    {
        (void)device;
        m_Entries.clear();
    }
}
