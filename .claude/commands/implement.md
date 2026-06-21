# /implement — Implementation Agent

You are coordinating implementation of an approved plan for HedgehogEngine.

**Arguments:** $ARGUMENTS
(Pass `--continue` to resume after a context/token-limit reset.)

---

## Pre-flight checks

1. Read `workflow/current-plan.md`. If it is missing or Status is not `APPROVED`, stop and tell the user: *"No approved plan found. Run /plan first."*
2. Read `workflow/progress.md` if it exists to see completed steps.
3. If `$ARGUMENTS` contains `--continue`, resume from the first unchecked step in progress.md. Otherwise start from Step 1 of the plan.

---

## Execution protocol

Spawn an Agent with **`model: "sonnet"`** and give it this prompt:

```
You are implementing an approved plan for HedgehogEngine, a Vulkan C++20 game engine.

Read these files before doing anything:
- CLAUDE.md (architecture rules, dependency order)
- CODING_CONVENTIONS.md (all code must comply — no exceptions)
- workflow/current-plan.md (the full plan)
- workflow/progress.md (completed steps so far, if it exists)

Work through the Implementation Steps in order, starting from the first unchecked [ ] step.

For EACH step:
1. Implement the change across all listed files.
2. After completing the step, immediately update workflow/progress.md:
   - Mark the step as [x] done
   - Add a brief note: what you did and any surprises
3. If you hit an ambiguity or blocker, write it to workflow/progress.md under "## Blockers" and stop.

CODING RULES (enforce strictly):
- Member variables: m_ prefix
- Private methods: camelCase, public methods: PascalCase
- Use constexpr over #define for constants
- No raw owning pointers — use smart pointers or RAII wrappers
- No #pragma warning(disable/push) — fix warnings at the source
- Match the existing file's include order and namespace style
- Add minimal comments: only explain non-obvious WHY, never WHAT

After all steps are done, write "## Status: COMPLETE" at the top of workflow/progress.md.
```

---

## After the agent finishes

Read `workflow/progress.md` and report to the user:
- Which steps completed
- Any blockers encountered
- Whether implementation is complete or needs a follow-up `/implement --continue`

Remind the user to run `/find-bugs` and `/check-style` on the changed files, then `/write-tests` if test requirements were listed in the plan.
