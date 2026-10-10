# ShadowmapPass.shader for Cutoff casters: the depth prepass's alpha-tested layout, vertex description
# and shaders (DepthPrepassCutoff.shader), with the shadow pass's depth test.
pipeline_layout: ../Pipelines/DepthPrepassCutoff.pl
vertex_description: ../VertexDescriptions/PositionUv.vdes

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
    path: DepthPrepass/Cutoff.vert.spv
  - stage: fragment
    path: DepthPrepass/Cutoff.frag.spv
