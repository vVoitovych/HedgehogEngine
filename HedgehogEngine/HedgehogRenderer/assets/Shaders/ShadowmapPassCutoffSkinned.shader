# ShadowmapPass.shader for skinned Cutoff casters: the depth prepass's alpha-tested layout, vertex description
# and shaders (DepthPrepassCutoffSkinned.shader), with the shadow pass's depth test.
pipeline_layout: ../Pipelines/DepthPrepassCutoffSkinned.pl
vertex_description: ../VertexDescriptions/SkinnedPositionUv.vdes

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
    path: DepthPrepass/CutoffSkinned.vert.spv
  - stage: fragment
    path: DepthPrepass/CutoffSkinned.frag.spv
