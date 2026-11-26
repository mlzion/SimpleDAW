# v0.26.1 Context UI Foundation acceptance

## Integrated Windows baseline

The owner reported all C01–C10 passed on `790f532`, including subjective appearance and usability. Feedback was very positive. The sole requested refinement was the left-to-right Edit order **Cut | Copy | Paste | Delete**. [Context UI specification and checklist](../CONTEXT_UI_EXPERIMENT.md#windows-live-acceptance)

The Windows x64 Release `/MD` build at `790f532` passed all 18 CTests, runtime dependency verification, and isolated application startup and clean shutdown. The published ZIP passed its SHA-256 and ZIP integrity checks, and its source manifest and application sources matched `790f532` after accounting for Windows line endings. [Windows CI run](https://github.com/mlzion/SimpleDAW/actions/runs/45723282460) · [Live-tested baseline ZIP](https://github.com/mlzion/SimpleDAW/actions/runs/45723282460/artifacts/15072664248)

## v0.26.1 correction verification

Debian 13 x64 Release (Clang 19, `-O1`, Xvfb) passed all 18 CTests, including the on-screen left-to-right Edit order assertion and the existing command and clipboard regressions. A native X11 smoke test verified the v0.26.1 window title, popup opening, wheel cycling, dismissal, and classic restoration.

The first experimental milestone is subject to verification of the corrected Edit order. Final Windows x64 Release `/MD` CI awaits authorization. When the corrected ZIP is available, confirm the v0.26.1 version, open the popup in experimental mode, and verify that Edit opens first with the four buttons in the approved order. Check that each button still performs the corresponding UAT command. [Draft PR #29](https://github.com/mlzion/SimpleDAW/pull/29)
