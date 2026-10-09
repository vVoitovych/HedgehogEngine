# The ToneMap pass (ToneMapPass.cpp): the view's HDR radiance onto its LDR colour output.
# Set 0: the HDR texture, sampled linear and clamped (GraphPassServices::AllocateSampledTexture).
# Push constants: the exposure scale, 2^EV (ToneMapPushConstants).
descriptor_sets:
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment
        count: 1

push_constants:
  - stage: fragment
    offset: 0
    size: 16
