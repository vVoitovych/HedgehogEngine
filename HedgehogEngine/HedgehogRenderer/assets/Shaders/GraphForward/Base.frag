#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inWorldPosition;

#define MAX_LIGHTS_COUNT 16

#include "Common/Lighting.glsl"

layout(set = 0, binding = 0) uniform ViewData
{
    mat4 view;
    mat4 viewProj;
    vec4 eyePos;
} viewData;

layout(set = 1, binding = 0) uniform MaterialData
{
    float transparency;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(set = 2, binding = 0) uniform SceneLights
{
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} sceneLights;

layout(location = 0) out vec4 outColor;

void main()
{
    SurfaceData data;
    data.pos    = inWorldPosition;
    data.norm   = inNormal;
    data.albedo = texture(texSampler, fragTexCoord);
    outColor    = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < sceneLights.lightCount; ++i)
    {
        outColor += CalculateLight(sceneLights.lights[i], data, viewData.eyePos);
    }
    outColor.a = 1.0f;
}
