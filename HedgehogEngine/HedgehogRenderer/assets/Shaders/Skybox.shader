pipeline_layout: ../Pipelines/Skybox.pl

topology: triangle_list

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
    path: Skybox/Sky.vert.spv
  - stage: fragment
    path: Skybox/Sky.frag.spv
