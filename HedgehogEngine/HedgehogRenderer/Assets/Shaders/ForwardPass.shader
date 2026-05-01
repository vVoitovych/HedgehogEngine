pipeline_layout: ../Pipelines/ForwardPass.pl
vertex_description: ../VertexDescriptions/FullMesh.vdes

shaders:
  - stage: vertex
    path: ForwardPass/Base.vert.spv
  - stage: fragment
    path: ForwardPass/Base.frag.spv
