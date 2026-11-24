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
        auto* first = gui.selectedDrumPattern();
        if (!check(first && gui.timeX(first->getPosition().getEnd()) == gui.arrangement.getWidth(), "one bar fills the timeline"))
            return false;
        auto model = gui.makeDrumEditor(*first);
        if (!check(DrumPattern(*first, model).count() == 16, "one 4/4 bar has 16 sixteenths"))
            return false;

        gui.editCursor = gui.barTime(2);
        gui.menuItemSelected(midiLengthMenuBase + 4, 2);
        auto* clip = gui.selectedDrumPattern();
        if (!check(clip && clip != first && gui.midiInsertBars == 4, "shared selector stores the chosen length"))
            return false;

        auto* lane = gui.trackAt(1);
        const auto cells = [&] {
            auto editor = gui.makeDrumEditor(*clip);
            const DrumPattern pattern(*clip, editor);
            std::vector<DrumCell> result;
            gui.forDrumCells(*clip, pattern, *lane, gui.rowTop(1), gui.rowHeight(1),
                [&](const DrumCell& cell) { result.push_back(cell); });
            return result;
        };
        auto visible = cells();
        if (!check(visible.size() == 32 && visible.front().step.index == 0 && visible.back().step.index == 31,
                   "four bars initially display 32 steps aligned to the timeline"))
            return false;
        gui.navigateTime(1, true);
        visible = cells();
        if (!check(visible.size() == 32 && visible.front().step.index == 32 && visible.back().step.index == 63,
                   "horizontal page scroll reveals the remaining 32 steps"))
            return false;
        gui.navigateTime(-1, true);
        visible = cells();

        const auto event = [&](ju::Point<int> point, ju::Point<int> start, int button, int clicks = 1) {
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), point.toFloat(),
                ju::ModifierKeys(button), 0, 0, 0, 0, 0, &gui.arrangement, &gui.arrangement,
                now, start.toFloat(), now, clicks, point != start);
        };
        const auto gesture = [&](ju::Point<int> a, ju::Point<int> b, int button, int clicks = 1) {
            gui.arrangement.mouseDown(event(a, a, button, clicks));
            if (a != b)
                gui.arrangement.mouseDrag(event(b, a, button, clicks));
            gui.arrangement.mouseUp(event(b, a, button, clicks));
        };
        const auto settle = [] {
            ju::MessageManager::getInstance()->runDispatchLoopUntil(ju::MouseEvent::getDoubleClickTimeout() + 50);
        };
        constexpr int left = ju::ModifierKeys::leftButtonModifier;
        constexpr int right = ju::ModifierKeys::rightButtonModifier;
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

        gesture(cell, next, left);
        if (!check(gui.drumSteps.size() == 2 && gui.adjustDrumVelocity(cell, 1.0f)
            && hitState()[0].velocity == 101 && hitState()[1].velocity == 101, "box selection and wheel adjust whole hits"))
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
        gesture(laneHeader, laneHeader, left);
        if (!check(gui.selectedDrumPattern() == clip && gui.actionTrack() == lane, "lane Inspector preserves the pattern target"))
            return false;

        gui.inspectorContext = EditingArea::timeline;
        gui.editCursor = gui.barTime(8);
        gui.copyInspectedClip(true);
        auto* clone = gui.selectedDrumPattern();
        if (!check(clone && clone != clip && gui.cloneMembers(*clip).size() == 2, "clone retains a linked drum source"))
            return false;
        auto cloneEditor = gui.makeDrumEditor(*clone);
        if (!check(DrumPattern(*clone, cloneEditor).velocity({ { 36, 0 } }, 1)
            && hitState().front().velocity == 102, "linked editor mutations propagate"))
            return false;
        gui.useHistory(false);
        if (!check(hitState().front().velocity == 101, "undo restores the linked original"))
            return false;
        gui.useHistory(true);
        if (!check(hitState().front().velocity == 102, "redo re-applies the linked edit"))
            return false;

        gui.addTrack(true);
        gui.editCursor = {};
        if (!check(gui.insertMidi() && !gui.selectedDrumPattern(), "ordinary MIDI uses the same remembered selector"))
            return false;
        auto* ordinary = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(ordinary && std::abs(ordinary->getLengthInBeats().inBeats() - 16.0) < 1.0e-7,
                   "ordinary MIDI insert keeps four complete bars"))
            return false;

        gui.edit->tempoSequence.getTimeSigAt(te::TimePosition{}).setStringTimeSig("7/8");
        gui.selectTrack(0);
        gui.editCursor = {};
        if (!check(gui.insertMidi(1), "one bar of 7/8 inserts"))
            return false;
        auto* odd = gui.selectedDrumPattern();
        if (!odd)
            return false;
        auto oddEditor = gui.makeDrumEditor(*odd);
        return check(DrumPattern(*odd, oddEditor).count() == 14, "7/8 has fourteen sixteenth-note steps");
    }
