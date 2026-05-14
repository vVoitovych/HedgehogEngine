# Set 0: per-frame data (camera matrices, light list).
# Set 1: per-material data (uniform buffer + texture sampler).
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

push_constants:
  - stage: vertex
    offset: 0
    size: 64
