# Set 0: per-frame data (camera matrices, light list).
# Set 1 (material: UBO + sampler) is owned by ResourceRegistry and
# combined with this layout in C++ when building the pipeline layout.
descriptor_sets:
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: "vertex | fragment"
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 64
