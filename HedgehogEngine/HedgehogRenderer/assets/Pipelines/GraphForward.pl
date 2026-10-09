# The render graph's forward pass (EnginePassTypes.cpp).
# Set 0: per view (camera matrices, eye position).
# Set 1: per material: the factors (MaterialUniform), then the base colour, normal,
#        metallic-roughness, occlusion and emissive maps (ResourceRegistry's MaterialTextureBinding);
#        identical to GraphForwardSkinned.pl's set 1, so a material set binds to both pipelines.
# Set 2: per frame, shared by every view (the light list the shared phase uploads).
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

push_constants:
  - stage: vertex
    offset: 0
    size: 64
