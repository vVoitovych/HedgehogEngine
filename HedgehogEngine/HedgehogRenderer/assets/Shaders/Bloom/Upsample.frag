#version 450

// The bloom chain's upsample (the Bloom pass): a 3 x 3 tent of the smaller level, added (the
// pipeline blends one to one) onto the level above.

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D source;

layout(push_constant) uniform Constants
{
    vec2 texel; // the source's texel size
} bloom;

layout(location = 0) out vec4 outColor;

void main()
{
    const vec2 t = bloom.texel;
    vec3 color = texture(source, inUv).rgb * 4.0f;
    color += (texture(source, inUv + vec2(-t.x, 0.0f)).rgb + texture(source, inUv + vec2(t.x, 0.0f)).rgb +
              texture(source, inUv + vec2(0.0f, -t.y)).rgb + texture(source, inUv + vec2(0.0f, t.y)).rgb) * 2.0f;
    color += texture(source, inUv + vec2(-t.x, -t.y)).rgb + texture(source, inUv + vec2(t.x, -t.y)).rgb +
             texture(source, inUv + vec2(-t.x, t.y)).rgb + texture(source, inUv + vec2(t.x, t.y)).rgb;
    outColor = vec4(color / 16.0f, 1.0f);
}
