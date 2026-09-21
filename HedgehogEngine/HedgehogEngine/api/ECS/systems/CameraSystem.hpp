#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    // A pure entity view over every CameraComponent, tracked by signature like any other
    // system. It does not render, own a graph, or hold a target — those belong to the View
    // that HE-78 (RENDERING.md section 3.2) derives from each enabled camera at extraction.
    // GetEntities() (inherited from ECS::System) is the whole interface for now.
    class CameraSystem : public ECS::System
    {
    };
}
