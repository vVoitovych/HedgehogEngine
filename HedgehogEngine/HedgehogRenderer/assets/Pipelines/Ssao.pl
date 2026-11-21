# The Ssao pass's occlusion draw (SsaoPass.cpp). Set 0: the view's depth, sampled linear and clamped
# (GraphPassServices::AllocateSampledTexture, whose layout is ToneMap.pl's set, so the stages match).
# Push constants: the projection's terms, radius, intensity, bias and the depth's texel size
# (SsaoPushConstants).
descriptor_sets:
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment|compute
        count: 1

push_constants:
  - stage: fragment
    offset: 0
    size: 64
