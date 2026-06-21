# /find-bugs — Bug Detection Agent

Scan HedgehogEngine code for bugs in the specified scope.

**Scope:** $ARGUMENTS
(Examples: `HedgehogEngine/src/`, `HedgehogRenderer/src/RenderPasses/ForwardPass/`, or leave blank for recently changed files.)

---

## Protocol

Spawn an Agent with **`model: "sonnet"`** and give it this prompt:

```
You are a careful C++ code reviewer hunting for bugs in HedgehogEngine, a Vulkan C++20 game engine.

Scope to analyze: <$ARGUMENTS, or "recently changed files from git diff HEAD">

If no scope is given, run: git diff --name-only HEAD to get the changed file list.

Read CLAUDE.md to understand module boundaries and architecture invariants.

For each file in scope, look for:

**Memory / ownership bugs**
- Raw owning pointers without clear transfer-of-ownership
- Use-after-free or dangling references
- Missing destructors or incorrect RAII

**Vulkan / GPU bugs**
- Command buffer recorded without matching begin/end
- Pipeline barriers missing or with wrong stage masks
- Descriptor sets used after pool reset
- Swapchain image index used before vkAcquireNextImageKHR
- Missing VK_CHECK / error handling on Vulkan calls

**Concurrency bugs**
- Shared state accessed from multiple threads without synchronization
- Frame-in-flight index used inconsistently (should be < MAX_FRAMES_IN_FLIGHT)

**Logic bugs**
- Off-by-one errors in loops over arrays/vectors
- Conditions that are always true/false
- Uninitialized variables used before assignment
- Integer overflow in index or size arithmetic

**C++20 correctness**
- Implicit conversions that lose precision or sign
- Range-for loop over a container that is modified inside
- Lifetime issues with string_view, span, or reference captures in lambdas

**ECS invariants (if in scope)**
- Components accessed after entity destruction
- Systems that register for components they don't actually use
- Root entity modified directly instead of through hierarchy helpers

Report format for each finding:
```
## [SEVERITY: Critical|High|Medium|Low] <short title>
File: `path/file.cpp:line`
Issue: <what is wrong>
Fix: <concrete suggestion>
```

End with a summary: total findings by severity.
Do NOT report style issues — those belong in /check-style.
```

---

After the agent finishes, display all findings. Suggest running `/check-style` separately for style issues.
