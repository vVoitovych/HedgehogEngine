pipeline_layout: ../Pipelines/PostProcess.pl

topology: triangle_list

rasterization:
  cull_mode: none
  fill_mode: solid

depth:
  test: false
  write: false
  compare: always

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
    path: Fullscreen/Triangle.vert.spv
  - stage: fragment
    path: Bloom/Downsample.frag.spv
