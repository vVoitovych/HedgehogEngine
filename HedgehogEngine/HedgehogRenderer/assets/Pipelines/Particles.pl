# The Particles pass (ParticlesPass.cpp): each emitter instance's alive particles as camera-facing
# billboards, drawn indirectly from its slot row.
# Set 0: the particle set the shared phase's simulation made (ParticleCompute.pl's layout; the
# vertex shader reads the states, the alive list or the sort list, and the emitters).
# Set 1: the emitter's texture (the resource registry's UI texture sets; a white pixel for none).
# Push constants: the view, the camera's right and up, the emitter and whether it has a texture
# (ParticlesPushConstants).
descriptor_sets:
  - bindings:
      - binding: 0
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 1
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 2
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 3
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 4
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 5
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 6
        type: storage_buffer
        stage: compute|vertex
        count: 1
      - binding: 7
        type: storage_buffer
        stage: compute|vertex
        count: 1
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment
        count: 1

push_constants:
  - stage: vertex|fragment
    offset: 0
    size: 112
