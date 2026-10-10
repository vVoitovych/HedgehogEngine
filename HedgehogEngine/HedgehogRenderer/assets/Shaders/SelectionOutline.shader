# The editor's selection outline (SelectionOutline pass): one fullscreen triangle sampling the
# selection mask, blended over the Scene view's colour output, with no depth attachment.
pipeline_layout: ../Pipelines/ToneMap.pl

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
    src_alpha: zero
    dst_alpha: one
    alpha_op: add

shaders:
  - stage: vertex
    path: Fullscreen/Triangle.vert.spv
  - stage: fragment
    path: SelectionOutline/Outline.frag.spv
