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
    uint Spare[8];
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
    uint  SheetTilesX, SheetTilesY, EmitFromSurface, Padding1;
    vec4  SizeOverLife[8];
    vec4  SpeedOverLife[8];
    vec4  ColorOverLife[32];
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
layout(std430, set = 0, binding = 6) readonly buffer EmitterBuffer { EmitterData emitters[]; };

// Which emitter (an index into emitters) this dispatch works on; for Finish, how many emitters. The
// billboards (PARTICLE_DRAW) and the sort (PARTICLE_SORT) have push constants of their own.
#if !defined(PARTICLE_DRAW) && !defined(PARTICLE_SORT)
layout(push_constant) uniform Push
{
    uint EmitterIndex;
    uint EmitterCount;
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
