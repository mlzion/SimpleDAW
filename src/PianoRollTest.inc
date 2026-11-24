// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    struct PianoRollPreviewListener final : te::AudioTrack::Listener
    {
        void injectLiveMidiMessage(te::AudioTrack&, const te::MidiMessageWithSource& message, bool& used) override
        {
            messages.push_back(message);
            used = true;
        }
        void recordedMidiMessageSentToPlugins(te::AudioTrack&, const ju::MidiMessage&) override {}
        std::vector<ju::MidiMessage> messages;
    };

    static bool testPianoRoll(te::Engine& engine)
    {
        using Action = UniversalAction;
        const auto directory = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-piano-roll", {});
        if (directory.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { directory.deleteRecursively(); } };
        const auto check = [](bool passed, const char* step) {
            if (!passed)
                std::fprintf(stderr, "Piano Roll: %s\n", step);
            return passed;
        };
        const auto path = directory.getChildFile("notes.tracktionedit");
        MainGui gui(engine, directory.getChildFile("gui.settings"));
        gui.setProject(te::createEmptyEdit(engine, path), path);
        const auto midiRow = gui.trackCount();
        gui.addTrack(true);
        const auto seconds = te::TimePosition::fromSeconds<double>;
        auto clip = te::insertMIDIClip(*gui.trackAt(midiRow), "Piano roll fixture", { seconds(1), seconds(5) });
        if (!clip)
            return false;
        const auto clipID = clip->itemID;
        auto& roll = gui.pianoRoll;
        ju::Component& surface = roll;
        const auto mouse = [](ju::Component& component, ju::Point<int> where, ju::Point<int> down,
                              int button, int clicks = 1) {
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), where.toFloat(),
                ju::ModifierKeys(button), 0, 0, 0, 0, 0, &component, &component,
                now, down.toFloat(), now, clicks, where != down);
        };
        constexpr int left = ju::ModifierKeys::leftButtonModifier;
        constexpr int right = ju::ModifierKeys::rightButtonModifier;
        const auto clipPoint = gui.clipBounds(clip->getPosition().time, gui.rowTop(midiRow), gui.rowHeight(midiRow)).getCentre();
        gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, clipPoint, clipPoint, right, 2));
        if (!check(!roll.isVisible(), "right double-click preserves workspace state"))
            return false;
        gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, clipPoint, clipPoint, left, 2));
        if (!check(roll.clip.get() == clip.get() && roll.isVisible() && gui.activeEditingArea() == EditingArea::window,
                   "MIDI double-click opens and focuses shared workspace"))
            return false;

        const auto at = [&](double beat, int pitch) {
            return ju::Point<int>{ ju::roundToInt(roll.grid.x(beat)), roll.grid.y(pitch) + roll.grid.row / 2 + roll.canvas().getY() };
        };
        const auto gesture = [&](ju::Point<int> from, ju::Point<int> to, int button = ju::ModifierKeys::leftButtonModifier) {
            surface.mouseDown(mouse(surface, from, from, button));
            if (from != to)
                surface.mouseDrag(mouse(surface, to, from, button));
            surface.mouseUp(mouse(surface, to, from, button));
        };
        const auto ruler = at(0.40, 60).withY(roll.canvas().getY() + 7);
        gesture(ruler, ruler);
        if (!check(roll.editCursor == 0.5, "edit cursor snaps to the nearest grid line"))
            return false;
        roll.snap.setSelectedId(1, ju::dontSendNotification);
        gesture(ruler, ruler);
        const auto freeCursor = roll.grid.beat(static_cast<float>(ruler.x));
        if (!check(roll.editCursor == freeCursor, "Snap None keeps the free edit cursor position"))
            return false;
        roll.snap.setSelectedId(4, ju::dontSendNotification);
        roll.updatePlayback(seconds(1.5), false, 0.0);
        if (!check(roll.playbackCursor == 1.0 && roll.editCursor == freeCursor,
                   "playback cursor maps to source beats independently of the edit cursor"))
            return false;
        roll.updatePlayback(seconds(5), false, 0.0);
        if (!check(!roll.playbackCursor, "playback cursor is hidden at the clip end"))
            return false;
        clip->setMuted(true);
        roll.updatePlayback(seconds(2), false, 0.0);
        if (!check(!roll.playbackCursor, "muted MIDI clip has no playback cursor"))
            return false;
        clip->setMuted(false);

        const auto originalGrid = roll.grid;
        const auto timelineScale = gui.pixelsPerSecond;
        const auto timelineStart = gui.viewStart;
        const auto timelineRow = gui.firstRow;
        for (const auto slot : { 5, 3, 7 })
            static_cast<ju::Button*>(gui.navigation[slot])->onClick();
        static_cast<ju::Button*>(gui.pitchNavigation.back())->onClick();
        if (!check(roll.grid.scale > originalGrid.scale && roll.grid.left > originalGrid.left
                && roll.grid.top != originalGrid.top && roll.grid.row > originalGrid.row
                && gui.pixelsPerSecond == timelineScale && gui.viewStart == timelineStart && gui.firstRow == timelineRow,
                "shared navigation sends all four axes and zoom controls to Piano Roll"))
            return false;
        if (!check(gui.clipPulse(clipID, 500.0) == 1.0f && !gui.inspectorPulse(clipID, 500.0),
                   "Piano Roll focus pulses only its bound clip"))
            return false;
        gui.actionSelection.requestedArea = EditingArea::inspector;
        gui.updateControls();
        static_cast<ju::Button*>(gui.navigation[5])->onClick();
        if (!check(gui.pixelsPerSecond > timelineScale && roll.grid.scale > originalGrid.scale
                && !gui.pitchNavigation.front()->isVisible()
                && gui.inspectorPulse(clipID, 500.0) == 1.0f && gui.clipPulse(clipID, 1000.0) == 0.0f,
                "Inspector focus targets Timeline navigation and keeps the original pulse"))
            return false;
        gui.focusPianoRoll();
        if (!check(gui.clipPulse(clipID, 500.0) == 1.0f, "returning focus resumes the edited clip pulse"))
            return false;
        roll.grid = originalGrid;
        roll.updateScrollbars();
        gui.pixelsPerSecond = timelineScale;
        gui.setViewStart(timelineStart);
        roll.updatePlayback(std::nullopt, true, 0.0);
        if (!check(!roll.playbackCursor && roll.followResumeTime == 0.0, "stop clears playback cursor and follow suspension"))
            return false;
        {
            PianoRollPreviewListener listener;
            auto& track = *gui.trackAt(midiRow);
            track.addListener(&listener);
            const ju::ScopeGuard removeListener { [&] { track.removeListener(&listener); } };
            clip->setMidiChannel(te::MidiChannel(5));
            const auto key = at(0.0, 60).withX(30);
            gesture(key, key);
            roll.timerCallback();
            if (!check(listener.messages.size() == 2 && listener.messages[0].isNoteOn()
                    && listener.messages[1].isNoteOff() && listener.messages[0].getChannel() == 5
                    && listener.messages[1].getChannel() == 5 && listener.messages[1].getNoteNumber() == 60,
                    "keyboard audition sends a paired note-on and note-off on the clip channel"))
                return false;

            if (!insertBuiltIn<te::ReverbPlugin>(track, false))
                return false;
            const auto effectID = audioInserts(&track).front()->itemID;
            gesture(key, key);
            if (!check(gui.openEffectsEditor(track.itemID, effectID) && !roll.isVisible()
                    && gui.effectsEditor.isVisible() && !gui.canDispatch(Action::paste)
                    && listener.messages.size() == 4 && listener.messages.back().isNoteOff(),
                    "switching to E1 releases audition and prevents note commands"))
                return false;
            const auto scaleBeforeE1 = gui.pixelsPerSecond;
            const auto pianoScaleBeforeE1 = roll.grid.scale;
            static_cast<ju::Button*>(gui.navigation[5])->onClick();
            if (!check(gui.pixelsPerSecond > scaleBeforeE1 && roll.grid.scale == pianoScaleBeforeE1
                    && !gui.clipPulse(clipID, 500.0), "E1 focus navigates the Timeline without a Piano Roll pulse"))
                return false;
            gui.pixelsPerSecond = scaleBeforeE1;
            gui.setViewStart(timelineStart);
            gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, clipPoint, clipPoint, left, 2));
            if (!check(roll.isVisible() && !gui.effectsEditor.isVisible() && gui.effectsEditor.isOpen()
                    && gui.actionSelection.track == track.itemID && gui.actionSelection.clips.contains(clipID),
                    "Piano Roll suspends E1 and leaves independent selections in place"))
                return false;
            gesture(key, key);
            gui.play();
            if (!check(listener.messages.size() == 6 && listener.messages.back().isNoteOff()
                    && listener.messages.back().getChannel() == 5, "starting playback releases guide notes"))
                return false;
            gui.stop();
            gui.updateControls();
        }

        MidiNoteEditor model(*clip);
        const auto notes = [&] { return model.notes(); };
        const auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
        gesture(at(0.1, 60), at(0.5, 60));
        gesture(at(1.1, 64), at(1.1, 64));
        if (!check(notes().size() == 2 && near(notes()[0].start, 0) && near(notes()[1].start, 1)
                && near(notes()[0].length, 0.5) && near(notes()[1].length, 0.5),
                "snapped drawing and remembered length"))
            return false;

        gesture(at(0.1, 60), at(0.1, 60), right);
        if (!check(roll.selectedNotes().size() == 2 && gui.canDispatch(Action::right) && !gui.canDispatch(Action::endPlus),
                   "right click adds a note to UAT selection and disables group sizing"))
            return false;
        const auto original = notes();
        auto* midiTrack = gui.trackAt(midiRow);
        const auto effectID = audioInserts(midiTrack).front()->itemID;
        if (!check(gui.openEffectsEditor(midiTrack->itemID, effectID) && !gui.canDispatch(Action::duplicate),
                   "E1 focus cannot duplicate selected notes"))
            return false;
        gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, clipPoint, clipPoint, left, 2));
        if (!check(roll.selectedNotes().size() == 2 && gui.canDispatch(Action::duplicate),
                   "switching back preserves the note selection and restores its commands"))
            return false;
        if (!check(!gui.canDispatch(Action::left), "an out-of-range move is rejected for the whole group"))
            return false;
        gui.actionSelection.clips = { clipID };
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.updateControls();
        if (!check(gui.canDispatch(Action::endPlus) && roll.selectedNotes().size() == 2,
                   "Timeline focus retains the independent note selection"))
            return false;
        gui.actionSelection.requestedArea = EditingArea::window;
        gui.updateControls();
        gui.dispatchAction(Action::right);
        if (!check(near(notes()[0].start, 0.25) && near(notes()[1].start, 1.25), "atomic UAT group movement"))
            return false;
        gui.useHistory(false);
        if (!check(notes() == original, "one Undo restores both notes"))
            return false;
        gui.useHistory(true);
        gesture(at(0.70, 60), at(0.70, 60));
        gesture(at(0.74, 60), at(0.99, 60));
        if (!check(near(notes()[0].length, 0.75), "trailing-edge drag resizes a single note"))
            return false;

        const auto velocityPoint = roll.velocityHandle(notes()[0]).getCentre().roundToInt() + roll.canvas().getPosition();
        const auto laneEnd = velocityPoint.translated(0, 12);
        surface.mouseDown(mouse(surface, velocityPoint, velocityPoint, left));
        surface.mouseDrag(mouse(surface, laneEnd, velocityPoint, left));
        if (!check(roll.velocityReadout && roll.velocityReadout->value < 96, "velocity lane displays its live preview"))
            return false;
        surface.mouseUp(mouse(surface, laneEnd, velocityPoint, left));
        if (!check(notes()[0].velocity < 96 && !roll.velocityReadout, "velocity lane commits and clears its readout"))
            return false;
        gui.useHistory(false);
        if (!check(notes()[0].velocity == 96, "velocity drag is undoable"))
            return false;
        roll.velocity.setValue(72, ju::sendNotificationSync);
        if (!check(notes()[0].velocity == 72, "numeric velocity edits the selected note"))
            return false;
        roll.mode.setToggleState(true, ju::dontSendNotification);
        gesture(at(0.1, 64), at(1.8, 59));
        if (!check(roll.selectedNotes().size() == 2, "Select mode marquee selects intersecting note blocks"))
            return false;
        gesture(at(0.4, 60), at(0.65, 59));
        if (!check(near(notes()[0].start, 0.5) && notes()[0].pitch == 59
                && near(notes()[1].start, 1.5) && notes()[1].pitch == 63,
                "group drag preserves relative time and pitch"))
            return false;
        gui.useHistory(false);

        gesture(at(0.4, 60), at(0.4, 60));
        gui.dispatchAction(Action::cut);
        if (!check(notes().size() == 1, "cut removes the note"))
            return false;
        const auto pastePoint = at(2.05, 60).withY(roll.canvas().getY() + 7);
        gesture(pastePoint, pastePoint);
        gui.dispatchAction(Action::paste);
        if (!check(notes().size() == 2 && near(notes()[1].start, 2)
                && near(notes()[1].length, 0.75) && notes()[1].pitch == 60 && notes()[1].velocity == 72,
                "paste uses the snapped ruler cursor and retains the copied note data"))
            return false;
        gui.dispatchAction(Action::remove);
        if (!check(notes().size() == 1, "UAT delete removes the selected note"))
            return false;
        gui.useHistory(false);

        const auto editedPoint = at(1.35, 64);
        const auto beforeVelocityDrag = roll.selected;
        const auto priorVelocity = notes()[0].velocity;
        const auto rightEnd = editedPoint.translated(0, 16);
        surface.mouseDown(mouse(surface, editedPoint, editedPoint, right));
        surface.mouseDrag(mouse(surface, rightEnd, editedPoint, right));
        if (!check(roll.velocityReadout && roll.velocityReadout->value == priorVelocity - 16
                && !roll.velocityReadoutBounds().toFloat().intersects(roll.grid.bounds(roll.clicked)),
                "right-drag readout follows velocity and avoids the edited note"))
            return false;
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.updateControls();
        if (!check(!roll.velocityReadout && notes()[0].velocity == priorVelocity,
                   "losing focus cancels the readout and the pending change"))
            return false;
        gesture(editedPoint, rightEnd, right);
        if (!check(notes()[0].velocity == priorVelocity - 16 && roll.selected == beforeVelocityDrag && !roll.velocityReadout,
                   "right-drag adjusts the note without toggling selection"))
            return false;
        roll.mode.setToggleState(false, ju::dontSendNotification);
        gesture(at(3.1, 67), at(3.1, 67));
        if (!check(notes().back().velocity == priorVelocity - 16, "draw uses last manually adjusted velocity"))
            return false;
        gui.useHistory(false);

        const auto group = notes();
        roll.selected = { group[0].id, group[1].id };
        if (!check(gui.canDispatch(Action::duplicate), "UAT Duplicate accepts a group with room to the right"))
            return false;
        gui.dispatchAction(Action::duplicate);
        const auto copies = roll.selectedNotes();
        const auto shift = group.back().end() - group.front().start;
        if (!check(copies.size() == 2 && near(copies[0].start, group[0].start + shift)
                && near(copies[1].start, group[1].start + shift)
                && copies[0].pitch == group[0].pitch && copies[1].velocity == group[1].velocity
                && near(copies[1].length, group[1].length) && notes().size() == 4,
                "Duplicate shifts by the group span and selects the copies"))
            return false;
        gui.useHistory(false);
        if (!check(notes() == group, "one Undo removes the full duplicate group"))
            return false;

        const auto saved = notes();
        if (!check(saved.size() == 2 && te::EditFileOperations(*gui.edit).save(false, true, false),
                   "undo delete and save edited project"))
            return false;
        clip = nullptr;
        gui.setProject(te::loadEditFromFile(engine, path), path);
        auto* reopened = dynamic_cast<te::MidiClip*>(te::findClipForID(*gui.edit, clipID));
        if (!check(reopened != nullptr, "reopen original MIDI clip"))
            return false;
        const auto restored = MidiNoteEditor(*reopened).notes();
        if (restored.size() != saved.size())
            return check(false, "reopened note count");
        for (size_t i = 0; i < saved.size(); ++i)
            if (!check(restored[i].pitch == saved[i].pitch && restored[i].velocity == saved[i].velocity
                    && near(restored[i].start, saved[i].start) && near(restored[i].length, saved[i].length),
                    "reopened note pitch, velocity, start and duration"))
                return false;
        return testLinkedPianoRoll(gui, *reopened, midiRow);
    }

    static bool testLinkedPianoRoll(MainGui& gui, te::MidiClip& parent, int midiRow)
    {
        const auto check = [](bool passed, const char* step) {
            if (!passed)
                std::fprintf(stderr, "Linked Piano Roll: %s\n", step);
            return passed;
        };
        const auto beat = te::BeatPosition::fromBeats<double>;
        auto& parentSequence = parent.getSequence();
        constexpr int controller = 11, value = 7500;
        parentSequence.addControllerEvent(beat(2.5), controller, value, nullptr);
        gui.addTrack(true);
        gui.actionSelection.clips = { parent.itemID };
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.editCursor = parent.getPosition().getEnd() + te::TimeDuration::fromSeconds(1);
        gui.updateControls();
        gui.copyInspectedClip(true);
        auto* clone = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(clone && gui.cloneMembers(*clone).size() == 2, "create linked MIDI clips"))
            return false;
        const auto moved = gui.movePlan({ clone }, 1, {});
        if (!check(gui.applyClipChanges(moved, "Move linked clip") && clone->getTrack() == gui.trackAt(midiRow + 1),
                   "linked member moves to another MIDI track"))
            return false;

        auto& roll = gui.pianoRoll;
        gui.pianoRollClip = clone->itemID;
        roll.setVisible(true);
        gui.resized();
        gui.focusPianoRoll();
        auto& clonedSequence = clone->getSequence();
        const auto nonNotesIntact = [&](te::MidiClip& clip) {
            auto& sequence = clip.getSequence();
            auto* event = sequence.getControllerEventAt(beat(2.5), controller);
            return sequence.getNumControllerEvents() == 1 && event && event->getControllerValue() == value;
        };

        const auto initial = MidiNoteEditor(*clone).notes();
        roll.selected = { initial.front().id };
        gui.dispatchAction(UniversalAction::up);
        if (!check(MidiNoteEditor(parent).notes().front().pitch == initial.front().pitch + 1
                && MidiNoteEditor(*clone).notes().front().pitch == initial.front().pitch + 1
                && nonNotesIntact(parent) && nonNotesIntact(*clone), "note edits follow the family and preserve controller data"))
            return false;
        gui.useHistory(false);
        if (!check(MidiNoteEditor(parent).notes().front().pitch == initial.front().pitch
                && MidiNoteEditor(*clone).notes() == initial, "one Undo restores both sequences"))
            return false;
        gui.useHistory(true);
        if (!check(MidiNoteEditor(parent).notes().front().pitch == initial.front().pitch + 1
                && MidiNoteEditor(*clone).notes().front().pitch == initial.front().pitch + 1,
                "one Redo restores both edits"))
            return false;

        gui.dispatchAction(UniversalAction::duplicate);
        if (!check(parentSequence.getNotes().size() == 3 && clonedSequence.getNotes().size() == 3,
                   "linked Duplicate inserts into each member"))
            return false;
        gui.useHistory(false);
        if (!check(parentSequence.getNotes().size() == 2 && clonedSequence.getNotes().size() == 2,
                   "one Undo removes all linked copies"))
            return false;
        roll.selected = { MidiNoteEditor(*clone).notes().front().id };

        const auto cloneID = clone->itemID;
        const auto parentID = parent.itemID;
        const auto path = gui.projectFile;
        if (!check(gui.save(), "save linked note changes"))
            return false;
        auto reopened = te::loadEditFromFile(gui.engine, path);
        auto* savedParent = reopened ? dynamic_cast<te::MidiClip*>(te::findClipForID(*reopened, parentID)) : nullptr;
        auto* savedClone = reopened ? dynamic_cast<te::MidiClip*>(te::findClipForID(*reopened, cloneID)) : nullptr;
        if (!check(savedParent && savedClone && nonNotesIntact(*savedParent) && nonNotesIntact(*savedClone)
                && MidiNoteEditor(*savedParent).notes().front().pitch == initial.front().pitch + 1
                && MidiNoteEditor(*savedClone).notes().front().pitch == initial.front().pitch + 1,
                "reopen preserves the edited family and its controller data"))
            return false;

        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.updateControls();
        gui.toggleInspectorClone();
        gui.focusPianoRoll();
        gui.dispatchAction(UniversalAction::remove);
        if (!check(gui.cloneMembers(*clone).size() == 1 && clonedSequence.getNotes().size() == 1
                && parentSequence.getNotes().size() == 2, "Unclone gives subsequent note edits an independent source"))
            return false;
        gui.useHistory(false);
        gui.useHistory(false);
        gui.edit->getUndoManager().beginNewTransaction("Remove parent");
        parent.removeFromParent();
        gui.updateControls();
        roll.selected = { MidiNoteEditor(*clone).notes().front().id };
        gui.dispatchAction(UniversalAction::down);
        if (!check(gui.cloneMembers(*clone).size() == 1
                && MidiNoteEditor(*clone).notes().front().pitch == initial.front().pitch,
                "promoted member remains editable when its parent is deleted"))
            return false;

        clonedSequence.addNote(44, beat(0.25), te::BeatDuration::fromBeats(0.25), 96, 0, nullptr);
        const auto oldRange = clone->getPosition().time;
        const auto trim = te::TimeDuration::fromSeconds(0.5);
        clone->setPosition({ { oldRange.getStart() + trim, oldRange.getEnd() - trim }, trim });
        auto model = roll.mutation();
        const auto range = model.range();
        const GridNote hidden { {}, range.getStart() - 0.125, 0.125, 45, 96 };
        const GridNote crossing { {}, range.getEnd() - 0.125, 0.25, 46, 96 };
        const auto oldNotes = model.notes();
        if (!check(model.insert({ hidden }, "Outside trim").empty() && model.insert({ crossing }, "Cross trim").empty()
                && oldNotes.front().pitch == 44 && oldNotes.front().end() <= range.getStart()
                && model.notes() == oldNotes, "trimmed range rejects out-of-bounds additions without removing hidden notes"))
            return false;

        if (!check(gui.setClipLoop(*clone, true, 3), "enable native MIDI looping"))
            return false;
        auto loopModel = roll.mutation();
        const auto loop = loopModel.range();
        const auto start = clone->getStartBeat();
        const auto atBeat = [&](double delta) { return gui.edit->tempoSequence.toTime(start + te::BeatDuration::fromBeats(delta)); };
        const auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
        roll.updatePlayback(atBeat(loop.getLength() + 0.125), false, 0.0);
        if (!check(roll.playbackCursor && near(*roll.playbackCursor, loop.getStart() + 0.125)
                && gui.clipPulse(clone->itemID, 500.0) == 1.0f && !gui.clipPulse(parentID, 500.0),
                "loop playback wraps into the source range and pulse transfers to the edited member"))
            return false;

        const auto previousGrid = roll.grid;
        roll.grid.scale = 640;
        roll.grid.left = loop.getStart();
        const auto farBeat = loop.getLength() * 0.75;
        gui.editCursor = atBeat(0.0);
        gui.play();
        gui.navigateTime(1, true);
        const auto manuallyScrolled = roll.grid.left;
        const auto resumes = roll.followResumeTime;
        roll.updatePlayback(atBeat(farBeat), true, resumes - 1.0);
        if (!check(resumes > 0.0 && roll.grid.left == manuallyScrolled, "manual navigation temporarily suspends follow"))
            return false;
        roll.updatePlayback(atBeat(farBeat), true, resumes);
        if (!check(roll.grid.left > manuallyScrolled && loop.getStart() + farBeat < roll.grid.left + roll.visibleBeats(),
                   "follow resumes and reveals the advancing playback cursor"))
            return false;
        gui.stop();
        gui.updateControls();
        roll.grid = previousGrid;
        roll.updateScrollbars();

        const GridNote inSource { {}, loop.getStart(), 0.125, 48, 96 };
        const GridNote repetition { {}, loop.getEnd(), 0.125, 48, 96 };
        if (!check(loopModel.insert({ inSource, repetition }, "Outside source loop").empty()
                && loopModel.notes() == oldNotes && loopModel.insert({ inSource }, "Inside source loop").size() == 1
                && nonNotesIntact(*clone), "loop bounds are atomic and only the source iteration is editable"))
            return false;

        const GridNote last { {}, loop.getEnd() - 0.125, 0.125, 62, 96 };
        roll.selected = loopModel.insert({ last }, "Draw at loop boundary");
        const auto beforeDuplicate = loopModel.notes();
        gui.dispatchAction(UniversalAction::duplicate);
        if (!check(roll.selected.size() == 1 && !gui.canDispatch(UniversalAction::duplicate)
                && loopModel.notes() == beforeDuplicate, "Duplicate rejects the entire selection at the loop boundary"))
            return false;

        PianoRollPreviewListener listener;
        auto* previewTrack = gui.trackAt(midiRow + 1);
        auto* previewEdit = gui.edit.get();
        previewTrack->addListener(&listener);
        const ju::ScopeGuard removeListener { [&] {
            if (gui.edit.get() == previewEdit)
                previewTrack->removeListener(&listener);
        } };
        roll.audition(60, 96);
        gui.edit->getUndoManager().beginNewTransaction("Delete edited clip");
        clone->removeFromParent();
        gui.updateControls();
        if (!check(!roll.isVisible() && !gui.clipPulse(cloneID, 500.0) && !roll.playbackCursor
                && listener.messages.size() == 2 && listener.messages.back().isNoteOff(),
                   "deleting the target closes Piano Roll and releases audition"))
            return false;
        gui.useHistory(false);
        gui.pianoRollClip = cloneID;
        roll.setVisible(true);
        gui.resized();
        gui.focusPianoRoll();
        roll.audition(64, 96);
        gui.setProject(std::move(reopened), path);
        return check(listener.messages.size() == 4 && listener.messages.back().isNoteOff()
                     && listener.messages.back().getNoteNumber() == 64
                     && !roll.isVisible() && roll.clip == nullptr && !gui.effectsEditor.isOpen()
                     && !gui.clipPulse(cloneID, 500.0) && !roll.playbackCursor
                     && roll.rememberedVelocity == 96, "project replacement clears editor bindings and gesture state");
    }
