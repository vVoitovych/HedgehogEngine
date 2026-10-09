#version 450

// The environment's radiance seen along each pixel's direction: the cube at mip 0 (the environment
// itself), turned by its rotation about +Z and times its intensity, as HDR radiance.

layout(location = 0) in vec2 inNdc;

layout(set = 0, binding = 0) uniform samplerCube radianceCube;

// SkyboxPushConstants.
layout(push_constant) uniform SkyboxData
{
    mat4  inverseViewProj; // clip space to a world direction (the camera's translation removed)
    float rotationCos;
    float rotationSin;
    float intensity;
} skybox;

layout(location = 0) out vec4 outColor;

void main()
{
    const vec4 world     = skybox.inverseViewProj * vec4(inNdc, 1.0f, 1.0f);
    const vec3 direction = normalize(world.xyz / world.w);
    // Turned back by the environment's rotation, as Pbr.glsl's EnvironmentDirection does.
    const float c = skybox.rotationCos;
    const float s = skybox.rotationSin;
    const vec3  d = vec3(c * direction.x + s * direction.y, -s * direction.x + c * direction.y, direction.z);
    outColor = vec4(textureLod(radianceCube, d, 0.0f).rgb * skybox.intensity, 1.0f);
}
