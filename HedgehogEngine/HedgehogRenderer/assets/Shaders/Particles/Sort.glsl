// The sort's push constants (ParticleSortPushConstants), shared by SortKeys.comp and SortStep.comp:
// the emitter sorted, its list's length (a power of two), the bitonic step and the view's eye.
layout(push_constant) uniform SortPush
{
    uint EmitterIndex;
    uint Count;
    uint StepJ; // the distance compared across
    uint StepK; // the length of the sequences being merged
    vec4 Eye;   // xyz: the view's position in the world
} sort;
