# The editor's selection mask (SelectionMask pass): skinned overlay instances as solid white into an R8 target, with
# no depth test and no face culling, so the whole silhouette is marked even where the scene hides it.
pipeline_layout: ../Pipelines/DepthPrepassSkinned.pl
vertex_description: ../VertexDescriptions/SkinnedPositionOnly.vdes

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
    path: DepthPrepass/Skinned.vert.spv
  - stage: fragment
    path: SelectionMask/Mask.frag.spv
