# The post-process passes (FxaaPass.cpp, and those after it): fullscreen triangles over sampled
# textures. Sets 0 to 2: one texture each, sampled linear and clamped
# (GraphPassServices::AllocateSampledTexture, whose layout is ToneMap.pl's set, so the stages match).
# Push constants: 32 bytes, each pass's own (FxaaPushConstants).
descriptor_sets:
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment|compute
        count: 1
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment|compute
        count: 1
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment|compute
        count: 1

push_constants:
  - stage: fragment
    offset: 0
    size: 32
