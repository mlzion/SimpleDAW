# Context UI (CUI) — experimental design and implementation brief

Status: approved experiment, not canonical product specification. Branch: experiment/context-ui, based on main. Do not merge into main or alter other branches without explicit approval. Goal: eliminate excessive mouse travel and eventually remove permanent editing toolbars in a clean interface, usable with an ordinary three-button wheel mouse without modifiers. Existing interface remains available for comparison.

## Interaction (v0.26.1 accepted baseline; superseded where specified by CUI2 below)
- Middle-click opens a context-sensitive popup near pointer; middle-click again or outside click dismisses. Right-click while popup closed retains selection toggle; left-click/drag/double-click and wheel retain existing behavior when closed.
- While open, wheel cycles toolbar pages in either direction, wrapping; popup stays anchored, with stable dimensions for equivalent pages. Consume wheel events so underlying timeline/editor does not scroll or change velocity. Freeze originating workspace and selection context; opening must not implicitly select or change cursor. Validate target and command availability again on invocation; dismiss on invalidated context.
- Left-click invokes existing command handler and closes after a discrete action. Future parameter pages stay open for multiple adjustments, using drag controls rather than wheel. Escape and right-click dismiss the popup.
- Show applicable pages; retain stable button positions within a page, dim unavailable commands. Page title and page count/dots indicate current page. Start each invocation on Edit through v0.26.1. Move becomes the initial page once implemented in v0.26.2. When all four pages are present, forward wheel cycling is Move → Size → Edit → Modify → Move. Reverse cycling follows the reverse order.
- Visual standard: large recognizable vector icon above short text label, equal button dimensions, subtle rounded corners, dark slate surfaces, hover feedback, disabled dimming, clear toggle states (not color alone). Use consistent permissively licensed vector icons (Lucide-style; preserve license notices if importing assets). Exact dimensions/colors subject to Windows acceptance. Clamp popup to available screen bounds.
- Reuse existing UAT command dispatch, selection/focus rules, validation, clipboard and undo semantics. Popup must not steal effective editing context. Do not introduce new editing capabilities or change command semantics.

## Historical implementation authorization: CUI0/CUI1
1. Audit actual menu entries, command handlers, keyboard/mouse bindings, and availability by workspace at branch baseline; document current vs unbound vs proposed, with source references. Identify reusable dispatch paths; no wholesale command refactor.
2. Implement opt-in experimental mode, default existing interface; middle-click popup, anchor/clamp, dismissal, wheel page cycling and focus preservation. For CUI0 exercise cycling with a clearly labeled non-operational test page if necessary; no dummy production commands.
3. CUI1 implements the Edit page in left-to-right order Cut | Copy | Paste | Delete using existing command handlers. Hide only corresponding permanent Edit group in experimental mode once functional; restore on switching back. Other UAT groups, Inspector, navigation and transport remain intact. Support existing workspaces present in baseline; do not import unmerged features.
4. Add automated tests for dispatch equivalence, disabled commands, focus/selection preservation, wheel consumption and wrap, dismissal, screen bounds, classic mode restoration, undo and clipboard behavior. Run existing Linux CTests and Windows x64 Release /MD build/tests per project policy. Publish testable Windows ZIP with matching source via established workflow; report commit, tests and artifact. Stop for Windows live acceptance; no merge.

## Windows live acceptance

The owner reported all C01–C10 passed on `790f532`, including appearance and usability. The owner also passed C11, the v0.26.1 Edit button-order correction, on `e1921ec`. [Acceptance record](testing/context-ui-foundation.md)

- C01 classic mode unchanged; enable/disable experiment restores original controls.
- C02 middle-click opens near cursor; selection and edit cursor unchanged.
- C03 popup stays on screen at edges; stable anchor when cycling.
- C04 wheel cycles both directions and wraps without underlying scroll/velocity changes.
- C05 Cut/Copy/Paste/Delete match UAT behavior in each available context; invalid operations disabled; clipboard and undo correct.
- C06 outside click/middle-click/Escape dismiss without accidental editing; normal mouse behavior restored.
- C07 editor/track/timeline focus and selection remain correct, including multiple selections; stale context cannot execute.
- C08 visual readability, icon recognition, mouse travel and comfort at 1920x1080; report subjective findings.
- C09 existing regression suite passes; transport/recording restrictions preserved.

- C10 passed on the integrated Windows baseline, as reported by the owner.
- C11 verifies the corrected left-to-right Edit order Cut | Copy | Paste | Delete.

## Approved development sequence

| Version | Phase | Popup page |
| --- | --- | --- |
| v0.26.1 | CUI0/CUI1 — Context UI Foundation | Edit |
| v0.26.2 | CUI2 | Move · accepted at `a8fcd4b` |
| v0.26.3 | CUI3 | Size · includes deferred F07 ruler interaction isolation |
| v0.26.4 | CUI4 | Modify |

v0.26.1 is the first accepted experimental milestone. v0.26.2 is the accepted Move milestone, with F07 deferred. CUI3 awaits design review and separate implementation authorization. Each later phase requires its own Windows live acceptance before progression. [CUI2 acceptance](testing/context-ui-move.md#final-owner-acceptance) Preserve the existing UAT command behavior as each group is moved into the popup. Navigation, other controls, interface cleanup, and workflow improvements remain for separate authorization. [Workflow Improvement Log](WORKFLOW_IMPROVEMENT_LOG.md)

## v0.26.2 — CUI2 Move: approved implementation scope

Owner authorizes implementation on experiment/context-ui only, preserving the accepted v0.26.1-experimental tag and draft PR #29. This section supersedes conflicting baseline interaction rules.

- Move is the initial page; replace the test page with functional Move. Forward/reverse wheel cycles Move and Edit with rollover. Preserve the eventual Move → Size → Edit → Modify order. Hide permanent Move group only in experimental mode; classic controls remain available.
- Move layout: Left | Right | Up | Down, equal icon-above-label buttons, stable positions, unavailable actions dimmed. Audit existing UAT handlers and actual context availability before implementing; reuse existing validation, compatibility, bounds, transport restrictions and undo semantics. Report discrepancies rather than invent unsupported operations.
- Above Move buttons, compact SNAP row with left arrow, current value, right arrow. Generous clickable arrow areas. Left selects finer, right coarser; both wrap. Timeline sequence OFF (10 ms) → 1/32 → 1/16 → 1/8 → 1/4 → 1/2 (half note) → 1 Bar → OFF. Timeline startup default 1 Bar; meter-aware. Update existing snap state immediately without closing popup. Preserve independent Piano Roll snap and Drum Pattern Editor step resolution; reflect applicable context, disable selector where irrelevant (e.g. Track List). Wheel remains page cycling only. Reuse selector on future Size page, not a separate Snap page.
- Persistent popup across all implemented pages including Edit and all workspaces including Piano Roll and Drum Pattern Editor. Successful commands no longer automatically close popup. Preserve anchor and originating workspace; refresh command availability and surviving selection after each command. Never execute against stale/deleted targets; dismiss if context becomes invalid and cannot safely continue. Outside click dismisses and restores normal selection behavior; middle-click, right-click and Escape dismiss. Do not implicitly retarget on opening or introduce accidental edits. Keep editor lifecycle otherwise unchanged.
- W01–W05 in Workflow Improvement Log are authorized for this phase. Revised W03 supersedes selectable ruler targets: direct manipulation of edit cursor and time window, including boundary sizing, per the workflow log. Remove their UAT/CUI targeting; preserve other command semantics. W01, W02, W04, W05 retain accepted behavior.
- Individual Move clicks use existing undo conventions. Hold-to-repeat timing and gesture grouping are not finalized; do not introduce unapproved repeat behavior. Exact target sizes and visual dimensions are provisional for Windows live acceptance. Keep popup geometry stable across pages.

### Verification and delivery
Audit source command inventory and report context matrix. Add regression tests for Move dispatch, snap rollover and context isolation, persistence including Edit and invalidated targets, dismissal and selection, W01–W05, classic restoration and existing regressions. Run Linux CTests and native popup smoke checks. Report implementation commit and results before requesting Windows x64 Release /MD CI authorization. Preserve matching ZIP/source delivery and stop for owner Windows live acceptance. No merge, no CUI3/CUI4 or MX0 work, no changes to main or other branches.

### v0.26.2 verification checklist

- Open on Move and cycle both ways between Move and Edit. Confirm the popup stays anchored and wheel events do not reach the workspace.
- Move clips, tracks, Piano Roll notes and drum hits with eligible directions. Confirm bounds, type checks, transport restrictions and one Undo per existing undoable Move command.
- Change Timeline snap with either arrow and check both wrap points, bar snap in different meters and the 10 ms OFF step. Check separate Piano Roll choices, fixed drum sixteenths and disabled Track List snap.
- Perform Edit and Move actions without reopening the popup. Verify surviving selections, updated availability, stale-target dismissal, and normal input after dismissal.
- Verify direct cursor drag, ruler click positioning and double-click reset to zero; no cursor/window UAT or CUI targeting.
- Verify continuous pointer-following range preview and nearest-division final snapping at successive halfway thresholds, reverse and shrinking drags, zero-length release clearing previous range, and 10 ms OFF. Verify center drag preserves duration, double-click deletes, both boundary handles resize and reaching opposite boundary deletes. Preserve clip selection, loop semantics and time-window-dependent commands.
- Recheck existing Move/Edit commands across workspaces for possible F06 regressions; reproduce and report any failures. The owner closed F05 and F06 on `a8fcd4b` and deferred only G05 (F07).
- Check that opening Piano Roll keeps the edited row above the dock, the Master scrolls out while retaining first position, and ordinary Track List identifiers follow track order. Confirm classic mode restores the permanent Edit and Move groups.
