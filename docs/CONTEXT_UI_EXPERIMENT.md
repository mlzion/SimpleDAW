# Context UI (CUI) — experimental design and implementation brief

Status: approved experiment, not canonical product specification. Branch: experiment/context-ui, based on main. Do not merge into main or alter other branches without explicit approval. Goal: eliminate excessive mouse travel and eventually remove permanent editing toolbars in a clean interface, usable with an ordinary three-button wheel mouse without modifiers. Existing interface remains available for comparison.

## Interaction
- Middle-click opens a context-sensitive popup near pointer; middle-click again or outside click dismisses. Right-click while popup closed retains selection toggle; left-click/drag/double-click and wheel retain existing behavior when closed.
- While open, wheel cycles toolbar pages in either direction, wrapping; popup stays anchored, with stable dimensions for equivalent pages. Consume wheel events so underlying timeline/editor does not scroll or change velocity. Freeze originating workspace and selection context; opening must not implicitly select or change cursor. Validate target and command availability again on invocation; dismiss on invalidated context.
- Left-click invokes existing command handler and closes after a discrete action. Future parameter pages stay open for multiple adjustments, using drag controls rather than wheel. Escape and right-click dismiss the popup.
- Show applicable pages; retain stable button positions within a page, dim unavailable commands. Page title and page count/dots indicate current page. Start each invocation on Edit through v0.26.1. Move becomes the initial page once implemented in v0.26.2. When all four pages are present, forward wheel cycling is Move → Size → Edit → Modify → Move. Reverse cycling follows the reverse order.
- Visual standard: large recognizable vector icon above short text label, equal button dimensions, subtle rounded corners, dark slate surfaces, hover feedback, disabled dimming, clear toggle states (not color alone). Use consistent permissively licensed vector icons (Lucide-style; preserve license notices if importing assets). Exact dimensions/colors subject to Windows acceptance. Clamp popup to available screen bounds.
- Reuse existing UAT command dispatch, selection/focus rules, validation, clipboard and undo semantics. Popup must not steal effective editing context. Do not introduce new editing capabilities or change command semantics.

## Initial implementation authorization: CUI0/CUI1 only
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
| v0.26.2 | CUI2 | Move |
| v0.26.3 | CUI3 | Size |
| v0.26.4 | CUI4 | Modify |

v0.26.1 is the first accepted experimental milestone. Each later phase requires separate implementation authorization and Windows live acceptance before progression. Preserve the existing UAT command behavior as each group is moved into the popup. Navigation, other controls, interface cleanup, and workflow improvements remain for separate authorization. [Workflow Improvement Log](WORKFLOW_IMPROVEMENT_LOG.md)
