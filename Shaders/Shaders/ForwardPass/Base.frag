#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inWorldPosition;

#define MAX_LIGHTS_COUNT 16

#include "Common/Lighting.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 view;
    mat4 viewProj;
    vec4 eyePos;
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} ubo;

layout(set = 1, binding = 0) uniform MaterialData 
{
    float transparency;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    SurfaceData data;
    data.pos = inWorldPosition;
	data.norm = inNormal;
	data.albedo = texture(texSampler, fragTexCoord);
    outColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < ubo.lightCount; ++i)
    {
        outColor += CalculateLight(ubo.lights[i], data, ubo.eyePos);
    }
}

