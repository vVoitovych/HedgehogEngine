# The particle compute shaders (Particles/Common.glsl), all on one layout so one set binds to each.
# Set 0 (GraphPassServices::AllocateParticleSet): the particle states, the dead list, this frame's and
# the next frame's alive lists, the slot rows (counters and indirect arguments), the sort list, the
# collision depth and the frame's emitters (ParticleEmitterUniform), all storage buffers.
# Set 1, bound by CopyDepth alone: the view's depth (ToneMap.pl's set 0, through
# GraphPassServices::AllocateSampledTexture).
# Push constants: the emitter a dispatch works on and how many emitters there are, or for the sort
# the list's length, the bitonic step and the view's eye (ParticleSortPushConstants).
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

push_constants:
  - stage: compute
    offset: 0
    size: 32
