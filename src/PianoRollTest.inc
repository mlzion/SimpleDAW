// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

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
        gui.panels.workspaceOpen = false;
        gui.resized();
        const auto seconds = te::TimePosition::fromSeconds<double>;
        auto clip = te::insertMIDIClip(*gui.trackAt(0), "Piano roll fixture", { seconds(1), seconds(5) });
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
        const auto clipPoint = gui.clipBounds(clip->getPosition().time, rulerHeight).getCentre();
        gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, clipPoint, clipPoint, right, 2));
        if (!check(!roll.isVisible(), "right double-click preserves workspace state"))
            return false;
        gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, clipPoint, clipPoint, left, 2));
        if (!check(roll.clip == clip.get() && roll.isVisible() && gui.activeEditingArea() == EditingArea::window,
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
        {
            struct PreviewListener final : te::AudioTrack::Listener
            {
                void injectLiveMidiMessage(te::AudioTrack&, const te::MidiMessageWithSource& message, bool& used) override
                {
                    messages.push_back(message);
                    used = true;
                }
                void recordedMidiMessageSentToPlugins(te::AudioTrack&, const ju::MidiMessage&) override {}
                std::vector<ju::MidiMessage> messages;
            } listener;
            auto& track = *gui.trackAt(0);
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
        gesture(velocityPoint, velocityPoint.translated(0, 12));
        if (!check(notes()[0].velocity < 96, "velocity lane drag updates sequence"))
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
        return true;
    }
