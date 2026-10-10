#version 450

// A Cutoff material's depth: a fragment whose alpha is below the material's cutoff writes none.

layout(location = 0) in vec2 fragTexCoord;

#define MATERIAL_SET 2
#include "Common/Material.glsl"

void main()
{
    if (IsCutOut(MaterialBaseColor(fragTexCoord).a))
        discard;
}
