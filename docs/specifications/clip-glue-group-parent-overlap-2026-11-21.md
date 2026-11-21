# Clip Glue, Clip Groups, Parent/Child Tracks, and Overlap Playback
**Decision register — 21 November 2026**  
**Design status: ACCEPTED. Implementation status: PLANNED / NOT VERIFIED.**

This is an authoritative *newer design supplement* to the dated Master Specification PDF and Development Phase Plan Draft 0.1. It records decisions made in the SimpleDAW specification conversation. Preserve the original PDFs as historical baselines. Do not interpret these decisions as implemented features or as authorization to change the open v0.22.2 PR.

## 1. GLUE — ACCEPTED
- Inspector **Clip Actions** contains GLUE; it is not a permanent Universal Actions Toolbar (UAT) button.
- Glue combines two or more selected **same-track, same-kind** clips into one new clip. Audio and MIDI are never glued together.
- Gaps become silence in glued audio and empty time in glued MIDI; overlapping audio is mixed in the rendered result and overlapping MIDI notes are retained.
- Audio glue produces a new WAV source without altering the original source files. Glue excludes track effects; the resulting clip continues through the existing track FX chain.
- One Undo restores the original separate clips.
- The glued clip behaves and looks like an ordinary clip; **no special Glue badge** is needed.
- **Interaction with MIX/REPLACE:** overlap playback mode is a separate, nondestructive playback feature. Before implementation, reconcile which rendered signal Glue uses when overlapping sources include REPLACE clips; do not silently override the accepted playback precedence.

## 2. GROUP / UNGROUP — ACCEPTED
- Grouping is **clip editing only**, not track grouping or bus routing.
- Clips may be grouped across tracks and may mix audio and MIDI clip types. Grouping preserves separate clips and contents.
- Grouped clips move, copy, delete and change horizontal start/end boundaries together, retaining relative timing. Do not infer time-stretching of underlying audio.
- To edit an individual clip independently, UNGROUP first.
- GROUP and UNGROUP are undoable. The Inspector **Clip Actions** holds both commands.
- Every group receives a stable automatic number (1, 2, 3, …); all its clips show a small **link icon + group number** on the timeline. Ungrouped clips show no group badge. Numbers are unrelated to track numbers/colors. Group membership is also shown in Inspector.
- A clip may show both group and REPLACE indicators, e.g. link 2 and R.
- **Compatibility checkpoint:** Existing v0.22.2 UAT rules limit multi-clip SIZE to a single selected clip. Group-linked resizing is accepted for a *future clip-group implementation* and does not retroactively expand v0.22.2 scope. Specify exact group resize behavior and validation at that phase's clarification checkpoint.

## 3. PARENT / CHILD TRACKS — ACCEPTED
- One level only: a parent may have children, but a child may not itself be a parent. No nested folders.
- Ordinary audio and MIDI tracks may be organized as children; the protected Drum Master retains its special behavior.
- **Audio children route through the parent audio bus by default**, and must not also reach Master directly (avoid double summing). Parent offers shared volume, pan, mute, solo, EQ/FX. Children retain their individual controls and FX.
- MIDI children retain independent MIDI and instrument routing; do not force raw MIDI through an audio bus. Where appropriate, resulting instrument audio may be routed to the parent bus.
- Parent expands/collapses children; dragging a parent reorders the whole family. All tracks, parents included, count toward the 24-track limit.
- **Track List drag-and-drop** is the primary control: drop a track onto a parent to attach, drag a child out to detach, drag a child within a family to reorder. Show a clear destination preview; reject invalid nested-parent drops.
- Deleting a parent **promotes its children to independent tracks**, preserving their contents. Before implementation, ensure their audio output is safely rerouted rather than orphaned.
- Track hierarchy is independent of clip grouping.

## 4. CLIP OVERLAP PLAYBACK: MIX / REPLACE — ACCEPTED
- Per-audio-clip **MIX / REPLACE** toggle in Inspector Clip Properties; **MIX is default**.
- **MIX:** overlapping audio clips play together (summed).
- **REPLACE:** the winning REPLACE clip plays exclusively in the overlap, suppressing other overlapped clips without deleting or changing them.
- REPLACE always supersedes MIX regardless of clip start order. If multiple REPLACE clips overlap, the **later-starting** clip wins. For identical start times, the **most recently placed** clip wins.
- After the winning REPLACE clip ends, any underlying continuing audio resumes.
- Both modes are nondestructive. A small **R** on the timeline identifies REPLACE; MIX has no badge.
- **Waveform display follows audible playback**: MIX+MIX shows the combined waveform in overlap; REPLACE+MIX shows only the winning REPLACE waveform; REPLACE+REPLACE shows the winning REPLACE waveform; outside overlaps, clips show normal waveforms. Use a subtle boundary where audible precedence switches. This is visualization only: clip objects remain individually selectable and editable.
- Visual rendering of summed waveforms must not imply that the original audio has been destructively joined.

## 5. Planning and acceptance
- **Clip Glue, Clip Group and MIX/REPLACE:** place with Timeline/Clip Editing and Inspector Clip Properties phases.
- **Parent/Child Tracks:** place with Track List/Inspector and routing/mixer phases; coordinate with Drum Master.
- Live acceptance should verify overlapping precedence and restoration, same-track Glue gaps/overlaps/Undo, numbered groups and group editing, drag/drop hierarchy, parent deletion promotion, and no doubled bus audio.
- Design approved here is **not implemented, CI-tested, or Windows live-accepted**. Clarify any remaining engine/API or edge-case conflicts before coding; follow AGENTS.md clarification gate.
