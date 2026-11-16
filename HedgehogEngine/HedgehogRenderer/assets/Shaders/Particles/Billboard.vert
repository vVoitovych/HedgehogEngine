#version 450

// One emitter instance's alive particles (gl_InstanceIndex into the alive list the simulation just
// filled, or for an alpha emitter into the view's sort list) as camera-facing quads of six vertices: sized and coloured over their life, turned by their
// rotation, their flipbook frame chosen by age.

#define PARTICLE_DRAW
#include "Particles/Common.glsl"
#include "Particles/Billboard.glsl"

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outCorner; // -1 to 1 across the quad, for the soft dot

const vec2 CORNERS[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
                               vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

vec4 SampleGradient(EmitterData emitter, float t)
{
    float x = clamp(t, 0.0, 1.0) * 31.0;
    uint  a = uint(floor(x));
    uint  b = min(a + 1u, 31u);
    return mix(emitter.ColorOverLife[a], emitter.ColorOverLife[b], x - float(a));
}

void main()
{
    EmitterData emitter  = emitters[draw.EmitterIndex];
    // An alpha emitter draws through the view's sort list, farthest first; an additive one in any order.
    uint        instance = uint(gl_InstanceIndex);
    uint        index    = emitter.Blend == 0u ? sortList[2u * emitter.SlotFirst + instance].y
                                               : aliveOut[emitter.SlotFirst + instance];
    Particle    particle = particles[index];
    float       age      = particle.PositionAge.w;
    float       t        = age / particle.VelocityLifetime.w;

    vec2  corner = CORNERS[gl_VertexIndex];
    float turn   = particle.SizeRotation.y;
    vec2  turned = vec2(corner.x * cos(turn) - corner.y * sin(turn), corner.x * sin(turn) + corner.y * cos(turn));
    float size   = particle.SizeRotation.x * SampleCurve(emitter.SizeOverLife, t);

    vec3 center = (emitter.DrawMatrix * vec4(particle.PositionAge.xyz, 1.0)).xyz;
    vec3 world  = center + (draw.CameraRight.xyz * turned.x + draw.CameraUp.xyz * turned.y) * (size * 0.5);
    gl_Position = draw.ViewProj * vec4(world, 1.0);

    uint tilesX = max(emitter.SheetTilesX, 1u);
    uint tilesY = max(emitter.SheetTilesY, 1u);
    uint frames = tilesX * tilesY;
    uint frame  = emitter.SheetFps > 0.0 ? uint(age * emitter.SheetFps) % frames
                                         : min(uint(clamp(t, 0.0, 1.0) * float(frames)), frames - 1u);
    vec2 uv     = vec2(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5);
    outUv       = (vec2(float(frame % tilesX), float(frame / tilesX)) + uv) / vec2(float(tilesX), float(tilesY));
    outColor    = emitter.StartColor * SampleGradient(emitter, t);
    outCorner   = corner;
}
