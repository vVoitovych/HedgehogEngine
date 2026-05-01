# Pipeline File Format (.pl)

Pipeline files configure the descriptor set layouts and push constant ranges that are loaded by `PipelineLoader::Load()` at renderer startup.

Files are YAML. The loader resolves paths relative to the repository root, so place new files here and reference them with:

```
/HedgehogEngine/HedgehogRenderer/Assets/Pipelines/YourPass.pl
```

---

## Top-level keys

| Key | Required | Description |
|-----|----------|-------------|
| `descriptor_sets` | no | List of descriptor sets. Position in the list = set index in the pipeline layout. |
| `push_constants` | no | List of push constant ranges. |

---

## `descriptor_sets`

Each entry is one set. Sets are bound in order (set 0, set 1, …).

```yaml
descriptor_sets:
  - bindings:          # set 0
      - binding: 0
        type: uniform_buffer
        stage: vertex
        count: 1
  - bindings:          # set 1
      - binding: 0
        type: combined_image_sampler
        stage: fragment
        count: 1
      - binding: 1
        type: uniform_buffer
        stage: "vertex | fragment"
        count: 1
```

### Binding fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `binding` | uint | yes | — | Slot number matching the shader `binding = N` annotation. |
| `type` | string | yes | — | Descriptor type (see table below). |
| `stage` | string | yes | — | Shader stage(s) that access this binding (see table below). |
| `count` | uint | no | `1` | Array size; use `1` for non-array descriptors. |

### Descriptor types

| Value | Vulkan equivalent |
|-------|-------------------|
| `uniform_buffer` | `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` |
| `storage_buffer` | `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` |
| `combined_image_sampler` | `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` |
| `storage_image` | `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` |
| `input_attachment` | `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT` |

### Shader stages

Stages can be combined with `|`. Surround the value with quotes when using `|`.

| Value | Vulkan equivalent |
|-------|-------------------|
| `vertex` | `VK_SHADER_STAGE_VERTEX_BIT` |
| `fragment` | `VK_SHADER_STAGE_FRAGMENT_BIT` |
| `compute` | `VK_SHADER_STAGE_COMPUTE_BIT` |
| `all` | `VK_SHADER_STAGE_ALL` |
| `"vertex \| fragment"` | vertex + fragment |

---

## `push_constants`

```yaml
push_constants:
  - stage: vertex
    offset: 0
    size: 64
```

### Push constant fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `stage` | string | yes | — | Shader stage(s) that read this range (same values as binding stages). |
| `offset` | uint | no | `0` | Byte offset into the push constant block. |
| `size` | uint | yes | — | Size in bytes of this range. |

Push constant ranges must not overlap. Typical usage: one 64-byte range for a `mat4` model matrix in the vertex stage.

---

## Complete example

```yaml
# Set 0: per-frame camera + lights uniform (vertex and fragment).
# Set 1 is owned externally (e.g. ResourceRegistry) and combined in C++.
descriptor_sets:
  - bindings:
      - binding: 0
        type: uniform_buffer
        stage: "vertex | fragment"
        count: 1

push_constants:
  - stage: vertex
    offset: 0
    size: 64
```

---

## Notes

- Descriptor sets not listed here (e.g. a material set owned by `ResourceRegistry`) must be combined with the loaded layouts manually in C++ when building the `GraphicsPipelineDesc`.
- `MakePoolSizes()` derives `DescriptorPool` allocation sizes automatically from the binding list; no separate pool configuration is needed.
- The file is parsed once during pass construction. Hot-reload is not supported.
