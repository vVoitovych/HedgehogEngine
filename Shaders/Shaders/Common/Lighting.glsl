#ifndef LIGHTING
#define LIGHTING

#define EPS 0.0000001;

#include "Common/LightData.glsl"
#include "Common/SurfaceData.glsl"

vec4 CalculateDirectional(Light light, SurfaceData data, vec4 eyePos)
{
	float attenuation = max(dot(light.direction, data.norm.xyz), 0.0f);
	vec4 resLight = vec4(light.color, 0.0f) * light.data[1] * attenuation;
	return resLight;
}

vec4 CalculatePoint(Light light, SurfaceData data, vec4 eyePos)
{
	vec3 lightDir = data.pos.xyz - light.position;
	float magnitude = dot(lightDir, lightDir);
	lightDir = normalize(lightDir);

	float attenuation = 1.0/(0.2 + magnitude * light.data[2] * light.data[2]);
	vec4 resLight = vec4(light.color, 0.0) * light.data[1] * attenuation;
	return resLight;
}

vec4 CalculateSpot(Light light, SurfaceData data, vec4 eyePos)
{
	vec3 lightDir = data.pos.xyz - light.position;
	float magnitude = dot(lightDir, lightDir);
	lightDir = normalize(lightDir);

	float angleAttenuation = (dot(lightDir, light.direction) > light.data[3]) ? 1.0f : 0.0f;

	float attenuation = angleAttenuation/(0.2f + magnitude * light.data[2] * light.data[2]);
	vec4 resLight = vec4(light.color, 0.0f) * light.data[1] * attenuation;
	return resLight;
}

vec4 CalculateLight(Light light, SurfaceData data, vec4 eyePos)
{
	if (light.data[0] < 0.5f)
	{
		return CalculateDirectional(light, data, eyePos) * data.albedo;
	}
	else if (light.data[0] < 1.5f)
	{
		return CalculatePoint(light, data, eyePos) * data.albedo;
	}
	else if (light.data[0] < 2.5f)
	{
		return CalculateSpot(light, data, eyePos) * data.albedo;
	}
}



#endif

