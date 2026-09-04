<!-- Title this PR "HE-<number>: <summary>" — CI checks it, and Jira uses it to
     move the ticket to In Review now and to Done when this merges. -->

## What and why

<!-- One paragraph. What changes, and what problem it solves. -->

## Changes

-
-

## Size

Changed lines: ~<!-- number -->

<!-- Target is under 1000. If this is over budget, say why here — a mechanical
     rename, generated files, or a slice that genuinely could not be split. -->

## Verification

- [ ] `Scripts\Build.bat Debug` exits 0
- [ ] `Scripts\Build.bat Release` exits 0
- [ ] `Scripts\RunTests.bat Debug` exits 0
- [ ] Renderer / RHI / shaders touched → `Editor.exe --smoke-test` exits 0
- [ ] Frame loop or render pass touched → before/after `--benchmark` numbers below
- [ ] Follows CODING_CONVENTIONS.md

<!-- Benchmark numbers, if applicable:
     before:  x.xx ms avg, xxx FPS
     after:   x.xx ms avg, xxx FPS
-->
