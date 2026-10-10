# DepthPrepassCutoff.pl for skinned meshes.
# Set 0: per view (viewProj). Set 1: the frame's joint palette, as DepthPrepassSkinned.pl's.
# Set 2: per material, identical to GraphForward.pl's set 1.
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
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: fragment
        count: 1
      - binding: 1
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 2
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 3
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 4
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 5
        type: combined_image_sampler
        stage: fragment
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 68
