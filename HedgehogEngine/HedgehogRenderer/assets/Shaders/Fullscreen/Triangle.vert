#version 450

// One triangle covering the viewport, from gl_VertexIndex alone (draw 3 vertices, no buffers).
// UV (0, 0) is the top-left of the target, as Vulkan's clip space has y down.
layout(location = 0) out vec2 outUv;

void main()
{
    outUv       = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUv * 2.0f - 1.0f, 0.0f, 1.0f);
}
