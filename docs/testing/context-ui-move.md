# v0.26.2 Context UI Move Windows acceptance

## Initial owner verification

The owner reported eight clean passes, two passes with findings, and two failures on `d38f90b`. The Windows x64 Release `/MD` build passed all 18 automated CTests, runtime dependency verification, and isolated startup and shutdown. The package and matching source passed integrity checks, including all ten bundled drum WAVs. [Initial Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733) · [Initial Windows ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733/artifacts/15087783415)

| Finding | Owner observation | Correction and focused Windows retest |
| --- | --- | --- |
| F01 | Snap-arrow graphics were garbled. Arrow actions and hit sizes passed. | Render opposing vector chevrons. Verify readability, hover and disabled states, both rollover directions, and popup persistence. |
| F02 | Opening Piano Roll did not reveal the MIDI clip. Other Piano Roll movement tests passed. | Clip visibility now uses the shared Track List and Timeline viewport above the dock. Open normal and tall MIDI rows near the bottom and resize while the editor is open. Verify the complete row is above Piano Roll after the slide, horizontal view stays put, and closing restores the full arrangement. |
| F03 | The edit-cursor handle worked but was too small. | The ruler keeps its bar and elapsed-time labels above a dedicated handle area. Check the larger cursor hit area, selection feedback, ordinary ruler seeking, and range dragging. |
| F04 | The time-window handle was absent, preventing selection and Move. | Make a short drag within one bar using startup 1 Bar snap. Confirm a labeled time-window handle appears and highlights when clicked. Move it in both directions and verify duration and loop state. Delete must clear the window and loop markers while leaving selected or enclosed clips intact. Retest the handle with the range near the window edge. |

The initial F04 correction created an unsnapped range when both snapped endpoints coincided. F05 supersedes that behavior. Window boundary sizing remains deferred.

## Correction verification

Debian 13 x64 Release (Clang 19, Xvfb) passed all 18 CTests. Focused coverage renders both vector chevrons and the selectable time-window handle, creates a window via a short 1 Bar snap drag, checks edge hit testing, and verifies movement and Delete. Layout coverage checks that normal and tall edited rows fit above the actual Piano Roll dock after layout and resize. Native X11 input confirmed the time-window handle appeared after a short ruler drag and the edited MIDI row scrolled into view above Piano Roll.

Final Windows CI and owner retests for F01–F04 await separate authorization.

### Windows failure investigation at `c9a89fd`

Windows x64 Release `/MD` compiled, then 16 of 18 CTests passed. The failed test step skipped runtime verification and ZIP publication. [Windows failure log](https://github.com/mlzion/SimpleDAW/actions/runs/45764439998/job/139721848333)

- **Context Toolbar.** The chevron assertion read from the default JUCE image while its `Graphics` writer was still alive. On Windows that image can use Direct2D, which calls `EndDraw` when its frame finishes. The assertions now read only after the drawing scope ends, including the related ruler rendering check. The test still paints the real button and requires opposite chevron tips. It reports the pixel counts on failure. The popup rendering code, hit bounds, rollover and persistence are unchanged. Windows confirmation is pending. [JUCE native image](https://github.com/juce-framework/JUCE/blob/19edd538429c93d277bf95b55aaa7e3eb545f951/modules/juce_graphics/native/juce_Direct2DImage_windows.cpp) · [JUCE drawing lifecycle](https://github.com/juce-framework/JUCE/blob/19edd538429c93d277bf95b55aaa7e3eb545f951/modules/juce_graphics/native/juce_Direct2DGraphicsContext_windows.cpp)
- **Inspector Clone.** This case exercises linked MIDI clip creation and movement, Inspector property propagation, unlinking, parent promotion, Undo/Redo, save/reopen, parent and child badge rendering, and linked audio clip creation. The only crash evidence is `0xc0000409`. The log does not identify the failing operation, fail-fast subtype, module or stack. The same test passed in the earlier v0.26.2 Windows run. F01–F04 do not modify Clone logic, and this case does not open the CUI popup or Piano Roll or use the ruler handles. This evidence does not establish intermittency or rule out an indirect regression. [Earlier passing run](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733)

The clone test now emits flushed checkpoints before each major operation and teardown. On the next authorized Windows run the workflow repeats the case ten times after the full suite passes. If the case fails in either step, the workflow prints matching Application events and attempts a bounded debugger rerun where Windows SDK `cdb` is available. The debugger records exception parameters and a stack if the fail-fast recurs. The original failure remains a failure and blocks publication. The exact crash cause remains open pending Windows evidence.

The post-investigation build passed all 18 Linux Release CTests under Xvfb and 20 additional consecutive runs each of `context_toolbar` and `inspector_clone`. Review of F01–F04 found no changes needed to their application behavior. The diagnostics and chevron readback correction are pending Windows verification.


### F01–F05 owner results

The owner passed F01–F04 on `e1f9ed9`. Windows CI at that revision passed all 18 CTests and all 10 additional Inspector Clone runs, then verified runtime imports and isolated startup. [Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/45769122990) · [Live-tested ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45769122990/artifacts/15094125741)

**F05 remains pending Windows verification.** Dragging below the first halfway threshold leaves the existing selection intact or creates no window if none exists. At each halfway threshold, the moving end snaps one increment farther in that direction. The same divisions apply in reverse and while shrinking a drag. On Snap OFF the increment for creating a window is 10 ms. Ordinary cursor positioning, selected-window Move and Delete, and ruler handle sizes retain their existing behavior.

For the focused retest, create ranges from an off-grid start in both directions. Check just below, at and above the first, second and third halfway thresholds, then shrink the drag through them before releasing. Repeat with 1 Bar and a finer grid. At a zoom that makes 5 ms selectable, repeat with Snap OFF. Confirm coincident snapped endpoints never create an unsnapped window, and then select, move and delete a valid range.

The F05 correction passed all 18 Linux Release CTests under Xvfb. The Context UI test checks the first three halfway thresholds in both directions while extending and shrinking, with 1 Bar, 1/16 and Snap OFF. Mouse-event coverage verifies 4 ms creates no range and 5 ms commits a 10 ms range at high zoom. It also verifies preserving a previously committed range, ordinary cursor positioning, handle rendering and hit testing, and selected-window Move and Delete.
