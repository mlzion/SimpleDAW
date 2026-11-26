# v0.26.1 Context UI Foundation acceptance

**Accepted experimental milestone.** The accepted application revision is `e1921ec`, tagged `v0.26.1-experimental`. [Milestone tag](https://github.com/mlzion/SimpleDAW/tree/v0.26.1-experimental)

| Windows live checks | Revision | Owner result |
| --- | --- | --- |
| C01–C10 | `790f532` | PASS |
| C11 · Edit button-order correction | `e1921ec` | PASS |

## Integrated Windows baseline

The owner reported all C01–C10 passed on `790f532`, including subjective appearance and usability. Feedback was very positive. The sole requested refinement was the left-to-right Edit order **Cut | Copy | Paste | Delete**. [Context UI specification and checklist](../CONTEXT_UI_EXPERIMENT.md#windows-live-acceptance)

The Windows x64 Release `/MD` build at `790f532` passed all 18 CTests, runtime dependency verification, and isolated application startup and clean shutdown. The published ZIP passed its SHA-256 and ZIP integrity checks, and its source manifest and application sources matched `790f532` after accounting for Windows line endings. [Windows CI run](https://github.com/mlzion/SimpleDAW/actions/runs/45723282460) · [Live-tested baseline ZIP](https://github.com/mlzion/SimpleDAW/actions/runs/45723282460/artifacts/15072664248)

## v0.26.1 correction verification

Debian 13 x64 Release (Clang 19, `-O1`, Xvfb) passed all 18 CTests, including the on-screen left-to-right Edit order assertion and the existing command and clipboard regressions. A native X11 smoke test verified the v0.26.1 window title, popup opening, wheel cycling, dismissal, and classic restoration.

The owner passed C11 on `e1921ec`, verifying **Cut | Copy | Paste | Delete**. This completed Windows live acceptance for v0.26.1.

The accepted Windows x64 Release `/MD` run passed all 18 CTests, including the Edit order assertion. Runtime import verification, isolated startup, clean shutdown, and ZIP integrity checks passed. The ZIP contains `SimpleDAW-v0.26.1.exe`, all 10 drum samples, and `SimpleDAW-Windows-x64-source.tar.gz`. The source manifest identifies `e1921ec` and the pinned JUCE and Tracktion revisions. All 89 packaged application-source files matched the accepted commit after Windows line ending normalization. [Accepted Windows ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45731539661/artifacts/15077700824) · [Windows CI run](https://github.com/mlzion/SimpleDAW/actions/runs/45731539661)

| File | SHA-256 |
| --- | --- |
| Windows ZIP | `f4d2010bf275407bc774c8c657f36ceef6bce39ad692781bf79eb5c58a930b72` |
| Matching source archive | `dbc33aade1de90d609b719224e92b5a871fe349e43d2c9f4baf59542099190a2` |
