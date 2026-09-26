descriptor_sets:
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: vertex
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 64
