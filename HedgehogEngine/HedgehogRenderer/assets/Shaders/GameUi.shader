pipeline_layout: ../Pipelines/GameUi.pl
vertex_description: ../VertexDescriptions/Ui.vdes

topology: triangle_list

rasterization:
  cull_mode: none
  fill_mode: solid

depth:
  test: false
  write: false
  compare: always

blend:
  - enabled: true
    src_color: src_alpha
    dst_color: one_minus_src_alpha
    color_op: add
    src_alpha: one
    dst_alpha: one_minus_src_alpha
    alpha_op: add

shaders:
  - stage: vertex
    path: GameUi/Quad.vert.spv
  - stage: fragment
    path: GameUi/Quad.frag.spv
