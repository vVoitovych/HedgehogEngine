pipeline_layout: ../Pipelines/Particles.pl

topology: triangle_list

rasterization:
  cull_mode: none
  fill_mode: solid

depth:
  test: true
  write: false
  compare: less_or_equal

blend:
  - enabled: true
    src_color: src_alpha
    dst_color: one
    color_op: add
    src_alpha: one
    dst_alpha: one_minus_src_alpha
    alpha_op: add

shaders:
  - stage: vertex
    path: Particles/Billboard.vert.spv
  - stage: fragment
    path: Particles/Billboard.frag.spv
