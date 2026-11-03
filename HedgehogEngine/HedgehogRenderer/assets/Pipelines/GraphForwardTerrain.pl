# The forward pass's terrain tiles (EnginePassTypes.cpp); GraphForward.pl but for set 1.
# Set 0: per view (camera matrices, eye position).
# Set 1: per terrain (HR::MakeTerrainSetBindings): the layers' values (HR::TerrainUniform), the
#        splat map, then the four layers' base colour maps and their four normal maps.
# Set 2: per frame, shared by every view (the light list the shared phase uploads).
# Set 3: per forward pass, the sun's shadow: the shadow uniform (Renderer::ShadowUniform) and the
#        shadow atlas, read through a comparison sampler (Common/Shadows.glsl); then the scene's
#        image-based lighting: the environment uniform (Renderer::EnvironmentUniform), the
#        prefiltered radiance cube and the split-sum BRDF table (Common/Pbr.glsl).
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
      - binding: 6
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 7
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 8
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 9
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

push_constants:
  - stage: vertex
    offset: 0
    size: 64
