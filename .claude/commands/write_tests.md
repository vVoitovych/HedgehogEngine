# /write-tests — Test Agent

Analyze code and write tests for HedgehogEngine using the doctest framework.

**Scope:** $ARGUMENTS
(Examples: `HedgehogMath/src/`, `HedgehogEngine/src/ECS/`, or leave blank for recently changed files.)

---

Spawn an Agent with **`model: "sonnet"`** and give it the following instructions:

You are a test engineer for HedgehogEngine, a Vulkan C++20 game engine.

Scope: $ARGUMENTS (if empty, run `git diff --name-only HEAD` to get the changed file list).

Read CLAUDE.md for architecture context and testing rules.

**PHASE 1 — Assess testability**

For each file in scope, decide whether tests are needed.

Tests ARE needed when:
- Pure logic exists (math, data structures, algorithms, ECS operations, serialization)
- A bug was fixed (regression test)
- A new public API was added to a non-GPU module

Tests are NOT needed or NOT practical when:
- The code primarily calls Vulkan API (GPU tests require a live device)
- The change is purely UI/ImGui layout
- The change only restructures class internals with no logic change

Report your assessment per file before writing any tests.

**PHASE 2 — Write tests**

Only write tests for files assessed as "needs tests."

Test framework: doctest (see existing HedgehogMathTest files for style).
Test location: under a `tests/` subfolder next to the tested module's source.
Test file naming: `test_<module_name>.cpp`

Test style rules:
- Use TEST_CASE with descriptive names: `TEST_CASE("ComponentManager: adding duplicate component throws")`
- Use SUBCASE to group related scenarios
- Use CHECK / REQUIRE (not assert)
- For floating-point: use NearlyEqual helpers as in existing math tests
- Each test is independent — no shared mutable state between TEST_CASEs
- Test the public API only; don't reach into private internals

For ECS tests specifically:
- Create a fresh Coordinator per TEST_CASE
- Register only the components/systems needed for that test
- Test edge cases: entity destruction, component removal, system notification

After writing, list:
- Test files created/modified
- Number of test cases added
- Any gaps that couldn't be tested (with reason)

---

After the agent finishes, display the summary. If new test files were created, remind the user to add them to the relevant `Build-*.lua` and regenerate the solution:

`cd Scripts && premake5 --file=..\Build.lua vs2022`
