pipeline_layout: ../Pipelines/ShadowmapPass.pl
vertex_description: ../VertexDescriptions/PositionOnly.vdes

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
    path: ShadowmapPass/Shadowmap.vert.spv
