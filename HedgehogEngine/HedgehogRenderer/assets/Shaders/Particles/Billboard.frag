#version 450

// A billboard's colour: its emitter's texture, or a soft round dot without one, times its colour
// over life. Written as HDR radiance, blended over what is behind (alpha or additive).

#include "Particles/Billboard.glsl"

layout(set = 1, binding = 0) uniform sampler2D particleTexture;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inCorner;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 shape = draw.HasTexture != 0u ? texture(particleTexture, inUv)
                                       : vec4(1.0, 1.0, 1.0, smoothstep(1.0, 0.0, length(inCorner)));
    outColor = inColor * shape;
}
