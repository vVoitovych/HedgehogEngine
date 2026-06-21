# /plan — Feature Planning Agent

You are coordinating a feature planning session for HedgehogEngine, a Vulkan-based C++20 game engine.

**Ticket:** $ARGUMENTS

---

## Step 1 — Clarification (interactive, do this FIRST)

Before writing any plan, scan the ticket for ambiguities. If any exist, ask the user up to 3 focused questions. Wait for answers before continuing.

Typical things to clarify:
- Which modules are in scope? (see CLAUDE.md for dependency graph)
- Public API/interface change or internal-only?
- New feature, refactor, or bug fix?
- Performance or frame-budget constraints?
- Does this touch the render graph, ECS, or serialization?

If the ticket is fully clear, skip this step and say: *"Ticket is unambiguous — proceeding to plan."*

---

## Step 2 — Architecture Analysis (spawn Opus agent)

Once all clarifications are resolved, call the Agent tool with **`model: "opus"`** and give it the full prompt below. The Opus agent has access to all tools.

**Opus agent prompt:**

```
You are a senior C++ game engine architect planning a feature for HedgehogEngine.

Ticket: <paste full ticket + any clarification answers>

Tasks:
1. Read CLAUDE.md for module architecture and dependency rules.
2. Read CODING_CONVENTIONS.md for code style constraints.
3. Search/read the relevant source files to understand the current state.
4. Produce a complete implementation plan in the format below.
5. Save the plan to workflow/current-plan.md (overwrite if exists).
6. Save a one-line ticket summary to workflow/current-ticket.md.

--- PLAN FORMAT (save exactly this structure to workflow/current-plan.md) ---

# Plan: <short title>
Date: <today YYYY-MM-DD>
Status: PENDING_APPROVAL

## Ticket
<full ticket text>

## Summary
<2-3 sentence executive summary of the approach>

## Files to Modify
- `path/to/file.hpp` — <what changes and why>
- `path/to/file.cpp` — <what changes and why>

## Files to Create
- `path/to/new-file.hpp` — <purpose>
(or: None)

## Implementation Steps
1. [ ] <atomic step> — touches: <file list>
2. [ ] <atomic step> — touches: <file list>
...

## Interface / API Changes
<List header or public API changes. "None" if purely internal.>

## Risks & Dependencies
- <risk, constraint, or ordering dependency>

## Test Requirements
<What new tests are needed and where. "None" if no logic change.>

## Estimated Complexity
Low | Medium | High — <one sentence justification>
```

---

## Step 3 — Present and confirm

After the Opus agent completes, read `workflow/current-plan.md` and display it to the user.

Then ask: *"Does this plan look good? Reply 'approved' to queue it for implementation, or describe what to change."*

If the user requests changes, re-run the Opus agent with the feedback and regenerate the plan. Repeat until approved.

When approved, update the Status line in `workflow/current-plan.md` from `PENDING_APPROVAL` to `APPROVED`.
