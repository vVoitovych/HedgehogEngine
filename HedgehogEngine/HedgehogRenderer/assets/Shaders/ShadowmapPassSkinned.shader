# ShadowmapPass.shader for skinned casters. The vertex work is the skinned depth prepass's
# (viewProj * model * skin * position), so its layout and vertex shader are shared.
pipeline_layout: ../Pipelines/DepthPrepassSkinned.pl
vertex_description: ../VertexDescriptions/SkinnedPositionOnly.vdes

topology: triangle_list

rasterization:
  cull_mode: back
  fill_mode: solid

depth:
  test: true
  write: true
  compare: less_or_equal

shaders:
  - stage: vertex
    path: DepthPrepass/Skinned.vert.spv
