pipeline_layout: ../Pipelines/Gizmo.pl
vertex_description: ../VertexDescriptions/PositionOnly.vdes

topology: line_list

rasterization:
  cull_mode: none
  fill_mode: solid

depth:
  test: true
  write: false
  compare: less_or_equal

blend:
  - enabled: false
    src_color: one
    dst_color: zero
    color_op: add
    src_alpha: one
    dst_alpha: zero
    alpha_op: add

shaders:
  - stage: vertex
    path: Gizmo/Box.vert.spv
  - stage: fragment
    path: Gizmo/Box.frag.spv
