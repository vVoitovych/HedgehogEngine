// Linear blend skinning. Define PALETTE_SET (the set the joint palette is bound at) before
// including. The palette holds every skinned instance's matrices back to back
// (RenderScene::JointMatrices); paletteOffset is where this instance's start.

layout(std430, set = PALETTE_SET, binding = 0) readonly buffer JointPalette
{
    mat4 joints[];
} palette;

mat4 SkinMatrix(uvec4 jointIndices, vec4 weights, uint paletteOffset)
{
    return weights.x * palette.joints[paletteOffset + jointIndices.x]
         + weights.y * palette.joints[paletteOffset + jointIndices.y]
         + weights.z * palette.joints[paletteOffset + jointIndices.z]
         + weights.w * palette.joints[paletteOffset + jointIndices.w];
}
