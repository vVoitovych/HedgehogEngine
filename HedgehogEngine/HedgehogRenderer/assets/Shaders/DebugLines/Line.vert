#version 450

// The Gizmo pass's debug lines: world-space vertex pairs with an RGBA8 colour picked in sRGB, which
// is decoded to linear here, as GameUi/Quad.vert does.

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 viewProj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor    = vec4(pow(inColor.rgb, vec3(2.2f)), inColor.a);
    gl_Position = ubo.viewProj * vec4(inPosition, 1.0f);
}
