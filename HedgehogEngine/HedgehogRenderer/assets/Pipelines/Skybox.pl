# The Skybox pass (SkyboxPass.cpp): the environment's radiance cube behind everything.
# Set 0: the cube, sampled linear and clamped (GraphPassServices::AllocateSampledTexture; the same
#        layout as ToneMap.pl's set 0).
# Push constants: the inverse view-projection without translation, the environment's turn about +Z
#                 and its intensity (SkyboxPushConstants).
descriptor_sets:
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment
        count: 1

push_constants:
  - stage: fragment
    offset: 0
    size: 80
