# The render graph's forward pass (EnginePassTypes.cpp).
# Set 0: per view (camera matrices, eye position).
# Set 1: per material (uniform buffer + texture sampler); identical to ForwardPass.pl's set 1, so
#        the material sets the resource registry allocates bind to both pipelines.
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
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: fragment
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 64
