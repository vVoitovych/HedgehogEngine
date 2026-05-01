# Vertex Description File Format (.vdes)

Vertex description files configure the `VertexBinding` and `VertexAttribute` arrays loaded by `VertexDescLoader::Load()` at renderer startup.

Files are YAML. The loader resolves paths relative to the repository root, so place new files here and reference them with:

```
/HedgehogEngine/HedgehogRenderer/Assets/VertexDescriptions/YourDesc.vdes
```

---

## Top-level keys

| Key | Required | Description |
|-----|----------|-------------|
| `bindings` | no | List of vertex buffer bindings (one per buffer slot). |
| `attributes` | no | List of vertex attributes (one per shader input). |

---

## `bindings`

Each entry maps to one vertex buffer slot.

```yaml
bindings:
  - binding: 0
    stride: 12
    input_rate: per_vertex
  - binding: 1
    stride: 8
    input_rate: per_vertex
```

### Binding fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `binding` | uint | yes | — | Buffer slot index. Must match the index used in `BindVertexBuffers`. |
| `stride` | uint | yes | — | Byte distance between consecutive elements in the buffer (e.g. `12` for `vec3`, `8` for `vec2`). |
| `input_rate` | string | no | `per_vertex` | How the buffer advances (see table below). |

### Input rates

| Value | Vulkan equivalent |
|-------|-------------------|
| `per_vertex` | `VK_VERTEX_INPUT_RATE_VERTEX` |
| `per_instance` | `VK_VERTEX_INPUT_RATE_INSTANCE` |

---

## `attributes`

Each entry maps to one `location` in the vertex shader.

```yaml
attributes:
  - location: 0
    binding: 0
    format: r32g32b32_float
    offset: 0
  - location: 1
    binding: 1
    format: r32g32_float
    offset: 0
```

### Attribute fields

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `location` | uint | yes | — | Shader input location (`layout(location = N)`). |
| `binding` | uint | yes | — | Which binding slot this attribute reads from. |
| `format` | string | yes | — | Element format (see table below). |
| `offset` | uint | no | `0` | Byte offset of this attribute within one element of the binding's stride. |

### Formats

| Value | C++ equivalent | Typical use |
|-------|----------------|-------------|
| `r32_float` | `Format::R32Float` | `float` |
| `r32g32_float` | `Format::R32G32Float` | `vec2` (UV coordinates) |
| `r32g32b32_float` | `Format::R32G32B32Float` | `vec3` (positions, normals) |
| `r32g32b32a32_float` | `Format::R32G32B32A32Float` | `vec4` (colors, tangents with sign) |

---

## Complete examples

### Position-only (depth pre-pass, shadow map pass)

```yaml
bindings:
  - binding: 0
    stride: 12
    input_rate: per_vertex

attributes:
  - location: 0
    binding: 0
    format: r32g32b32_float
    offset: 0
```

### Full mesh (forward pass)

Three separate buffer streams: positions, texture coordinates, normals.

```yaml
bindings:
  - binding: 0
    stride: 12
    input_rate: per_vertex
  - binding: 1
    stride: 8
    input_rate: per_vertex
  - binding: 2
    stride: 12
    input_rate: per_vertex

attributes:
  - location: 0
    binding: 0
    format: r32g32b32_float
    offset: 0
  - location: 1
    binding: 1
    format: r32g32_float
    offset: 0
  - location: 2
    binding: 2
    format: r32g32b32_float
    offset: 0
```

### Interleaved layout (position + UV in one buffer)

```yaml
bindings:
  - binding: 0
    stride: 20       # 12 (vec3) + 8 (vec2)
    input_rate: per_vertex

attributes:
  - location: 0
    binding: 0
    format: r32g32b32_float
    offset: 0
  - location: 1
    binding: 0
    format: r32g32_float
    offset: 12
```

---

## Notes

- The number of `bindings` entries determines how many buffer slots are bound via `BindVertexBuffers`. Ensure the C++ call supplies exactly that many buffers.
- `binding` values in `attributes` must reference a slot declared in `bindings`.
- `location` values must match `layout(location = N) in` declarations in the vertex shader.
- The file is parsed once during pass construction. Hot-reload is not supported.
