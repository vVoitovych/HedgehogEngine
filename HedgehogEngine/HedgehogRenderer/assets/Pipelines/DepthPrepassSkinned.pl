# DepthPrepass.pl for skinned meshes.
# Set 0: per view (viewProj), as DepthPrepass.pl.
# Set 1: the frame's joint palette (a storage buffer of matrices), the same layout as
#        GraphForwardSkinned.pl's set 3, so one palette set binds to both pipelines.
# Push constants: the model matrix and the instance's palette offset.
descriptor_sets:
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: vertex
        count: 1
  - bindings:
      - binding: 0
        type: storage_buffer
        stage: vertex
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 68
