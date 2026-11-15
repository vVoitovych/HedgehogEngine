// The billboards' push constants (ParticlesPushConstants), shared by Billboard.vert and .frag.
layout(push_constant) uniform DrawPush
{
    mat4 ViewProj;
    vec4 CameraRight; // xyz: the view's right in the world
    vec4 CameraUp;    // xyz: the view's up in the world
    uint EmitterIndex;
    uint HasTexture;  // 0: a soft round dot instead of the texture
    uint Padding0;
    uint Padding1;
} draw;
