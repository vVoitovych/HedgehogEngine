#ifndef UNIFORM_FRAME_DATA_DATA
#define UNIFORM_FRAME_DATA_DATA

#define MAX_LIGHTS_COUNT 16

layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 view;
    mat4 proj;
    vec4 eyePos;
    Light lights[MAX_LIGHTS_COUNT];
    int lightCount;
} ubo;

#endif
