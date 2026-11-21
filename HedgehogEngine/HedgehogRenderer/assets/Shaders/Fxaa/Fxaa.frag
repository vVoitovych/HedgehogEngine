#version 450

// Fast approximate anti-aliasing (the Fxaa pass) over the tone-mapped, linear image: a pixel whose
// neighbourhood's luma contrast marks an edge is resampled across the edge, by how far along it the
// edge's ends lie (the published FXAA approach: local contrast, edge direction, a walk along the
// edge, and a sub-pixel blend for thin features). With FXAA off the pixel passes through.

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D source;

layout(push_constant) uniform Constants
{
    vec2 texelSize;
    uint enabled;
} fxaa;

layout(location = 0) out vec4 outColor;

const float EDGE_THRESHOLD_MIN = 0.0312f; // below this contrast nothing is an edge, however dark
const float EDGE_THRESHOLD     = 0.125f;  // the contrast, relative to the brightest neighbour, an edge needs
const float SUBPIXEL_AMOUNT    = 0.75f;
const int   SEARCH_STEPS       = 10;
const float SEARCH_STRIDE[SEARCH_STEPS] = float[](1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 2.0f, 2.0f, 4.0f, 8.0f);

// Perceptual luma: the image is linear, so square-rooted (a cheap gamma) before weighting.
float Luma(vec3 color)
{
    return dot(sqrt(max(color, vec3(0.0f))), vec3(0.299f, 0.587f, 0.114f));
}

float LumaAt(vec2 uv)
{
    return Luma(texture(source, uv).rgb);
}

void main()
{
    const vec4 center = texture(source, inUv);
    if (fxaa.enabled == 0u)
    {
        outColor = center;
        return;
    }
    const vec2 texel = fxaa.texelSize;

    const float lumaC = Luma(center.rgb);
    const float lumaN = LumaAt(inUv + vec2(0.0f, -texel.y));
    const float lumaS = LumaAt(inUv + vec2(0.0f, texel.y));
    const float lumaE = LumaAt(inUv + vec2(texel.x, 0.0f));
    const float lumaW = LumaAt(inUv + vec2(-texel.x, 0.0f));

    const float lumaMax = max(lumaC, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    const float lumaMin = min(lumaC, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    const float contrast = lumaMax - lumaMin;
    if (contrast < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD))
    {
        outColor = center;
        return;
    }

    const float lumaNW = LumaAt(inUv + vec2(-texel.x, -texel.y));
    const float lumaNE = LumaAt(inUv + vec2(texel.x, -texel.y));
    const float lumaSW = LumaAt(inUv + vec2(-texel.x, texel.y));
    const float lumaSE = LumaAt(inUv + vec2(texel.x, texel.y));

    // Which way the edge runs: compare the contrast across rows with that across columns.
    const float horizontal = abs(lumaNW - 2.0f * lumaW + lumaSW) + 2.0f * abs(lumaN - 2.0f * lumaC + lumaS) +
                             abs(lumaNE - 2.0f * lumaE + lumaSE);
    const float vertical = abs(lumaNW - 2.0f * lumaN + lumaNE) + 2.0f * abs(lumaW - 2.0f * lumaC + lumaE) +
                           abs(lumaSW - 2.0f * lumaS + lumaSE);
    const bool isHorizontal = vertical >= horizontal; // the edge runs along x: step across it in y

    // The side of the pixel the edge lies on: the neighbour across it with the steeper gradient.
    const float lumaBefore = isHorizontal ? lumaN : lumaW;
    const float lumaAfter  = isHorizontal ? lumaS : lumaE;
    const float gradBefore = abs(lumaBefore - lumaC);
    const float gradAfter  = abs(lumaAfter - lumaC);
    float       across     = isHorizontal ? texel.y : texel.x;
    float       lumaEdge;
    if (gradBefore >= gradAfter)
    {
        across   = -across;
        lumaEdge = 0.5f * (lumaBefore + lumaC);
    }
    else
        lumaEdge = 0.5f * (lumaAfter + lumaC);
    const float gradient = 0.25f * max(gradBefore, gradAfter);

    // Walk both ways along the edge, half a texel across it, until the luma leaves the edge's.
    vec2 edgeUv = inUv;
    if (isHorizontal)
        edgeUv.y += 0.5f * across;
    else
        edgeUv.x += 0.5f * across;
    const vec2 along = isHorizontal ? vec2(texel.x, 0.0f) : vec2(0.0f, texel.y);

    vec2  uvNeg   = edgeUv - along;
    vec2  uvPos   = edgeUv + along;
    float endNeg  = LumaAt(uvNeg) - lumaEdge;
    float endPos  = LumaAt(uvPos) - lumaEdge;
    bool  doneNeg = abs(endNeg) >= gradient;
    bool  donePos = abs(endPos) >= gradient;
    for (int i = 1; i < SEARCH_STEPS && !(doneNeg && donePos); ++i)
    {
        if (!doneNeg)
        {
            uvNeg -= along * SEARCH_STRIDE[i];
            endNeg  = LumaAt(uvNeg) - lumaEdge;
            doneNeg = abs(endNeg) >= gradient;
        }
        if (!donePos)
        {
            uvPos += along * SEARCH_STRIDE[i];
            endPos  = LumaAt(uvPos) - lumaEdge;
            donePos = abs(endPos) >= gradient;
        }
    }

    const float distNeg  = isHorizontal ? inUv.x - uvNeg.x : inUv.y - uvNeg.y;
    const float distPos  = isHorizontal ? uvPos.x - inUv.x : uvPos.y - inUv.y;
    const bool  nearNeg  = distNeg < distPos;
    const float nearest  = min(distNeg, distPos);
    const float span     = distNeg + distPos;
    // Only the end whose luma moves away from the centre's tells which way to blend.
    const bool  centerBelow = lumaC < lumaEdge;
    const bool  agrees      = ((nearNeg ? endNeg : endPos) < 0.0f) != centerBelow;
    float       offset      = agrees ? 0.5f - nearest / span : 0.0f;

    // Thin features shorter than a pixel: blend by how much the centre differs from its neighbourhood.
    const float average  = (2.0f * (lumaN + lumaS + lumaE + lumaW) + lumaNW + lumaNE + lumaSW + lumaSE) / 12.0f;
    const float subpixel = clamp(abs(average - lumaC) / contrast, 0.0f, 1.0f);
    const float ease     = (-2.0f * subpixel + 3.0f) * subpixel * subpixel;
    offset               = max(offset, ease * ease * SUBPIXEL_AMOUNT);

    vec2 uv = inUv;
    if (isHorizontal)
        uv.y += offset * across;
    else
        uv.x += offset * across;
    outColor = vec4(texture(source, uv).rgb, center.a);
}
