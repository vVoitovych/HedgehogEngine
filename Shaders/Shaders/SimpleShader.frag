#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inWorldPosition;

#include "Common/Lighting.glsl"
#include "Common/UniformFrameData.glsl"

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
    outColor = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < ubo.lightCount; ++i)
    {
        outColor += CalculateLight(ubo.lights[i], data, ubo.eyePos);
    }
}

