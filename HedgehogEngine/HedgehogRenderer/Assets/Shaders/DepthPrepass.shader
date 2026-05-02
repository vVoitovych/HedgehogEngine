pipeline_layout: ../Pipelines/DepthPrepass.pl
vertex_description: ../VertexDescriptions/PositionOnly.vdes

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
    path: DepthPrepass/Base.vert.spv
