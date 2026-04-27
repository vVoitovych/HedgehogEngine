#pragma once

#include "Scene/api/SceneApi.hpp"

#include <string>

namespace Scene
{
    class Scene;

    class SceneSerializer
    {
    public:
        SCENE_API static void SerializeScene(Scene& scene, std::string scenePath);

        SCENE_API static void DeserializeScene(Scene& scene, std::string scenePath);

    };
}




