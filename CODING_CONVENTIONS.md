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
| Static class variable | `s_PascalCase` | `sDefaultMeshPath` |

### 2.2 Member Variables

All member variables (private, protected, or public) use the `m_` prefix:

```cpp
// Good
class Foo
{
    int         m_Count = 0;
    std::string m_Name;
};

// Bad — inconsistent prefix
int mCount;      // missing underscore
int count;       // no prefix at all
```

This applies equally to data-only component structs:

```cpp
// Good
class TransformComponent
{
public:
    HM::Vector3  m_Position  = {};
    HM::Matrix4x4 m_ObjMatrix = {};
};
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
- Prefer `const std::string&` over `std::string` for read-only parameters.
- Mark read-only local variables `const`.

```cpp
// Good
std::string Scene::GetSceneName() const { return m_SceneName; }
void Foo(const std::string& path);

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

### 9.3 Exceptions

Throw exceptions only for truly exceptional external failures (e.g., invalid YAML, unknown enum value in a UI dispatch). Internal invariant violations should use `assert`.

---

## 10. Formatting

### 10.1 Indentation

- 4 spaces per level. No tabs.

### 10.2 Braces

Opening brace on the **same line** for all constructs:

```cpp
void Foo()
{
    if (condition)
    {
        // ...
    }
}
```

Actually: opening brace on its **own line** (Allman style), as seen throughout the existing codebase.

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

| Module | Namespace |
|--------|----------|
| ECS | `ECS` |
| Scene | `Scene` |
| HedgehogMath | `HM` |
| Context | `Context` |
| Renderer | `Renderer` |
| Wrappers | `Wrappers` |
| WindowManager | `WinManager` |
| Settings | `HedgehogSettings` |
| Logger | `EngineLogger` |
| ContentLoader | `ContentLoader` |
| DialogueWindows | `DialogueWindows` |

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
