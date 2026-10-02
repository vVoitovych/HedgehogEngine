# The GameUi pass (GameUiPass.cpp): HX::UiDrawList quads over the game view.
# Set 0: the draw command's texture (a white pixel for a solid fill), owned by the resource registry.
# Push constants: the pixel-to-clip-space scale and offset (GameUiPushConstants).
descriptor_sets:
  - bindings:
      - binding: 0
        type: combined_image_sampler
        stage: fragment
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 16
