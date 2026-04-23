#pragma once

#include "FrameData.hpp"
#include "MaterialDef.hpp"

#include <functional>
#include <cstdint>

namespace Scene
{
    class Scene;
}

namespace Context
{
    class Camera;
}

namespace FD
{
    class FrameDataBuilder
    {
    public:
        FrameData Build(
            const Scene::Scene&                       scene,
            const Context::Camera&                    camera,
            float                                     deltaTime,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup) const;

    private:
        void BuildDrawList(
            const Scene::Scene&                          scene,
            const std::function<MaterialType(uint64_t)>& materialTypeLookup,
            DrawList&                                    outDrawList) const;
    };
}
