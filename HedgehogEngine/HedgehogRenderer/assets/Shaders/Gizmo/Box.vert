#version 450

// The Gizmo pass: one unit cube's edges per editor-layer instance, mapped onto its world bounds.

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 viewProj;
} ubo;

layout(location = 0) in vec3 inPosition;

layout( push_constant ) uniform constants
{
    mat4 model;
} PushConstants;

void main()
{
    gl_Position = ubo.viewProj * PushConstants.model * vec4(inPosition, 1.0f);
}
