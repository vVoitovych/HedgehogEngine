#pragma once

namespace EcsSerialization
{
    class ComponentTypeRegistry;
}

namespace HedgehogEngine
{
    // Registers the engine's 16 component types, in the order their serializers write a scene's
    // components (so scenes stay byte for byte what they were), with the names, groups and icon
    // names the editor offers them under. Mesh and Render add their default through MeshSystem and
    // RenderSystem, found in the ECS when the default is added.
    void RegisterEngineComponents(EcsSerialization::ComponentTypeRegistry& types);
}
