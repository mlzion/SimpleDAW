# v0.26.2 Context UI Move Windows acceptance

## Final owner acceptance

The owner accepted **v0.26.2 Context UI Move (CUI2)** on Windows at `a8fcd4b`, tagged `v0.26.2-experimental`. The full A–J live checklist passed with one minor deferred finding.

| Windows live checks | Owner result |
| --- | --- |
| All numbered checks in A–F and H–J | PASS |
| All numbered checks in G except G05 | PASS |
| G05 — Timeline ruler interaction isolation | DEFERRED as F07 to v0.26.3 |

**F01–F06 are closed.** CUI2 and W01–W05 are accepted, with F07 as the documented W03 exception. Correct F07 during CUI3 implementation after design review and separate authorization.

Windows x64 Release `/MD` CI passed 18/18 CTests and 10/10 additional instrumented Inspector Clone runs. Runtime dependencies, isolated startup, ZIP and source integrity, and all ten bundled drum samples passed verification. [Accepted Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/45890602357) · [Accepted Windows ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45890602357/artifacts/15152520441)

| File | SHA-256 |
| --- | --- |
| Windows ZIP | `62d2de94170da083a3b41bce411fbc36971c8ae92d5b374b0eeb4aa2a32b1b3a` |
| Matching source archive | `89e9d9012042c7aaed8ebe5c09670fee9831e526033371801a971dd8856521de` |

## Initial owner verification

The owner reported eight clean passes, two passes with findings, and two failures on `d38f90b`. The Windows x64 Release `/MD` build passed all 18 automated CTests, runtime dependency verification, and isolated startup and shutdown. The package and matching source passed integrity checks, including all ten bundled drum WAVs. [Initial Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733) · [Initial Windows ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733/artifacts/15087783415)

| Finding | Owner observation | Correction and focused Windows retest |
| --- | --- | --- |
| F01 | Snap-arrow graphics were garbled. Arrow actions and hit sizes passed. | Render opposing vector chevrons. Verify readability, hover and disabled states, both rollover directions, and popup persistence. |
| F02 | Opening Piano Roll did not reveal the MIDI clip. Other Piano Roll movement tests passed. | Clip visibility now uses the shared Track List and Timeline viewport above the dock. Open normal and tall MIDI rows near the bottom and resize while the editor is open. Verify the complete row is above Piano Roll after the slide, horizontal view stays put, and closing restores the full arrangement. |
| F03 | The edit-cursor handle worked but was too small. | The ruler keeps its bar and elapsed-time labels above a dedicated handle area. Check the larger cursor hit area, selection feedback, ordinary ruler seeking, and range dragging. |
| F04 | The time-window handle was absent, preventing selection and Move. | Make a short drag within one bar using startup 1 Bar snap. Confirm a labeled time-window handle appears and highlights when clicked. Move it in both directions and verify duration and loop state. Delete must clear the window and loop markers while leaving selected or enclosed clips intact. Retest the handle with the range near the window edge. |

The initial F04 correction created an unsnapped range when both snapped endpoints coincided. See the revised W03 record below for current behavior.

## Correction verification

Debian 13 x64 Release (Clang 19, Xvfb) passed all 18 CTests. Focused coverage renders both vector chevrons and the selectable time-window handle, creates a window via a short 1 Bar snap drag, checks edge hit testing, and verifies movement and Delete. Layout coverage checks that normal and tall edited rows fit above the actual Piano Roll dock after layout and resize. Native X11 input confirmed the time-window handle appeared after a short ruler drag and the edited MIDI row scrolled into view above Piano Roll.

At this stage, final Windows CI and owner retests for F01–F04 were still pending.

### Windows failure investigation at `c9a89fd`

Windows x64 Release `/MD` compiled, then 16 of 18 CTests passed. The failed test step skipped runtime verification and ZIP publication. [Windows failure log](https://github.com/mlzion/SimpleDAW/actions/runs/45764439998/job/139721848333)

- **Context Toolbar.** The chevron assertion read from the default JUCE image while its `Graphics` writer was still alive. On Windows that image can use Direct2D, which calls `EndDraw` when its frame finishes. The assertions now read only after the drawing scope ends, including the related ruler rendering check. The test still paints the real button and requires opposite chevron tips. It reports the pixel counts on failure. The popup rendering code, hit bounds, rollover and persistence are unchanged. The assertion passed in the subsequent Windows CI runs at `e1f9ed9` and `a8fcd4b`. [JUCE native image](https://github.com/juce-framework/JUCE/blob/19edd538429c93d277bf95b55aaa7e3eb545f951/modules/juce_graphics/native/juce_Direct2DImage_windows.cpp) · [JUCE drawing lifecycle](https://github.com/juce-framework/JUCE/blob/19edd538429c93d277bf95b55aaa7e3eb545f951/modules/juce_graphics/native/juce_Direct2DGraphicsContext_windows.cpp)
- **Inspector Clone.** This case exercises linked MIDI clip creation and movement, Inspector property propagation, unlinking, parent promotion, Undo/Redo, save/reopen, parent and child badge rendering, and linked audio clip creation. The only crash evidence is `0xc0000409`. The log does not identify the failing operation, fail-fast subtype, module or stack. The same test passed in the earlier v0.26.2 Windows run. F01–F04 do not modify Clone logic, and this case does not open the CUI popup or Piano Roll or use the ruler handles. This evidence does not establish intermittency or rule out an indirect regression. [Earlier passing run](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733)

The clone test now emits flushed checkpoints before each major operation and teardown. On the next authorized Windows run the workflow repeats the case ten times after the full suite passes. If the case fails in either step, the workflow prints matching Application events and attempts a bounded debugger rerun where Windows SDK `cdb` is available. The debugger records exception parameters and a stack if the fail-fast recurs. The original failure remains a failure and blocks publication. The exact crash cause remains unclassified. The termination did not recur at `e1f9ed9` or `a8fcd4b`.

The post-investigation build passed all 18 Linux Release CTests under Xvfb and 20 additional consecutive runs each of `context_toolbar` and `inspector_clone`. Review of F01–F04 found no changes needed to their application behavior. Subsequent Windows runs confirmed the chevron readback correction and exercised the instrumented Clone repeats.


### F01–F05 owner results

The owner passed F01–F04 on `e1f9ed9`. Windows CI at that revision passed all 18 CTests and all 10 additional Inspector Clone runs, then verified runtime imports and isolated startup. [Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/45769122990) · [Live-tested ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45769122990/artifacts/15094125741)

**F05 failed live at `efa4ed2`.** The pointer-following preview was lost and a drag returning to zero length restored the previous window. Revised W03 replaces the F04 selection and popup interaction with direct manipulation. F01–F03 retain their owner PASS results.

**Revised W03 and F05 verification coverage.**

- Click the ruler to position the edit cursor. Drag its handle horizontally with snap and double-click it to reset to zero. Direct handle gestures leave the playback position alone.
- Create a time window from an off-grid point in each direction. The painted edge follows the pointer between thresholds. On release, the length rounds from the snapped origin at each successive halfway threshold, in either direction and while shrinking. Release below the first threshold or after returning to the start. Verify an existing window is cleared in both cases. Repeat with 1 Bar, a finer grid and Snap OFF (10 ms).
- Drag the center handle to move the window. Verify duration and enabled looping are preserved and loop markers follow. Double-click the center to delete. Resize each boundary separately, then drag one onto the other to clear the window and disable looping.
- Verify ruler readability and hit areas at different zoom levels and viewport edges. Check that active transport blocks handle edits and that ruler position clicks retain their existing behavior. Confirm the selected clips remain available to Edit and Move after using ruler handles. Retest time-window-dependent editing operations.

**F06 — command audit.** The former selected ruler target took precedence over a retained Timeline clip selection, which could suppress or redirect Edit and Move. Removing that path restores ordinary clip command dispatch. Linux regression coverage exercises Timeline, Track List, Piano Roll and Drum Pattern Editor dispatch, and checks Inspector and Effects Editor restrictions. No additional command regression was reproduced. [Command audit](../CONTEXT_UI_MOVE_AUDIT.md)

Debian 13 x64 Release passed all 18 Linux CTests under Xvfb. The ruler regression drives mouse-down, multiple drag events, reversal and mouse-up. It checks painted preview pixels, threshold arithmetic for 1 Bar, 1/16 and Snap OFF, direct cursor positioning and reset, moving and resizing, deletion from both boundaries, transport restrictions and loop markers. Native X11 input verified the painted range edge while dragging and reversing, the snapped release position, center movement, resizing and collapse, and both double-click actions.
