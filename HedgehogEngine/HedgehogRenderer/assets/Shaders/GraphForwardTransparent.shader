# GraphForward.shader for Transparent materials: the same layout, vertex description and shaders,
# alpha-blended over what is behind (src_alpha, one_minus_src_alpha), depth tested against the
# prepass depth and not written (ForwardTransparent pass).
pipeline_layout: ../Pipelines/GraphForward.pl
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
  - enabled: true
    src_color: src_alpha
    dst_color: one_minus_src_alpha
    color_op: add
    src_alpha: one
    dst_alpha: one_minus_src_alpha
    alpha_op: add

shaders:
  - stage: vertex
    path: GraphForward/Base.vert.spv
  - stage: fragment
    path: GraphForward/Base.frag.spv
