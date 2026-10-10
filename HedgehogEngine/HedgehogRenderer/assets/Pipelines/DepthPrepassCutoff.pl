# DepthPrepass.pl for Cutoff (alpha-tested) materials.
# Set 0: per view (viewProj), as DepthPrepass.pl.
# Set 1: per material, identical to GraphForward.pl's set 1, so a material set binds to both.
# Push constants: the model matrix.
descriptor_sets:
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: vertex
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

push_constants:
  - stage: vertex
    offset: 0
    size: 64
