pipeline_layout: ../Pipelines/DepthPrepassCutoffSkinned.pl
vertex_description: ../VertexDescriptions/SkinnedPositionUv.vdes

topology: triangle_list

rasterization:
  cull_mode: back
  fill_mode: solid

depth:
  test: true
  write: true
  compare: less

shaders:
  - stage: vertex
    path: DepthPrepass/CutoffSkinned.vert.spv
  - stage: fragment
    path: DepthPrepass/CutoffSkinned.frag.spv
