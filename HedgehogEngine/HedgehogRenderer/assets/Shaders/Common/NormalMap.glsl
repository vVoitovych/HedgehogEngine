#ifndef NORMAL_MAP_GLSL
#define NORMAL_MAP_GLSL

// A tangent-space normal from a normal map's red and green alone, z rebuilt to unit length, so an
// RGB map and a two-channel BC5 one (whose blue reads 0) decode alike.
vec3 DecodeNormal(vec2 rg)
{
    const vec2 xy = rg * 2.0f - 1.0f;
    return vec3(xy, sqrt(clamp(1.0f - dot(xy, xy), 0.0f, 1.0f)));
}

#endif
