# v0.26.2 Context UI Move Windows acceptance

## Initial owner verification

The owner reported eight clean passes, two passes with findings, and two failures on `d38f90b`. The Windows x64 Release `/MD` build passed all 18 automated CTests, runtime dependency verification, and isolated startup and shutdown. The package and matching source passed integrity checks, including all ten bundled drum WAVs. [Initial Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733) · [Initial Windows ZIP and matching source](https://github.com/mlzion/SimpleDAW/actions/runs/45756530733/artifacts/15087783415)

| Finding | Owner observation | Correction and focused Windows retest |
| --- | --- | --- |
| F01 | Snap-arrow graphics were garbled. Arrow actions and hit sizes passed. | Render opposing vector chevrons. Verify readability, hover and disabled states, both rollover directions, and popup persistence. |
| F02 | Opening Piano Roll did not reveal the MIDI clip. Other Piano Roll movement tests passed. | Clip visibility now uses the shared Track List and Timeline viewport above the dock. Open normal and tall MIDI rows near the bottom and resize while the editor is open. Verify the complete row is above Piano Roll after the slide, horizontal view stays put, and closing restores the full arrangement. |
| F03 | The edit-cursor handle worked but was too small. | The ruler keeps its bar and elapsed-time labels above a dedicated handle area. Check the larger cursor hit area, selection feedback, ordinary ruler seeking, and range dragging. |
| F04 | The time-window handle was absent, preventing selection and Move. | Make a short drag within one bar using startup 1 Bar snap. Confirm a labeled time-window handle appears and highlights when clicked. Move it in both directions and verify duration and loop state. Delete must clear the window and loop markers while leaving selected or enclosed clips intact. Retest the handle with the range near the window edge. |

If snapping would collapse both ends of a real range drag to the same point, the correction uses the pointer endpoints. Horizontal Move still uses the selected snap increment. Window boundary sizing remains deferred.

## Correction verification

Debian 13 x64 Release (Clang 19, Xvfb) passed all 18 CTests. Focused coverage renders both vector chevrons and the selectable time-window handle, creates a window via a short 1 Bar snap drag, checks edge hit testing, and verifies movement and Delete. Layout coverage checks that normal and tall edited rows fit above the actual Piano Roll dock after layout and resize. Native X11 input confirmed the time-window handle appeared after a short ruler drag and the edited MIDI row scrolled into view above Piano Roll.

Final Windows CI and owner retests for F01–F04 await separate authorization.
