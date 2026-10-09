#version 450

// One triangle covering the viewport at the far plane (depth 1), from gl_VertexIndex alone, so the
// sky passes the depth test only where nothing was drawn.
layout(location = 0) out vec2 outNdc;

void main()
{
    const vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outNdc        = uv * 2.0f - 1.0f;
    gl_Position   = vec4(outNdc, 1.0f, 1.0f);
}
