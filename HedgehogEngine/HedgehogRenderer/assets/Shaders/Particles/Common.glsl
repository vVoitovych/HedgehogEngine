// What every particle compute shader shares (ParticleCompute.pl's set 0): the pool's buffers, as
// ParticleResources lays them out, and the frame's emitters, ParticleEmitterUniform's std140 layout
// (identical under std430, which this storage buffer uses).

struct Particle
{
    vec4 PositionAge;      // xyz in the emitter's simulation space, w seconds lived
    vec4 VelocityLifetime; // xyz metres per second, w seconds it lives
    vec4 SizeRotation;     // x start size, y rotation (radians), z a random value in [0, 1), w unused
    vec4 Spare;
};

// One emitter instance's counters and indirect arguments (PARTICLE_SLOT_STRIDE, 64 bytes).
struct SlotRow
{
    int  Dead;       // entries of the dead list
    int  Alive;      // entries of this frame's alive list
    int  NewAlive;   // entries written to the next frame's alive list
    int  Dropped;    // spawns refused for want of dead particles
    uint DrawArgs[4]; // vertex count, instance count (the alive particles), first vertex, first instance
    int  Died;       // particles that died this frame: the top entries of the dead list after Simulate
    uint Spare[7];
};

struct EmitterData
{
    mat4  EmitMatrix;
    mat4  DrawMatrix;
    vec4  StartColor;
    vec3  Gravity;
    float Drag;
    vec3  BoxSize;
    float Radius;
    float LifetimeMin, LifetimeMax, SpeedMin, SpeedMax;
    float SizeMin, SizeMax, RotationMin, RotationMax;
    float ConeAngle, DeltaTime, SoftDistance, SheetFps;
    float PlaneHeight, Bounce, LifetimeLoss;
    uint  SlotRow;
    uint  SlotFirst, SlotCount, SpawnCount, Seed;
    uint  Shape, Blend, Space, Collision;
    uint  SheetTilesX, SheetTilesY, EmitFromSurface, DepthValid;
    vec4  SizeOverLife[8];
    vec4  SpeedOverLife[8];
    vec4  ColorOverLife[32];
    mat4  DepthViewProj;        // the collision depth's camera, the frame before
    mat4  DepthInverseViewProj;
};

// The billboards only read: a vertex shader may not write a storage buffer unless the device enables
// vertexPipelineStoresAndAtomics.
#ifdef PARTICLE_DRAW
#define PARTICLE_ACCESS readonly
#else
#define PARTICLE_ACCESS
#endif
layout(std430, set = 0, binding = 0) PARTICLE_ACCESS buffer ParticleBuffer { Particle particles[]; };
layout(std430, set = 0, binding = 1) PARTICLE_ACCESS buffer DeadListBuffer { uint deadList[]; };
layout(std430, set = 0, binding = 2) PARTICLE_ACCESS buffer AliveInBuffer { uint aliveIn[]; };
layout(std430, set = 0, binding = 3) PARTICLE_ACCESS buffer AliveOutBuffer { uint aliveOut[]; };
layout(std430, set = 0, binding = 4) PARTICLE_ACCESS buffer SlotBuffer { SlotRow slots[]; };
// Per alpha emitter and view: (key, particle) pairs, the farthest first once sorted, at twice the
// slot's first particle, as many as SlotCount rounded up to a power of two.
layout(std430, set = 0, binding = 5) PARTICLE_ACCESS buffer SortBuffer { uvec2 sortList[]; };
// The shadow view's depth as the frame before saw it, COLLISION_DEPTH_SIZE squared, row 0 the top.
layout(std430, set = 0, binding = 6) PARTICLE_ACCESS buffer CollisionDepthBuffer { float collisionDepth[]; };
layout(std430, set = 0, binding = 7) readonly buffer EmitterBuffer { EmitterData emitters[]; };

const uint COLLISION_DEPTH_SIZE = 512; // PARTICLE_COLLISION_DEPTH_SIZE

// ParticleCollisionMode
const uint COLLISION_HEIGHT_PLANE = 1u;
const uint COLLISION_DEPTH_BUFFER = 2u;

// Which emitter (an index into emitters) this dispatch works on; for Finish, how many emitters. The
// billboards (PARTICLE_DRAW) and the sort (PARTICLE_SORT) have push constants of their own.
#if !defined(PARTICLE_DRAW) && !defined(PARTICLE_SORT)
layout(push_constant) uniform Push
{
    uint EmitterIndex;
    uint EmitterCount;
    uint ChildIndex; // SpawnFromDeaths: the sub-emitter, and its particles per death
    uint ChildCount;
} push;
#endif

const uint GROUP_SIZE = 64;

// PCG hash: a well-mixed 32-bit value of the input.
uint Hash(uint value)
{
    uint state = value * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// A random number in [0, 1) from state, which moves on.
float Random(inout uint state)
{
    state = Hash(state);
    return float(state >> 8) / 16777216.0;
}

// A baked curve row at t in [0, 1], linear between its 32 samples.
float SampleCurve(vec4 row[8], float t)
{
    float x = clamp(t, 0.0, 1.0) * 31.0;
    uint  a = uint(floor(x));
    uint  b = min(a + 1u, 31u);
    return mix(row[a >> 2][a & 3u], row[b >> 2][b & 3u], x - float(a));
}

const float PI = 3.14159265358979;

// A unit direction, uniformly on the sphere.
vec3 RandomDirection(inout uint state)
{
    float z   = Random(state) * 2.0 - 1.0;
    float phi = Random(state) * 2.0 * PI;
    float r   = sqrt(max(0.0, 1.0 - z * z));
    return vec3(r * cos(phi), r * sin(phi), z);
}

// A newborn of emitter: placed on its shape by EmitMatrix, with its start values, from state.
Particle SpawnParticle(EmitterData emitter, inout uint state)
{
    vec3 position;
    vec3 direction;
    if (emitter.Shape == 1u) // Sphere: inside it, or on it, moving outwards
    {
        direction    = RandomDirection(state);
        float radius = emitter.EmitFromSurface != 0u ? emitter.Radius : emitter.Radius * pow(Random(state), 1.0 / 3.0);
        position     = direction * radius;
    }
    else if (emitter.Shape == 2u) // Cone: from a disc, within ConeAngle of +Z
    {
        float r      = emitter.Radius * sqrt(Random(state));
        float phi    = Random(state) * 2.0 * PI;
        position     = vec3(r * cos(phi), r * sin(phi), 0.0);
        float cosMax = cos(emitter.ConeAngle);
        float z      = mix(cosMax, 1.0, Random(state));
        float theta  = Random(state) * 2.0 * PI;
        float s      = sqrt(max(0.0, 1.0 - z * z));
        direction    = vec3(s * cos(theta), s * sin(theta), z);
    }
    else if (emitter.Shape == 3u) // Box: inside it, or on a face, moving along +Z
    {
        vec3 u   = vec3(Random(state), Random(state), Random(state)) - 0.5;
        if (emitter.EmitFromSurface != 0u)
        {
            uint axis = uint(Random(state) * 3.0) % 3u;
            u[axis]   = u[axis] < 0.0 ? -0.5 : 0.5;
        }
        position  = u * emitter.BoxSize;
        direction = vec3(0.0, 0.0, 1.0);
    }
    else // Point
    {
        position  = vec3(0.0);
        direction = RandomDirection(state);
    }

    float speed = mix(emitter.SpeedMin, emitter.SpeedMax, Random(state));
    Particle particle;
    particle.PositionAge      = vec4((emitter.EmitMatrix * vec4(position, 1.0)).xyz, 0.0);
    particle.VelocityLifetime = vec4(normalize((emitter.EmitMatrix * vec4(direction, 0.0)).xyz) * speed,
                                     max(mix(emitter.LifetimeMin, emitter.LifetimeMax, Random(state)), 1e-4));
    particle.SizeRotation     = vec4(mix(emitter.SizeMin, emitter.SizeMax, Random(state)),
                                     mix(emitter.RotationMin, emitter.RotationMax, Random(state)), Random(state), 0.0);
    particle.Spare            = vec4(0.0);
    return particle;
}
