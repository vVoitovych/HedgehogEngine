# /jira-tickets — Jira Breakdown Agent

You are breaking a problem down into Jira tickets for HedgehogEngine, a Vulkan
C++20 game engine.

**Problem:** $ARGUMENTS

The goal is not "write some tickets". It is to guarantee that no pull request
from this work exceeds ~1000 changed lines, because a diff larger than that
cannot be reviewed. Read WORKFLOW.md before you start — it is the contract this
command implements.

---

## Jira facts (already known — do not rediscover them)

| | |
|---|---|
| Site | https://viktoravoitovych.atlassian.net |
| cloudId | `6aaa8340-b88e-4ff0-9126-81f307ca19e5` |
| Project | Hedgehog Engine, key **HE** (team-managed) |
| Issue types | Epic, Story, Task, Subtask |
| Statuses | To Do → In Progress → In Review → Done |
| Link types | Blocks, Cloners, Duplicate, Relates |

Use **Epic** for the initiative, **Story** for a PR that changes observable
behaviour, **Task** for internal work (refactor, tooling, tests). **Never create
Subtasks** — they get no board card in a team-managed project.

Confirm the project key once with `getVisibleJiraProjects` (searchString
"Hedgehog") before creating anything. If it still comes back as `KAN`, the
rename described in WORKFLOW.md has not been done — tell the user and ask
whether to proceed against `KAN` or wait.

---

## Step 1 — Resolve the input

- If `$ARGUMENTS` is a file path, read that file and treat it as the problem
  statement.
- If `$ARGUMENTS` is prose, use it directly.
- If `$ARGUMENTS` is empty, ask the user what problem to break down and stop
  until they answer.

---

## Step 2 — Clarify (interactive, do this before any research)

Ask up to 3 focused questions, only where different answers would produce a
materially different breakdown. Wait for answers.

Worth asking about:
- Scope boundary — which modules are in and, more usefully, which are out?
- Is there a hard ordering constraint, or can tickets land in any order?
- Must `master` stay shippable throughout, or is a temporarily dead code path
  acceptable between tickets?
- Does this replace an existing subsystem, or sit alongside it?

If the problem is already unambiguous, say *"Problem is unambiguous — proceeding
to research."* and skip ahead.

---

## Step 3 — Research and draft (spawn an Opus agent)

Call the Agent tool with **`model: "opus"`** and give it this prompt:

```
You are a senior C++ game engine architect breaking a problem into Jira tickets
for HedgehogEngine.

Problem: <full problem statement + clarification answers>

Read first:
- WORKFLOW.md          (ticket model, sizing rule, branching — the contract)
- CLAUDE.md            (module architecture and the dependency graph)
- CODING_CONVENTIONS.md
Then read the actual source of every module in scope. Do not plan against
assumptions: check what the code does today. Where the plan depends on a file's
current shape, name the file and the line.

Produce a breakdown and save it to workflow/current-breakdown.md.

SIZING IS THE HARD CONSTRAINT. Every ticket must be deliverable in under 1000
changed lines. For each ticket, estimate the changed lines and show the
reasoning (files touched x rough size). A ticket you cannot estimate is a ticket
you do not understand well enough to write — research further, or split it.

How to split, in order of preference:
1. By module boundary. The dependency graph in CLAUDE.md is the natural seam.
   A ticket touching RHI and HedgehogRenderer and Editor is usually 3 tickets.
2. Interface before implementation. Land the header plus a stub, then the
   implementation, then the migration of callers.
3. Migration by call site. Ten call sites moving to a new API is often 3
   tickets, not 1.
4. New path alongside old. Add the new code, switch over in a later ticket,
   delete the old path in a third.

Every ticket must leave master building and green: Scripts\Build.bat Debug and
Release exit 0, Scripts\RunTests.bat Debug exits 0. A ticket that can only
compile once a later ticket lands is mis-split. Gate an incomplete feature
behind a settings flag or a compile-time constant rather than breaking the
build.

Tests ship with the code they cover. Do not emit "and now add tests" tickets.

--- SAVE EXACTLY THIS STRUCTURE TO workflow/current-breakdown.md ---

# Breakdown: <short title>
Date: <today YYYY-MM-DD>
Status: PENDING_APPROVAL

## Problem
<full problem statement>

## Approach
<3-5 sentences: the overall shape of the solution and the order of attack>

## Epic
**Summary:** <imperative, specific>
**Description:**
<why this work exists, what "done" means for the whole initiative, and what is
explicitly out of scope>

## Tickets

### T1 — <imperative summary>
- **Type:** Story | Task
- **Blocked by:** none | T2, T3
- **Estimated lines:** ~NNN  (<files touched x rough size>)
- **Modules:** <from the CLAUDE.md graph>
- **Description:**
  <the problem this ticket solves, the intended approach, and the specific files
  in scope>
- **Acceptance criteria:**
  - [ ] <verifiable statement>
  - [ ] <verifiable statement>
- **Verification:** <which of Build / RunTests / --smoke-test / --benchmark
  apply, beyond the always-required build and tests>

### T2 — ...

## Ordering
<a short dependency narrative: what must land first and why, and which tickets
can proceed in parallel>

## Risks
- <risk or constraint, and how the split mitigates it>

## Total
<N tickets, ~NNNN lines total, longest ticket ~NNN lines>
```

---

## Step 4 — Check the draft yourself before showing it

Read `workflow/current-breakdown.md` and verify, without involving the user:

- No ticket estimated over 1000 lines. If one is, send it back to the agent to
  be split — do not pass it on with a caveat.
- Every ticket has acceptance criteria someone else could verify.
- The Blocks graph has no cycles, and every referenced ticket exists.
- Summaries are specific and imperative. *"Swapchain work"* is not a summary.
- No Subtasks.

Re-run the agent with the specific failures until it passes. Only then continue.

---

## Step 5 — Present for approval

Show the user a compact table:

| # | Type | Summary | ~Lines | Blocked by |
|---|------|---------|--------|------------|

Followed by the epic summary, the ordering narrative, and the total.

Then ask: *"Create these N tickets in HE? Reply 'approved', or tell me what to
change."*

**Create nothing in Jira before an explicit approval.** Jira tickets are
outward-facing state: a wrong breakdown that only exists in a file is free to
throw away, one that exists in Jira is not. If the user asks for changes, revise
and re-present.

---

## Step 6 — Create the tickets

In this order:

1. **The epic** — `createJiraIssue` with `issueTypeName: "Epic"`.
2. **Each child** — `createJiraIssue` with `issueTypeName: "Story"` or `"Task"`
   and `parent: "<epic key>"`. If `parent` is rejected, retry with
   `additional_fields: {"parent": {"key": "<epic key>"}}`.
   Put the description, acceptance criteria and verification steps into the
   description field, as Markdown.
3. **The Blocks links** — `createIssueLink` with `type: "Blocks"`, where
   `inwardIssue` is the blocker and `outwardIssue` is the blocked ticket.

If a call fails, stop and report which tickets were created before the failure.
Do not retry blindly — a half-created epic is easier to finish by hand than to
untangle from duplicates.

Then write `workflow/current-tickets.md`:

```
# Tickets: <short title>
Epic: HE-<n> — <summary>
Created: <YYYY-MM-DD>

| Key | Type | Summary | ~Lines | Blocked by | Branch |
|-----|------|---------|--------|------------|--------|
| HE-<n> | Task | ... | ~400 | — | HE-<n>-<slug> |
```

---

## Step 7 — Report

Tell the user:

- The epic key and its URL
  (`https://viktoravoitovych.atlassian.net/browse/HE-<n>`).
- The table of created tickets with their keys.
- Which ticket to start with — the first one with no blockers — and the exact
  command to begin:

  ```
  git switch -c HE-<n>-<slug> master
  ```

- A reminder that `/plan HE-<n>` produces the implementation plan for it.
