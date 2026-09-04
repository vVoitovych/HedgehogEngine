# Development Workflow

Every change to HedgehogEngine starts as a Jira ticket in project **HE**
(https://viktoravoitovych.atlassian.net) and lands as one small, reviewable pull
request.

## Why this exists

The renderer-architecture work reached ~45,000 changed lines on a single branch
before anyone tried to review it. A diff that size cannot be reviewed — not by a
human, not by a tool. The fix is not "review harder", it is to never create that
diff: break the problem down *before* writing code, and merge in slices small
enough that each one can be read in one sitting.

**The size rule: a pull request targets under 1000 changed lines.** Trivial
mechanical diffs (renames, formatting, generated files) are exempt — say so in
the PR description. Anything else that exceeds the budget is a ticket that was
not broken down far enough; split it.

---

## One-time setup

### 1. Install the git hooks

```
Scripts\InstallHooks.bat
```

Points `core.hooksPath` at `.githooks/`. This gives you:

- `prepare-commit-msg` — prefills `HE-123: ` from the current branch name.
- `commit-msg` — rejects a commit whose subject has no ticket key.

`Scripts\SetupWindows.bat` runs this for you.

### 2. Connect Jira to GitHub (do once, in the browser)

1. In Jira: **Apps → Explore more apps →** search *GitHub for Jira* → **Install**.
   (Or start from GitHub's side at https://github.com/apps/jira.)
2. **Configure → Connect a GitHub account**, authorize the app, and grant it
   access to `vVoitovych/HedgehogEngine`.
3. Jira now scans branches, commits and PRs for `HE-<number>` and shows them in
   the ticket's **Development** panel.

### 3. Add the two automation rules

**Project settings → Automation → Create rule** in the HE project:

| Rule | Trigger | Action |
|------|---------|--------|
| Move to In Review | *Pull request created* | Transition issue to **In Review** |
| Move to Done | *Pull request merged* | Transition issue to **Done** |

Optionally a third: *Branch created* → transition to **In Progress**.

Jira ships templates for all three — look for "When a pull request is created"
in the rule template list rather than building them from scratch. The pull
request triggers only appear once step 2 is complete.

The HE workflow already has the four statuses these rules need:
**To Do → In Progress → In Review → Done**.

---

## Ticket model

| Level | Issue type | Scope |
|-------|-----------|-------|
| Initiative | **Epic** | A body of work spanning weeks, e.g. *Renderer architecture rewrite*. Never coded against directly. |
| Unit of work | **Story** | One PR that changes observable behaviour (a feature, a fix a user would notice). |
| Unit of work | **Task** | One PR of internal work — refactor, extraction, build/tooling, test coverage. |

**One ticket = one branch = one pull request = one merge.** If a ticket cannot
be delivered in under ~1000 lines, it is two tickets.

Subtasks are not used. They do not get their own board cards in a team-managed
project, and the extra level buys nothing when every Story is already one PR.

Ordering between tickets is expressed with **Blocks** links, not by ticket
number. `HE-12 blocks HE-13` means 13 cannot start until 12 has merged.

### What a good ticket contains

- **Summary** — imperative and specific: *Extract RHIContext swapchain ownership
  into SwapchainManager*, not *Swapchain work*.
- **Description** — the problem, the intended approach, and the files/modules in
  scope.
- **Acceptance criteria** — a checklist someone else could verify.
- **Size estimate** — expected changed lines, so an over-budget ticket is
  visible before the code is written.

### Definition of Done

A ticket is Done when its PR is merged and:

- `Scripts\Build.bat Debug` and `Scripts\Build.bat Release` both exit 0.
- `Scripts\RunTests.bat Debug` exits 0.
- If it touched HedgehogRenderer, RHI or shaders:
  `Binaries\windows-x86_64\Debug\Editor\Editor.exe --smoke-test` exits 0.
- If it touched the frame loop or a render pass: before/after `--benchmark`
  numbers are quoted in the PR (see PERFORMANCE.md).
- The code follows CODING_CONVENTIONS.md.

---

## Branching

**Task branches cut from `master` and merge back to `master`.** There are no
long-lived integration branches.

Long-lived epic branches were considered and rejected: they re-create exactly
the problem this workflow exists to prevent — the epic-to-master pull request is
just as unreviewable as the 45,000-line diff was, and the branch drifts from
master for as long as it lives.

Instead, keep `master` always green and land partial work safely:

- Add the new code path alongside the old one and switch over in a later ticket.
- Gate an incomplete feature behind a settings flag or a compile-time constant.
- Land an interface with no callers, then land callers in the next ticket.

Only if intermediate states genuinely cannot build or run should you raise an
integration branch — and then say so in the epic, so the eventual merge is
expected rather than a surprise.

### Names

| Thing | Format | Example |
|-------|--------|---------|
| Branch | `HE-<n>-<short-slug>` | `HE-42-extract-swapchain-manager` |
| Commit subject | `HE-<n>: <imperative summary>` | `HE-42: Move swapchain ownership into SwapchainManager` |
| PR title | `HE-<n>: <summary>` | `HE-42: Extract SwapchainManager from RHIContext` |

All three are scanned by Jira. The PR title is the one that matters most — it
becomes the squash-merge commit subject, and CI rejects a PR whose title has no
ticket key.

For the rare commit with genuinely no ticket, prefix `NO-TICKET: `. Use it
sparingly; it exists so you never reach for `--no-verify`.

---

## The loop

```
1. Pick a ticket        HE-42, status To Do
2. git switch -c HE-42-extract-swapchain-manager master
3. Work; commit as you go, each subject starting HE-42:
4. Push and open a PR titled "HE-42: ..."   -> Jira moves HE-42 to In Review
5. Review, address feedback, merge          -> Jira moves HE-42 to Done
6. git switch master && git pull
```

Steps 4 and 6 are the whole point: the ticket state follows the code, and you
never have to remember to update the board.

---

## Breaking a problem down

Run `/jira-tickets <problem>`. It researches the codebase, proposes an epic and
a sized set of Stories/Tasks with Blocks links, shows you the breakdown for
approval, and only then creates them in Jira.

Rules of thumb it applies, and you should too:

- **Split by module boundary.** The dependency graph in CLAUDE.md is the natural
  seam — a ticket that touches RHI *and* HedgehogRenderer *and* Editor is
  usually three tickets.
- **Split interface from implementation.** Land the header and a stub, then the
  implementation, then the migration of callers.
- **Split migration by call site.** Ten call sites moving to a new API is often
  three tickets, not one.
- **Keep tests with the code they cover** — a ticket ships its own tests, it is
  not followed by a "and now add tests" ticket.
