# HedgehogEngine Coding Conventions

> These conventions apply to all first-party source code under this repository.  
> Third-party code under `ThirdParty/` is excluded.

---

## 1. Language Standard

- Target **C++20** (currently enforced by the Premake5 build configuration).
- Avoid non-standard extensions unless absolutely necessary.

---

## 2. Naming Conventions

### 2.1 General Rules

| Entity | Convention | Example |
|--------|-----------|---------|
| Class / Struct | `PascalCase` | `EntityManager`, `DrawListContainer` |
| Function / Method | `PascalCase` | `CreateEntity()`, `GetMeshes()` |
| Local variable | `camelCase` | `entityCount`, `meshPath` |
| Function parameter | `camelCase` | `entity`, `inPath` |
| Namespace | `PascalCase` | `ECS`, `Scene`, `Context` |
| Enum | `PascalCase` | `LightType`, `MaterialType` |
| Enum value | `PascalCase` | `DirectionLight`, `PointLight` |
| `constexpr` / macro constant | `UPPER_SNAKE_CASE` | `MAX_ENTITIES`, `MAX_LIGHTS_COUNT` |
| Static file-local variable | `s_PascalCase` | `s_BaseActorScript` |
| Static class variable | `s_PascalCase` | `s_DefaultMeshPath` |
| Public data member | `PascalCase` (no prefix) | `Width`, `MeshPath` |
| Private / protected data member | `m_PascalCase` | `m_EntityCount` |
| Boolean variable / member | reads as a predicate | `IsVisible`, `HasDepth`, `CastShadows` |

### 2.2 Data Members

The `m_` prefix marks **internal state**. Public data members carry no prefix:

- **`struct` = passive data**: all members public, no invariants — members are plain
  `PascalCase` with no prefix.
- **`class` = encapsulated state**: `private`/`protected` members use `m_PascalCase`;
  behavior is exposed through methods.
- The rare public data member of an otherwise-encapsulated class (e.g. a tool window's
  `Open` flag) also carries no prefix — visibility, not the `struct`/`class` keyword,
  decides the prefix.

```cpp
// Good — passive data struct: no prefix
struct TextureDesc
{
    uint32_t    Width  = 1;
    uint32_t    Height = 1;
    RHI::Format Format = RHI::Format::R8G8B8A8Srgb; // qualify when a member shadows its type name
};

// Good — encapsulated class: m_ on internal state
class EntityManager
{
public:
    Entity CreateEntity();

private:
    uint32_t m_EntityCount = 0;
};

// Bad — prefix noise on public data
struct TextureDesc
{
    uint32_t m_Width = 1;    // caller reads desc.m_Width — the prefix buys nothing
};
```

This applies equally to reflected component structs (`HH_BEGIN_COMPONENT`):

```cpp
HH_BEGIN_COMPONENT(TransformComponent)
    HH_PROP_NAMED(HM::Vector3, Position, "Position", HM::Vector3(0.0f, 0.0f, 0.0f), None)

    HM::Matrix4x4 ObjMatrix = HM::Matrix4x4(); // runtime-only, not serialised
HH_END_COMPONENT(TransformComponent)
```

---

## 3. File and Folder Structure

### 3.1 File Extensions

- Use `.hpp` for C++ header files.
- Use `.cpp` for C++ source files.
- Do not add `.h` files to new code. Existing `.h` files in the `ECS/` module are legacy forwarding headers that simply `#include` their `.hpp` counterpart.

### 3.2 Header Guards

Use `#pragma once` on every header. No traditional `#ifndef` guards.

### 3.3 One Class Per File

Each class lives in its own `.hpp` / `.cpp` pair named after the class:

```
EntityManager.hpp / EntityManager.cpp
TransformSystem.hpp / TransformSystem.cpp
```

### 3.4 Directory Layout

```
Module/
├── ClassName.hpp
├── ClassName.cpp
└── SubGroup/
    ├── SubClass.hpp
    └── SubClass.cpp
```

---

## 4. Class Design

### 4.1 RAII

Prefer RAII for all resource ownership. Allocate in constructors (or `Init()`), release in destructors.

### 4.2 Rule of Zero / Rule of Five

- Prefer **Rule of Zero**: let compiler-generated special members do the work.
- When a class manages raw resources, explicitly define or `= delete` all five special members (copy constructor, copy assignment, move constructor, move assignment, destructor).
- Non-copyable, non-movable classes must `= delete` the corresponding operations:

```cpp
class Context
{
public:
    Context(const Context&)            = delete;
    Context(Context&&)                 = delete;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&)      = delete;
};
```

### 4.3 Access Specifiers

Order within a class: `public`, then `protected`, then `private`.

### 4.4 Inheritance

- Mark base-class destructors `virtual` when the class is intended for polymorphic use.
- Use `override` on all overriding methods; never use `virtual` on them again.

### 4.5 `struct` vs `class`

Use `struct` **only** for passive data carriers: all members public, no invariants, no
resource ownership. The moment a type needs private state, an invariant, or nontrivial
behavior, make it a `class`. (Same rule as the Google C++ Style Guide.) See §2.2 for the
member-naming consequences.

### 4.6 `explicit` Constructors

Mark every single-argument constructor `explicit` unless implicit conversion is
deliberately intended (document it with a comment when it is):

```cpp
class VulkanBuffer
{
public:
    explicit VulkanBuffer(const BufferDesc& desc);
};
```

### 4.7 `noexcept` on Move Operations

Declare move constructors and move assignment operators `noexcept`. Without it,
`std::vector` and friends **copy** instead of move on reallocation — a silent
performance loss in per-frame containers:

```cpp
VulkanTexture(VulkanTexture&& other) noexcept;
VulkanTexture& operator=(VulkanTexture&& other) noexcept;
```

---

## 5. Constants and Macros

### 5.1 Prefer `constexpr` Over `#define`

```cpp
// Good
inline constexpr size_t   MAX_ENTITIES   = 512;
inline constexpr uint32_t MAX_LIGHTS_COUNT = 16;

// Bad
#define MAX_ENTITIES 512
#define MAX_LIGHTS_COUNT 16
```

Use `inline constexpr` so the constant can be placed in a header without ODR issues.

### 5.2 When Macros Are Acceptable

- Platform-detection guards (`#ifdef _WIN32`).
- Include guards (but prefer `#pragma once`).
- Debug-break utilities (`ENGINE_DEBUG_BREAK`).

---

## 6. Memory Management

### 6.1 Smart Pointers

Prefer smart pointers over raw owning pointers:

| Situation | Pointer type |
|-----------|-------------|
| Sole ownership | `std::unique_ptr<T>` |
| Shared ownership | `std::shared_ptr<T>` |
| Non-owning observer | raw `T*` or `T&` |

### 6.2 Raw Pointers

Use raw pointers **only** for non-owning references to existing objects. Never call `delete` on a raw pointer in new code.

---

## 7. Const-Correctness

- Mark every method that does not mutate observable state `const`.
- Mark every parameter that is not modified `const` (references and pointers).
- Prefer `std::string_view` for read-only string parameters that are only inspected;
  keep `const std::string&` (or `const char*`) when the callee needs a null-terminated
  string (Lua, file APIs, C interfaces) or stores the value as `std::string` anyway.
- Never pass `std::string` by value for read-only use.
- Mark read-only local variables `const`.

```cpp
// Good
std::string Scene::GetSceneName() const { return m_SceneName; }
bool HasExtension(std::string_view path);      // only inspected
void LoadScript(const std::string& path);      // handed to Lua — needs null termination

// Bad
std::string Scene::GetSceneName() { return m_SceneName; }
void Foo(std::string path);
```

---

## 8. Type Safety

### 8.1 Enumerations

Always use `enum class` instead of plain `enum`:

```cpp
// Good
enum class LightType { DirectionLight = 0, PointLight = 1, SpotLight = 2 };

// Bad
enum LightType { DirectionLight, PointLight, SpotLight };
```

### 8.2 Casts

Prefer C++ named casts over C-style casts:

```cpp
// Good
static_cast<float>(lua_tonumber(L, -1));
static_cast<size_t>(MAX_LIGHTS_COUNT);

// Bad
(float)lua_tonumber(L, -1);
```

### 8.4 `auto`

Use `auto` when the type is obvious from the same line or is an implementation detail;
spell the type out otherwise:

```cpp
// Good — type is visible or irrelevant
auto texture = std::make_unique<VulkanTexture>(desc);
for (const auto& [entity, component] : m_Components) { ... }
auto it = m_Lookup.find(name);

// Bad — reader has to guess
auto result = ProcessFrame();   // what is result?
```

### 8.3 Type Maps / Type Dispatch

Use `std::type_index` (from `<typeindex>`) as map keys when keying on a runtime type. Never use `typeid(T).name()` as a `const char*` map key — pointer equality is not guaranteed:

```cpp
// Good
std::unordered_map<std::type_index, ComponentType> m_ComponentTypes;
const std::type_index typeId = typeid(T);
m_ComponentTypes.insert({ typeId, m_NextComponentType });

// Bad — undefined behavior
std::unordered_map<const char*, ComponentType> componentTypes;
componentTypes.insert({ typeid(T).name(), nextComponentType });
```

---

## 9. Error Handling

### 9.1 Assertions

Use `assert()` (from `<cassert>`) to enforce invariants and preconditions in **debug builds**. Write descriptive messages:

```cpp
assert(entity < MAX_ENTITIES && "Entity out of range.");
assert(m_EntityCount < MAX_ENTITIES && "Too many entities.");
```

### 9.2 Logging

Use the `LOGERROR`, `LOGWARNING`, `LOGINFO`, `LOGVERBOSE` helpers from `Logger/Logger.hpp` for runtime diagnostics:

```cpp
LOGERROR("Wrong file path: ", meshComponent.m_MeshPath);
LOGWARNING("Too many light components. Some will not be processed!");
```

### 9.3 Exceptions and Failure Returns

**Exceptions must not cross module (DLL) boundaries.** Fallible operations — file IO, asset
parsing/decoding, serialization — return `std::optional<T>` or `bool`, and log the failure with
`LOGERROR` at the point of failure (see `FS::FileSystemManager`, `ContentLoader::LoadMesh`).
Callers handle the failure path gracefully: substitute a placeholder resource, skip the item, or
propagate the failure return — never crash on bad content.

Mark fallible returns `[[nodiscard]]` so silently ignoring a failure is a compiler warning:

```cpp
[[nodiscard]] std::optional<LoadedMesh> LoadMesh(const std::string& path);
[[nodiscard]] bool SaveScene(const std::string& path);
```

Exceptions remain acceptable only *inside* a module for truly exceptional conditions that are
caught before reaching the module boundary. Internal invariant violations use `assert`.

---

## 10. Formatting

> The repository root contains a **`.clang-format`** file — it is the authority on
> formatting. Format code you touch with clang-format (most IDEs: format-on-save or
> format-selection). The rules below summarize it for human readers; if prose and
> `.clang-format` ever disagree, `.clang-format` wins.

### 10.1 Indentation

- 4 spaces per level. No tabs.

### 10.2 Braces

Opening brace on its **own line** (Allman style) for all constructs:

```cpp
void Foo()
{
    if (condition)
    {
        // ...
    }
}
```

Always brace the body of `if`/`else`/`for`/`while` — even a single statement:

```cpp
// Good
if (ready)
{
    Submit();
}

// Bad — unbraced body
if (ready)
    Submit();
```

### 10.3 Line Length

Keep lines under **120 characters** where practical.

### 10.4 Blank Lines

- One blank line between top-level definitions.
- No trailing blank lines inside function bodies.
- No more than one consecutive blank line anywhere.

### 10.5 `#include` Order

1. Corresponding header (for `.cpp` files).
2. Other project headers.
3. Third-party library headers.
4. Standard library headers.

Separate each group with a blank line.

```cpp
#include "ScriptSystem.hpp"      // own header first

#include "LuaHelpers.hpp"        // project headers
#include "Logger/Logger.hpp"

#include "yaml-cpp/yaml.h"       // third-party

#include <filesystem>            // standard library
#include <vector>
```

---

## 11. Namespaces

### 11.1 Project Namespaces

The existing per-module namespaces below are **frozen as canonical** — a deliberate mix of long
names and short abbreviations; do not mass-rename either direction. A new module picks one
unique PascalCase namespace (a short 2–4 letter abbreviation like `HM`/`HW`/`FS` is fine) and
adds it to this table in the same PR.

| Module | Namespace |
|--------|----------|
| ECS | `ECS` |
| EcsSerialization | `EcsSerialization` (reflection headers: `Reflection`) |
| HedgehogMath | `HM` |
| HedgehogCommon | `HedgehogEngine` |
| HedgehogEngine | `HedgehogEngine` |
| HedgehogRenderer | `Renderer` (legacy `HR` in ResourceRegistry — converge on `Renderer` when touching those files) |
| RHI | `RHI` |
| HedgehogWindow | `HW` |
| HedgehogSettings | `HedgehogSettings` |
| FileSystem | `FS` |
| Logger | `EngineLogger` |
| ContentLoader | `ContentLoader` |
| DialogueWindows | `DialogueWindows` |
| Editor | `Editor` |

### 11.2 Anonymous Namespaces

Use anonymous namespaces instead of `static` for file-local helpers in `.cpp` files:

```cpp
namespace
{
    bool IsFileAccessible(const std::string& path)
    {
        return std::filesystem::exists(ContentLoader::GetAssetsDirectory() + path);
    }
}
```

---

## 12. Standard Library Usage

- Prefer `std::vector`, `std::unordered_map`, `std::array` over custom containers.
- Prefer `std::optional` for nullable values.
- Prefer `std::variant` for type-safe unions.
- Prefer `std::filesystem::path` for path manipulation; use `/` as the path separator in string literals.
- Use `std::sqrt` (from `<cmath>`) instead of the C `sqrtf`.
- Use `std::cos`, `std::sin`, etc. instead of C equivalents.

---

## 13. ECS Architecture Notes

### 13.1 Component Registration

All components **must** be registered with the ECS coordinator before use. Registrations happen in `Scene::InitScene()`.

### 13.2 System Signatures

Each system declares which components it requires via a `Signature`. Systems only receive entities that have all required components.

### 13.3 System Entity Access

Inside a `System`-derived class, access the tracked entity list via the protected `m_Entities` member.  
External code that needs the entity list should call the public `GetEntities()` method.

```cpp
// Inside a system
for (auto const& entity : m_Entities) { ... }

// From outside
auto& entities = someSystem->GetEntities();
```

### 13.4 Constants

`MAX_ENTITIES` and `MAX_COMPONENTS` are `inline constexpr` values defined in `ECS/Entity.hpp`.

---

## 14. Logging Practices

```cpp
LOGINFO("Initializing scene.");
LOGVERBOSE("Entity created: ", entity);
LOGWARNING("Too many lights — excess will be ignored.");
LOGERROR("Failed to load file: ", path);
```

- Log at the point of failure, not at callers.
- Include relevant data in the message (path, ID, count, etc.).
- Prefer one log call per event; avoid fragmenting a message across multiple calls.

---

## 15. Performance Considerations

- Avoid unnecessary heap allocations inside per-frame code paths.
- Reserve `std::vector` capacity when the size is known in advance.
- Prefer range-based `for` loops with `const auto&` to avoid copies.
- Pass large objects by `const&`; return by value and rely on NRVO/move semantics.
- Component arrays in `ComponentArray<T>` are densely packed `std::array` structures — iterate them, not the index maps.

---

## 16. Typo Policy

- Identifiers (variable names, function names, type names) must be spelled correctly.
- Fix typos during any refactor of the containing file; do not introduce new ones.
- Known fixed typos (historical reference):
  - `m_Intencity` → `m_Intensity`
  - `mMeshPathes` / `mMaterialPathes` → `m_MeshPaths` / `m_MaterialPaths`
  - `UpdataSystem` → `UpdateSystem`
  - `UpdateShadowCastin` → `UpdateShadowCasting`
  - `precentModes` → `presentModes`
  - `InitializeDebugMessanger` → `InitializeDebugMessenger`
  - `HasGflwRequiredInstanceExtensions` → `HasGlfwRequiredInstanceExtensions`
