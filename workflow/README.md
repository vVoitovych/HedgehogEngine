# Workflow State Directory

This directory holds runtime state files for the multi-agent workflow.

## Files (generated, gitignored)

| File | Written by | Read by |
|------|-----------|---------|
| `current-ticket.md` | `/plan` | `/implement` |
| `current-plan.md` | `/plan` | `/implement`, `/write-tests` |
| `progress.md` | `/implement` | `/implement --continue` |
| `current-breakdown.md` | `/jira-tickets` | `/jira-tickets` (approval step) |
| `current-tickets.md` | `/jira-tickets` | you, when picking the next ticket |

## Recovery after context/token-limit reset

If Claude Code hits the token limit mid-implementation:

1. Start a new session in the same directory.
2. Run `/implement --continue`
3. The agent reads `workflow/progress.md` to find the last completed step and resumes from the next one.

If you need to revisit the plan: open `workflow/current-plan.md` directly.

## Workflow overview

```
/jira-tickets <problem> → Opus agent researches and drafts a sized breakdown
                        → saved to current-breakdown.md → user approves
                        → tickets created in Jira → current-tickets.md

/plan <ticket>          → Opus agent produces plan → saved to current-plan.md
                        → User approves

/implement              → Sonnet agent executes plan step-by-step
                        → Progress saved after each step to progress.md
/implement --continue   → Resumes from last checkpoint

/find-bugs [path]       → Sonnet agent scans for bugs
/check-style [path]     → Haiku agent checks CODING_CONVENTIONS.md
/write-tests [path]     → Sonnet agent writes doctest tests
```

## Model selection rationale

| Command | Model | Reason |
|---------|-------|--------|
| `/jira-tickets` | Opus | Sizing a breakdown correctly needs full architectural context |
| `/plan` | Opus 4.8 | Needs deep architectural reasoning and full project context |
| `/implement` | Sonnet 4.6 | Balanced: follows a clear plan, needs correctness not creativity |
| `/find-bugs` | Sonnet 4.6 | Needs reasoning about subtle bugs and Vulkan semantics |
| `/check-style` | Haiku 4.5 | Pattern-matching task; fast and cheap |
| `/write-tests` | Sonnet 4.6 | Needs to understand intent to write meaningful tests |
