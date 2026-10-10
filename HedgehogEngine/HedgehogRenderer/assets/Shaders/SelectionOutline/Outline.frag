#version 450

// The editor's selection outline (SelectionOutline pass): a pixel outside the selection's mask with a
// masked pixel within the outline's width turns orange, its edge softened by the distance; pixels
// inside the mask and away from it are left as they are.

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D selectionMask;

layout(push_constant) uniform Constants
{
    vec2  texelSize;
    float width;
    float unused;
} outline;

layout(location = 0) out vec4 outColor;

const vec3 OUTLINE_COLOR = vec3(1.0f, 0.6f, 0.1f);

void main()
{
    if (texture(selectionMask, inUv).r > 0.5f)
        discard; // the selected object itself shows as drawn

    const int reach   = int(ceil(outline.width));
    float     nearest = 0.0f;
    for (int y = -reach; y <= reach; ++y)
    {
        for (int x = -reach; x <= reach; ++x)
        {
            const float distance = length(vec2(x, y));
            const float weight   = clamp(outline.width + 0.5f - distance, 0.0f, 1.0f);
            if (weight > 0.0f)
                nearest = max(nearest, weight * texture(selectionMask, inUv + vec2(x, y) * outline.texelSize).r);
        }
    }
    if (nearest <= 0.0f)
        discard;
    outColor = vec4(OUTLINE_COLOR, nearest);
}
