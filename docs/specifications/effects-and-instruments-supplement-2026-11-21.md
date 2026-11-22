# SimpleDAW — Effects and Instruments Supplemental Specification
Version: Draft 0.1 • 2026-11-21
Status: DESIGN DECISIONS ACCEPTED; IMPLEMENTATION NOT AUTHORIZED BY THIS DOCUMENT
Parent: SimpleDAW Master Scope and existing UAT/Inspector design specifications.

## 1. Purpose and precedence
This supplement records decisions from the Effects Reviews and defines the isolated E1 Reverb/Effects Editor foundation. Existing Master Scope, UAT hierarchy, track routing, and Inspector Copy Settings/Apply Settings remain authoritative. If conflicts emerge, flag them for review; do not silently supersede them. ACCEPTED means design approved, not implemented or tested. PENDING and DEFERRED items must not be treated as requirements for E1.

## 2. Effects inventory — ACCEPTED
Essential (8): Equalizer (EQ), Compressor, Limiter, Noise Gate, Reverb, Delay/Echo, Chorus, Distortion/Overdrive.
Optional/later (6): Flanger, Phaser, Pitch Shifter, Tremolo, Stereo Widener, Graphic EQ.
Excluded (1): De-esser.
Additional guitar/bass amp models are ACCEPTED as a category, with exact model lineup PENDING. Other effects may be added in later reviews.

## 3. Track effects architecture — ACCEPTED
- Maximum eight audio-processing effects per track. MIDI instrument and two track LFO modulators are separate and do not consume effect slots.
- Signal processes in displayed order, top to bottom.
- Effects assigned, selected, ordered, individually bypassed, and removed from Track Inspector.
- Reordering by drag/drop and large Up/Down controls.
- Track-wide FX bypass in addition to individual effect bypass.
- Factory and user presets supported as a system goal.
- Copy/Paste **one selected effect at a time**, including its type, model if relevant, all settings/parameter values, preset modifications, and bypass state. Paste makes an independent copy and respects eight-slot limit.
- No separate whole-chain Copy/Paste action: existing Inspector track duplication and Copy Settings/Apply Settings handle whole-track/effects settings; preserve their existing behavior and constraints.
- Audio effects available on audio and MIDI/instrument tracks, after instrument sound generation as applicable.
- Effects Editor is **not** a floating plugin window: it occupies the existing shared bottom slide-up editing panel (alongside Piano Roll, Pattern Editor, etc.). Selecting an effect in Inspector opens that panel and displays its parameters. Editing windows retain established UAT priority.
- Large, clearly labeled controls and numeric values; no dependence on tiny handles or modifier keys.

## 4. Built-in amplifier modeling — ACCEPTED / PENDING
ACCEPTED: built-in Guitar Amp and Bass Amp effect categories; each exposes selectable amp models in the shared bottom editor. At most **one amplifier effect (guitar or bass) per track**. Cabinet simulation integrated into amp workflow with selectable cabinets and bypass; exact controls/IR handling PENDING.
PENDING: precise model lineup and tonal targets; illustrative candidates discussed were British Plexi, American Clean, Modern High Gain, Vintage Bass, Modern Bass, not a finalized list.
ACCEPTED DIRECTION: evaluate porting the existing separate Plexi REAPER JSFX DSP to native C++ rather than assuming JSFX can run natively in Tracktion. Audit source, licensing, DSP equivalence, cabinet processing, performance, and compatibility before committing implementation.
Routing: existing parent/child track routing may be used; note parent amp after child amp is **series** processing, not independent parallel amps. Parallel amps require distinct paths/tracks. No special amp router is specified.

## 5. Built-in MIDI instruments — ACCEPTED / PENDING
ACCEPTED lineup: standard acoustic Piano, electric Organ, simple Analog/subtractive Synth, simple Digital/FM-style Synth. Instrument selection belongs to track Inspector; parameters use shared bottom editor; MIDI input, clip playback and Piano Roll audition feed the instrument on the containing track’s output channel. Instrument audio feeds normal track effects. See the [MIDI track routing amendment](midi-track-routing.md).
ACCEPTED approach: sample-based piano; synthesized organ and synthesizers. Sample licensing, distribution, exact sound library, voices, presets, polyphony, and detailed controls PENDING. Do not assume Tracktion ships a redistributable piano library.

## 6. Track LFO modulation — ACCEPTED
- Two independent LFOs per audio or MIDI/instrument track; these are modulation sources, not audio-effect slots.
- One destination parameter per LFO, selected from compatible track, instrument, or effect parameters.
- Waveforms: sine, triangle, square, sawtooth.
- Rate in Hz or song-tempo-synchronized note divisions.
- Explicit minimum and maximum parameter limits, in destination's natural units (dB, Hz, pan, %, etc.); enable/bypass each LFO.
- Examples: track pan, volume, effect cutoff, reverb wet/dry, amp gain.
- Existing automation must be preserved rather than overwritten by LFO operation.
PENDING: exact automation/LFO combination rule, phase/reset, rate ranges, smoothing, supported destinations, parameter safety/clamping and GUI location details. No automation destructive writes.

## 7. Phase E1 — FIRST IMPLEMENTATION — ACCEPTED
Goal: use **only Reverb** to prove Inspector-to-Effects-Editor-to-audio-engine wiring; no implementation of EQ or other effects yet.
- Track Inspector: add/select/bypass/remove Reverb; show it in chain. Reuse existing Inspector architecture, not a competing interaction system.
- Selecting Reverb automatically opens shared bottom Effects Editor.
- Bottom editor: large sliders with numerical readouts for **Room Size, Decay, Damping, Wet/Dry**.
- Presets: **Small Room, Large Room, Hall, Plate**.
- Reverb audio processing should use actual built-in Tracktion Engine capabilities where available. Codex must inspect pinned engine source/API and document exact available parameters. Map approved controls faithfully; do not fake unsupported DSP parameters. If Plate cannot be meaningfully distinguished with available DSP, report limitation and request decision rather than mislabel.
- Parameters apply during playback; effect bypass restores dry signal; save/reload effect and parameters.
- Architect reusable parameter definitions and editor binding so future effects can be added without rebuilding the editor.
- E1 need not deliver full eight-effect management, additional DSP, amp models, instruments, or LFOs; prepare extensible structure only.

## 8. E1 Windows acceptance criteria — PROPOSED, PENDING USER TEST
E1-01: Add Reverb to a selected audio track through Inspector; correct effect entry appears.
E1-02: Select Reverb; shared bottom panel slides open to Effects Editor without disturbing other editor content/state.
E1-03: Room Size, Decay, Damping, Wet/Dry sliders show numerical values and audibly affect playback.
E1-04: Each factory preset recalls consistent parameter settings; Plate behavior is verified or explicitly flagged.
E1-05: Individual bypass produces unprocessed audio and can be reversed without loss of settings.
E1-06: Remove effect safely; no lingering processing or crash.
E1-07: Save, close, reopen project; effect order/selection as applicable, bypass, parameters, preset state restore.
E1-08: Check audio and MIDI/instrument track compatibility where instrument output is available.
E1-09: Regression test Inspector, UAT, playback/recording, project load, and current Windows x64 packaging.
E1-10: Verify no effect is unintentionally added to other tracks; CPU/stability sanity test.
E1-11: No merges without explicit user approval; separate branch/PR from active v0.22.2 UAT work.

## 9. Implementation workflow — ACCEPTED DIRECTION
Develop E1 in an isolated branch and PR while Windows UAT proceeds separately. Do not alter the active UAT branch. Keep overlap with UAT/Inspector changes minimal; rebase or reconcile against latest main before integration. Windows build and live UAT precede merge approval. Documentation-only PR for this specification should not implement effects.

## 10. Open questions / deferred reviews
PENDING: actual Tracktion effect API inventory; reverb parameter mapping and Plate viability; exact preset parameter values; GUI layout details; LFO+automation arithmetic and bounds; amp and instrument exact DSP/assets/licenses; how user presets are stored; full effect-chain multi-instance behavior.
DEFERRED: Review 6 EQ and detailed controls for compressor, limiter, and other effects until E1 foundation is tested.
