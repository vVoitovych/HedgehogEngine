pipeline_layout: ../Pipelines/DepthPrepassSkinned.pl
vertex_description: ../VertexDescriptions/SkinnedPositionOnly.vdes

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
    path: DepthPrepass/Skinned.vert.spv
