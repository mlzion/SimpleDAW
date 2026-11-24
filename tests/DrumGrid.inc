// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    static bool testDrumGrid(te::Engine& engine)
    {
        const auto check = [](bool ok, const char* message) {
            if (!ok)
                std::fprintf(stderr, "Drum grid: %s\n", message);
            return ok;
        };
        const ju::TemporaryFile editFile(".tracktionedit"), settings(".settings");
        MainGui gui(engine, settings.getFile());
        auto project = te::createEmptyEdit(engine, editFile.getFile());
        auto* master = drumMaster::provision(*project, true);
        if (!master)
            return false;
        gui.setProject(std::move(project), editFile.getFile());
        gui.snap.setSelectedId(1, ju::dontSendNotification);
        gui.selectTrack(0);
        if (!check(gui.insertMidi(1), "one-bar insertion"))
            return false;
        gui.updateControls();
        auto* first = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(first && !gui.drumEditor.isVisible(), "insertion selects without opening the editor"))
            return false;
        gui.openDrumEditor(*first);
        auto model = gui.makeDrumEditor(*first);
        const DrumPattern oneBar(*first, model);
        int oneBarCells = 0;
        gui.forDrumCells(*first, oneBar, *gui.trackAt(1), drumHeaderHeight, drumRowHeight,
                         [&](const DrumCell&) { ++oneBarCells; });
        if (!check(oneBarCells == 16, "one 4/4 bar displays 16 sixteenths"))
            return false;
        gui.closeDrumEditor();

        gui.editCursor = gui.barTime(2);
        gui.menuItemSelected(midiLengthMenuBase + 4, 2);
        auto* clip = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(clip && clip != first && gui.midiInsertBars == 4, "shared selector stores the chosen length"))
            return false;

        auto* lane = gui.trackAt(1);
        const auto cells = [&](te::MidiClip& target) {
            auto editor = gui.makeDrumEditor(target);
            const DrumPattern pattern(target, editor);
            std::vector<DrumCell> result;
            gui.forDrumCells(target, pattern, *lane, drumHeaderHeight, drumRowHeight,
                [&](const DrumCell& cell) { result.push_back(cell); });
            return result;
        };
        const auto event = [&](ju::Point<int> point, ju::Point<int> start, int button, int clicks = 1, ju::Component* surface = nullptr) {
            if (!surface) surface = &gui.drumEditor;
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), point.toFloat(),
                ju::ModifierKeys(button), 0, 0, 0, 0, 0, surface, surface,
                now, start.toFloat(), now, clicks, point != start);
        };
        const auto gesture = [&](ju::Point<int> a, ju::Point<int> b, int button, int clicks = 1) {
            gui.drumEditor.mouseDown(event(a, a, button, clicks));
            if (a != b)
                gui.drumEditor.mouseDrag(event(b, a, button, clicks));
            gui.drumEditor.mouseUp(event(b, a, button, clicks));
        };
        const auto settle = [] {
            ju::MessageManager::getInstance()->runDispatchLoopUntil(ju::MouseEvent::getDoubleClickTimeout() + 50);
        };
        constexpr int left = ju::ModifierKeys::leftButtonModifier;
        constexpr int right = ju::ModifierKeys::rightButtonModifier;
        gui.setViewStart(gui.barTime(1).inSeconds());
        const auto savedView = gui.viewStart;
        const auto savedScale = gui.pixelsPerSecond;
        const auto clipPoint = gui.clipBounds(clip->getPosition().time, gui.rowTop(0), gui.rowHeight(0)).getCentre();
        const auto arrangerClick = [&](ju::Point<int> point, int clicks) {
            const auto mouse = event(point, point, left, clicks, &gui.arrangement);
            gui.arrangement.mouseDown(mouse);
            gui.arrangement.mouseUp(mouse);
            if (clicks > 1)
                gui.arrangement.mouseDoubleClick(mouse);
        };
        arrangerClick(clipPoint, 1);
        if (!check(gui.selectedClip() == clip && !gui.drumEditor.isVisible()
            && gui.viewStart == savedView && gui.pixelsPerSecond == savedScale, "single click preserves the arrangement view"))
            return false;
        arrangerClick(clipPoint, 2);
        if (!check(gui.selectedDrumPattern() == clip && gui.drumGridHasFocus() && !gui.pianoRoll.isVisible(),
                   "double click opens the focused Drum Pattern Editor"))
            return false;
        auto visible = cells(*clip);
        if (!check(visible.size() == 32 && visible.front().step.index == 0 && visible.back().step.index == 31,
                   "four bars initially display 32 steps aligned to the timeline"))
            return false;
        gui.navigateTime(1, true);
        visible = cells(*clip);
        if (!check(visible.size() == 32 && visible.front().step.index == 32 && visible.back().step.index == 63,
                   "horizontal page scroll reveals the remaining 32 steps"))
            return false;
        gui.navigateTime(-1, true);
        visible = cells(*clip);


        const auto cell = visible[0].bounds.getCentre();
        const auto next = visible[1].bounds.getCentre();
        gesture(cell, cell, left);
        gesture(next, next, left);
        settle();
        if (!check(clip->getSequence().getNotes().size() == 2, "rapid single clicks insert both hits"))
            return false;

        gesture(cell, cell, left);
        gesture(cell, cell, left, 2);
        settle();
        if (!check(gui.drumPopup.isVisible() && gui.drumSubdivisionButtons[0].getToggleState()
            && !gui.drumSubdivisionButtons[1].getToggleState(), "double-click opens subdivisions without deleting the hit"))
            return false;
        gui.drumSubdivisionButtons[1].onClick();
        const auto hitState = [&] {
            auto editor = gui.makeDrumEditor(*clip);
            return DrumPattern(*clip, editor).hits({ 36, 0 });
        };
        auto hits = hitState();
        if (!check(hits.size() == 2 && hits[0].length == DrumPattern::subdivision
            && hits[1].start == DrumPattern::subdivision && hits[0].velocity == hits[1].velocity,
            "subdivision splits into standard 64th MIDI hits with the parent velocity"))
            return false;
        gui.useHistory(false);
        if (!check(hitState().size() == 1 && hitState().front().length == DrumPattern::step, "subdivision undo is atomic"))
            return false;
        gui.useHistory(true);

        gesture(cell, cell, right);
        ju::MouseWheelDetails wheel {};
        wheel.deltaX = 0.05f;
        wheel.deltaY = 1.0f;
        gui.drumEditor.mouseWheelMove(event(next, next, 0), wheel);
        if (!check(gui.drumEditor.velocity.getText() == "Velocity 112"
            && hitState()[0].velocity == 112 && hitState()[1].velocity == 112
            && clip->getSequence().getNotes().getLast()->getVelocity() == 100,
            "wheel adjusts only the selected step and all its subdivisions, with numeric feedback"))
            return false;
        gui.useHistory(false);
        if (!check(gui.drumEditor.velocity.getText() == "Velocity 100", "velocity undo restores numeric feedback"))
            return false;
        gui.useHistory(true);
        gesture(cell, next, left);
        wheel.deltaY = -0.1f;
        gui.drumEditor.mouseWheelMove(event(next, next, 0), wheel);
        if (!check(gui.drumSteps.size() == 2 && gui.drumEditor.velocity.getText() == "Velocity 96 - 108"
            && hitState()[0].velocity == 108 && hitState()[1].velocity == 108
            && clip->getSequence().getNotes().getLast()->getVelocity() == 96, "box and wheel adjust the selected group"))
            return false;
        gesture(next, next, right);
        if (!check(gui.drumSteps.size() == 1 && gui.drumSteps.contains({ 36, 0 }), "right-click toggles selection"))
            return false;
        gesture(next, next, left);
        settle();
        if (!check(clip->getSequence().getNotes().size() == 2, "single-click removes the occupied step"))
            return false;
        gui.useHistory(false);
        if (!check(clip->getSequence().getNotes().size() == 3, "single-click removal is undoable"))
            return false;
        const auto laneHeader = ju::Point<int>(15, gui.rowTop(1) + gui.rowHeight(1) / 2);
        arrangerClick(laneHeader, 1);
        if (!check(gui.selectedDrumPattern() == clip && gui.actionTrack() == lane, "lane Inspector preserves the pattern target"))
            return false;

        gui.drumEditor.close.onClick();
        if (!check(!gui.drumEditor.isVisible() && gui.selectedClip() == clip && gui.viewStart == savedView
            && gui.pixelsPerSecond == savedScale && gui.activeEditingArea() == EditingArea::timeline,
            "X returns focus and the selected clip to the unchanged arrangement viewport"))
            return false;
        const auto originalPosition = clip->getPosition();
        const auto correctionAction = [&](UniversalAction action) {
            gui.actionSelection.requestedArea = EditingArea::timeline;
            gui.dispatchAction(action);
        };
        correctionAction(UniversalAction::right);
        if (!check(clip->getPosition().getStart() > originalPosition.getStart() && !gui.drumEditor.isVisible(),
                   "timeline UAT Move works after closing the editor"))
            return false;
        gui.useHistory(false);
        if (!check(clip->getPosition() == originalPosition, "move undo restores position"))
            return false;
        gui.useHistory(true);
        gui.useHistory(false);
        gui.inspectorContext = EditingArea::timeline;
        gui.editCursor = gui.barTime(8);
        gui.copyInspectedClip(true);
        auto* clone = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(clone && clone != clip && gui.cloneMembers(*clip).size() == 2, "clone retains a linked drum source"))
            return false;
        auto cloneEditor = gui.makeDrumEditor(*clone);
        if (!check(DrumPattern(*clone, cloneEditor).velocity({ { 36, 0 } }, 1)
            && hitState().front().velocity == 109, "linked editor mutations propagate"))
            return false;
        gui.useHistory(false);
        if (!check(hitState().front().velocity == 108, "undo restores the linked original"))
            return false;
        gui.useHistory(true);
        if (!check(hitState().front().velocity == 109, "redo re-applies the linked edit"))
            return false;

        gui.toggleInspectorClone();
        if (!check(gui.cloneMembers(*clip).size() == 1 && gui.cloneMembers(*clone).size() == 1,
                   "Unclone separates the linked source"))
            return false;
        gui.useHistory(false);
        if (!check(gui.cloneMembers(*clone).size() == 2, "Unclone undo restores the link"))
            return false;
        gui.useHistory(true);
        gui.editCursor = gui.barTime(14);
        gui.copyInspectedClip(false);
        auto* independent = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(independent && independent != clone && gui.cloneMembers(*independent).size() == 1,
                   "Duplicate creates an independent drum pattern"))
            return false;
        auto independentEditor = gui.makeDrumEditor(*independent);
        if (!check(DrumPattern(*independent, independentEditor).velocity({ { 36, 0 } }, -10)
                   && hitState().front().velocity == 109, "editing a duplicate leaves the original untouched"))
            return false;
        const auto independentID = independent->itemID;
        const auto untrimmed = independent->getPosition().time;
        gui.timeSelection = te::TimeRange(untrimmed.getStart(), untrimmed.getEnd() - te::TimeDuration::fromSeconds(0.5));
        correctionAction(UniversalAction::trim);
        if (!check(independent->getPosition().time == *gui.timeSelection, "timeline Trim works on the drum clip"))
            return false;
        const auto trimmed = independent->getPosition().time;
        correctionAction(UniversalAction::endPlus);
        if (!check(independent->getPosition().getEnd() > trimmed.getEnd(), "timeline Resize extends the drum clip"))
            return false;
        gui.useHistory(false);
        if (!check(independent->getPosition().time == trimmed && gui.setClipLoop(*independent, true, 2),
                   "Resize undo and drum Loop"))
            return false;
        if (!check(independent->isLooping() && independent->getLengthInBeats() > independent->getLoopLengthBeats(),
                   "loop repeats one source"))
            return false;

        gui.addTrack(true);
        gui.editCursor = {};
        if (!check(gui.insertMidi() && !gui.selectedDrumPattern(), "ordinary MIDI uses the same remembered selector"))
            return false;
        auto* ordinary = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(ordinary && std::abs(ordinary->getLengthInBeats().inBeats() - 16.0) < 1.0e-7,
                   "ordinary MIDI insert keeps four complete bars"))
            return false;

        auto& history = gui.edit->getUndoManager();
        ordinary->setMidiChannel(te::MidiChannel(3));
        const auto onset = te::BeatPosition::fromBeats(0.75);
        const auto duration = te::BeatDuration::fromBeats(0.125);
        ordinary->getSequence().addNote(38, onset, duration, 75, 0, &history);
        const auto preserved = [&](const te::MidiClip& transferred) {
            const auto* note = transferred.getSequence().getNotes().getFirst();
            return transferred.getMidiChannel() == te::MidiChannel(10) && note && note->getNoteNumber() == 38
                && note->getVelocity() == 75 && note->getStartBeat() == onset && note->getLengthBeats() == duration;
        };
        gui.editCursor = gui.barTime(20);
        if (!check(gui.snapshotClips({ ordinary }), "copy ordinary MIDI source"))
            return false;
        gui.selectTrack(0);
        gui.pasteClips(gui.pastePlan());
        auto* transferred = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(transferred && preserved(*transferred) && ordinary->getMidiChannel() == te::MidiChannel(3),
                   "ordinary MIDI paste converts channel while preserving the note and source"))
            return false;
        const auto rows = gui.visibleTracks();
        const auto drumRow = rows.indexOf(master);
        const auto sourceRow = rows.indexOf(dynamic_cast<te::AudioTrack*>(ordinary->getTrack()));
        const auto ordinaryID = ordinary->itemID;
        const auto findOrdinary = [&] { return dynamic_cast<te::MidiClip*>(te::findClipForID(*gui.edit, ordinaryID)); };
        const auto changes = gui.movePlan({ ordinary }, drumRow - sourceRow, {});
        if (!check(gui.applyClipChanges(changes, "Move MIDI into Drum Master"), "move ordinary MIDI to drum master"))
            return false;
        auto* moved = findOrdinary();
        if (!check(moved && preserved(*moved) && moved->getTrack() == master,
                   "ordinary MIDI move converts channel 10 and preserves note data"))
            return false;
        gui.useHistory(false);
        auto* reverted = findOrdinary();
        if (!check(reverted && reverted->getTrack() != master && reverted->getMidiChannel() == te::MidiChannel(3),
                   "move undo restores the ordinary MIDI channel"))
            return false;
        gui.useHistory(true);
        if (!check(findOrdinary() && preserved(*findOrdinary()), "move redo restores channel 10"))
            return false;

        gui.selectTrack(0);
        gui.editCursor = gui.barTime(25);
        if (!check(gui.insertMidi(3), "insert a three-bar pattern"))
            return false;
        auto* threeBars = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!threeBars)
            return false;
        gui.openDrumEditor(*threeBars);
        auto threeVisible = cells(*threeBars);
        if (!check(threeVisible.size() == 32 && threeVisible.front().step.index == 0,
                   "three-bar pattern starts with 32 cells"))
            return false;
        gui.navigateTime(1, true);
        threeVisible = cells(*threeBars);
        if (!check(threeVisible.size() == 32 && threeVisible.front().step.index == 16
            && threeVisible.back().step.index == 47, "three-bar scrolling reaches the final sixteenth"))
            return false;
        gui.closeDrumEditor();
        if (!check(gui.save(), "save corrected drum clip operations"))
            return false;
        const auto reopened = te::loadEditFromFile(engine, editFile.getFile());
        auto* savedCopy = reopened ? dynamic_cast<te::MidiClip*>(te::findClipForID(*reopened, independentID)) : nullptr;
        if (!check(savedCopy && savedCopy->isLooping() && savedCopy->getSequence().getNotes().getFirst()
            && savedCopy->getSequence().getNotes().getFirst()->getVelocity() == 99,
            "project reopen preserves independent drum edits and loop"))
            return false;

        gui.edit->tempoSequence.getTimeSigAt(te::TimePosition{}).setStringTimeSig("7/8");
        gui.selectTrack(0);
        gui.editCursor = {};
        if (!check(gui.insertMidi(1), "one bar of 7/8 inserts"))
            return false;
        auto* odd = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!odd)
            return false;
        auto oddEditor = gui.makeDrumEditor(*odd);
        return check(DrumPattern(*odd, oddEditor).count() == 14, "7/8 has fourteen sixteenth-note steps");
    }
