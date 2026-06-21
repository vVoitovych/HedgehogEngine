# /check-style — Code Style Agent

Check HedgehogEngine code against CODING_CONVENTIONS.md.

**Scope:** $ARGUMENTS
(Examples: `HedgehogEngine/src/ECS/`, `HedgehogRenderer/`, or leave blank for recently changed files.)

---

## Protocol

Spawn an Agent with **`model: "haiku"`** and give it this prompt:

```
You are a code style enforcer for HedgehogEngine, a Vulkan C++20 game engine.

Scope: <$ARGUMENTS, or "recently changed files from git diff HEAD">

If no scope is given, run: git diff --name-only HEAD to get the changed file list.

Step 1: Read CODING_CONVENTIONS.md in full. These rules are authoritative.
Step 2: Check each .hpp and .cpp file in scope against every rule.

Rules to check (at minimum):

NAMING
- Member variables must have m_ prefix
- Private methods must be camelCase, public methods PascalCase
- Constants must be constexpr, not #define (unless macro behavior is required)
- Type aliases use PascalCase
- Namespaces match the module name

STRUCTURE
- No #pragma warning(push/disable) — violations must be fixed at the source
- Include order: own header first, then project headers, then third-party, then std
- No raw owning pointers (use unique_ptr / shared_ptr)
- Single blank line between methods; no blank lines at start/end of a block

COMMENTS
- No "what" comments — only "why" comments for non-obvious behavior
- No commented-out code left in files

DLL BOUNDARY
- Public methods of DLL types must have the module's API macro (HEDGEHOG_ENGINE_API, etc.)
- Non-public methods must NOT have the macro

Report each violation as:
```
File: `path/file.hpp:line`
Rule: <rule name>
Found: `<offending snippet>`
Expected: `<corrected form>`
```

Group findings by file. End with totals: files checked, total violations.
Do NOT report bugs or logic issues — those belong in /find-bugs.
```

---

After the agent finishes, show all violations grouped by file. If there are more than 10 violations, offer to auto-fix them with `/implement`.

Note: Haiku is used here because style checking is pattern-matching that doesn't require deep reasoning.
