pipeline_layout: ../Pipelines/ForwardPass.pl
vertex_description: ../VertexDescriptions/FullMesh.vdes

topology: triangle_list

rasterization:
  cull_mode: back
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
    path: ForwardPass/Base.vert.spv
  - stage: fragment
    path: ForwardPass/Base.frag.spv
