# GraphForward.pl for skinned meshes: sets 0 to 3 are GraphForward.pl's, so the same view,
# material, light and shadow (with environment) sets bind to both pipelines.
# Set 4: the frame's joint palette (a storage buffer of matrices).
# Push constants: the model matrix and the instance's palette offset.
descriptor_sets:
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: "vertex | fragment"
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
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: fragment
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
        type: uniform_buffer
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
  - bindings:
      - binding: 0
        type: storage_buffer
        stage: vertex
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 68
