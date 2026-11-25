// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    static bool testContextToolbar(te::Engine& engine)
    {
        using EditAction = ContextToolbar::Edit;
        const auto check = [](bool passed, const char* step) {
            if (!passed)
                std::fprintf(stderr, "Context Toolbar: %s\n", step);
            return passed;
        };
        auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-context-toolbar", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        MainGui gui(engine, dir.getChildFile("gui.settings"));
        gui.setVisible(true);
        gui.setSize(1200, 720);
        const auto project = dir.getChildFile("test.tracktionedit");
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.edit->ensureNumberOfAudioTracks(2);
        auto* track = gui.trackAt(0);
        track->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        const auto seconds = te::TimePosition::fromSeconds<double>;
        const auto first = te::insertMIDIClip(*track, "First", { seconds(1), seconds(2) });
        const auto second = te::insertMIDIClip(*track, "Second", { seconds(3), seconds(4) });
        if (!check(first && second, "create clips"))
            return false;

        auto& popup = gui.contextToolbar;
        const auto mouse = [](ju::Component& component, ju::Point<int> point, int modifiers) {
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), point.toFloat(),
                ju::ModifierKeys(modifiers), 0, 0, 0, 0, 0, &component, &component, now,
                point.toFloat(), now, 1, false);
        };
        const auto middle = ju::ModifierKeys::middleButtonModifier;
        const auto left = ju::ModifierKeys::leftButtonModifier;
        const auto right = ju::ModifierKeys::rightButtonModifier;
        const auto open = [&] {
            auto click = mouse(gui.arrangement, { 25, 12 }, middle);
            gui.arrangement.mouseDown(click);
            gui.arrangement.mouseUp(click);
            return popup.isVisible() && popup.page == 0;
        };
        const auto editGroupVisible = [&](bool expected) {
            const auto& group = gui.actionGroups.front();
            return group.frame.isVisible() == expected
                && std::all_of(group.buttons.begin(), group.buttons.end(),
                    [expected](auto* button) { return button->isVisible() == expected; });
        };
        const auto toggleViaMenu = [&](bool currentlyEnabled) {
            const auto menu = gui.getMenuForIndex(3, "OPTIONS");
            ju::PopupMenu::MenuItemIterator items(menu);
            while (items.next())
            {
                const auto& item = items.getItem();
                if (item.text == "Experimental Context UI" && item.isTicked == currentlyEnabled && item.action)
                {
                    item.action();
                    return true;
                }
            }
            return false;
        };
        const auto escape = [&] { return gui.keyPressed(ju::KeyPress(ju::KeyPress::escapeKey), &gui); };
        const auto invoke = [&](EditAction action) {
            const auto index = std::find(ContextToolbar::edits.begin(), ContextToolbar::edits.end(), action)
                - ContextToolbar::edits.begin();
            popup.buttons[static_cast<size_t>(index)].onClick();
        };
        const auto availabilityMatchesUAT = [&] {
            for (size_t i = 0; i < ContextToolbar::edits.size(); ++i)
                if (popup.buttons[i].isEnabled() != gui.canDispatch(contextAction(ContextToolbar::edits[i])))
                    return false;
            return true;
        };

        gui.selection = 0;
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.actionSelection.clips = { first->itemID, second->itemID };
        gui.editCursor = seconds(6);
        gui.updateControls();
        const auto originalSelection = gui.actionSelection.clips;
        if (!check(editGroupVisible(true) && !open() && !popup.isVisible(), "classic toolbar and middle click"))
            return false;

        if (!check(toggleViaMenu(false), "Options offers the unchecked experimental mode"))
            return false;
        const auto focus = ju::Component::getCurrentlyFocusedComponent();
        if (!check(editGroupVisible(false) && gui.actionGroups[1].frame.isVisible() && open()
            && gui.activeEditingArea() == EditingArea::timeline && gui.actionSelection.clips == originalSelection
            && gui.editCursor == seconds(6) && ju::Component::getCurrentlyFocusedComponent() == focus
            && availabilityMatchesUAT() && !popup.buttons.back().isEnabled(), "opening over a track keeps timeline context"))
            return false;
        const auto anchor = popup.panel;
        if (!check(gui.getComponentAt(1, 1) == &popup
            && popup.isParentOf(gui.getComponentAt(popup.buttons.front().getBounds().getCentre())),
            "popup receives input both on its buttons and outside its panel"))
            return false;
        const auto viewStart = gui.viewStart;
        const auto row = gui.firstRow;
        const auto wheel = [&](float delta, bool overButton) {
            const ju::MouseWheelDetails details { 0.2f, delta, false, false, false };
            const auto point = popup.panel.getCentre();
            if (overButton)
            {
                auto& button = popup.buttons.front();
                static_cast<ju::Component&>(button).mouseWheelMove(mouse(button, { 5, 5 }, 0), details);
            }
            else
            {
                popup.mouseWheelMove(mouse(popup, point, 0), details);
            }
        };
        wheel(1, true);
        if (!check(popup.page == 1 && !popup.buttons.front().isVisible(), "up wraps from Edit to test page"))
            return false;
        wheel(-1, false);
        if (!check(popup.page == 0 && popup.buttons.front().isVisible(), "down wraps to Edit"))
            return false;
        wheel(-1, true);
        if (!check(popup.page == 1, "down advances from Edit to test page"))
            return false;
        wheel(1, false);
        if (!check(popup.page == 0 && popup.panel == anchor && gui.firstRow == row
            && gui.viewStart == viewStart, "up returns to Edit at stable anchor without arrangement scroll"))
            return false;

        const ju::Rectangle<int> workArea { 40, 35, 600, 400 };
        for (auto pointer : { workArea.getTopLeft(), workArea.getTopRight(),
                              workArea.getBottomLeft(), workArea.getBottomRight() })
        {
            popup.open(pointer, workArea);
            if (!check(workArea.contains(popup.panel) && popup.panel.getWidth() == anchor.getWidth(),
                       "panel clamps at each screen edge"))
                return false;
        }
        for (const auto modifiers : { left, right, middle })
        {
            const auto point = modifiers == left ? ju::Point<int> { 1, 1 } : popup.panel.getCentre();
            auto click = mouse(popup, point, modifiers);
            popup.mouseDown(click);
            popup.mouseUp(click);
            if (!check(!popup.isVisible() && !gui.contextTarget && gui.actionSelection.clips == originalSelection
                && gui.editCursor == seconds(6) && open(), "outside, right and middle dismiss without editing"))
                return false;
        }
        if (!check(escape() && !popup.isVisible() && !escape() && open(), "Escape dismisses only an open popup"))
            return false;

        invoke(EditAction::copy);
        if (!check(!popup.isVisible() && gui.actionSelection.clips == originalSelection && open()
            && popup.buttons.back().isEnabled(), "popup Copy populates the UAT clipboard"))
            return false;
        invoke(EditAction::paste);
        if (!check(track->getClips().size() == 4, "popup Paste pastes both clips"))
            return false;
        gui.useHistory(false);
        if (!check(track->getClips().size() == 2, "UAT Undo undoes popup Paste"))
            return false;
        gui.useHistory(true);
        if (!check(track->getClips().size() == 4, "UAT Redo restores popup Paste"))
            return false;
        gui.useHistory(false);
        gui.actionSelection.clips = originalSelection;
        if (!open())
            return false;
        gui.actionSelection.clips = { second->itemID };
        invoke(EditAction::remove);
        if (!check(!popup.isVisible() && track->getClips().size() == 2, "changed selection blocks stale Delete"))
            return false;
        gui.actionSelection.clips = originalSelection;
        if (!open())
            return false;
        invoke(EditAction::cut);
        if (!check(track->getClips().isEmpty() && gui.clipClipboard && gui.clipClipboard->clips.size() == 2,
            "popup Cut uses the UAT clipboard"))
            return false;
        gui.useHistory(false);
        if (!check(track->getClips().size() == 2, "UAT Undo restores popup Cut"))
            return false;

        gui.selectTrack(0);
        if (!check(open() && availabilityMatchesUAT() && !popup.buttons.front().isEnabled(),
            "Track List disables Cut"))
            return false;
        invoke(EditAction::cut);
        if (!check(track->getClips().size() == 2 && open(), "disabled dispatch cannot mutate tracks"))
            return false;
        invoke(EditAction::copy);
        gui.selectTrack(1);
        if (!check(open() && popup.buttons.back().isEnabled() && availabilityMatchesUAT(), "Track List clipboard"))
            return false;
        invoke(EditAction::paste);
        if (!check(gui.trackCount() == 3, "popup Paste duplicates a track below the anchor"))
            return false;

        gui.mouseDown(mouse(gui.inspector, { 10, 10 }, left));
        gui.mouseDown(mouse(gui.inspector, { 10, 10 }, middle));
        if (!check(popup.isVisible() && availabilityMatchesUAT()
            && std::none_of(popup.buttons.begin(), popup.buttons.end(), [](const auto& button) { return button.isEnabled(); })
            && escape(), "Inspector context leaves all Edit actions disabled"))
            return false;

        auto* midi = dynamic_cast<te::MidiClip*>(track->getClips().getFirst());
        if (!midi)
            return false;
        gui.pianoRollClip = midi->itemID;
        gui.pianoRoll.setVisible(true);
        gui.focusPianoRoll();
        const GridNote note { {}, 0, 0.25, 60, 96 };
        gui.pianoRoll.selected = MidiNoteEditor(*midi).insert({ note }, "Context toolbar test note");
        gui.updateControls();
        const auto middleOnPianoRoll = mouse(gui.pianoRoll, { 100, 70 }, middle);
        gui.mouseDown(middleOnPianoRoll);
        if (!check(!gui.pianoRoll.selected.empty() && popup.isVisible() && availabilityMatchesUAT(), "Piano Roll availability"))
            return false;
        invoke(EditAction::copy);
        if (!check(gui.pianoRoll.clipboard.size() == 1 && open() && popup.buttons.back().isEnabled(),
            "Piano Roll uses its own clipboard"))
            return false;
        const auto noteWheel = mouse(popup, popup.panel.getCentre(), 0);
        const auto pitch = gui.pianoRoll.grid.top;
        const auto velocity = gui.pianoRoll.rememberedVelocity;
        popup.mouseWheelMove(noteWheel, { 0, -1, false, false, false });
        if (!check(gui.pianoRoll.grid.top == pitch && gui.pianoRoll.rememberedVelocity == velocity,
            "popup consumes Piano Roll wheel input"))
            return false;
        popup.mouseWheelMove(noteWheel, { 0, 1, false, false, false });
        invoke(EditAction::remove);
        if (!check(MidiNoteEditor(*midi).notes().empty(), "Piano Roll Delete routes to notes"))
            return false;
        gui.useHistory(false);
        if (!check(MidiNoteEditor(*midi).notes().size() == 1 && open(), "UAT Undo restores popup note Delete"))
            return false;

        if (!check(toggleViaMenu(true), "Options marks experimental mode as enabled"))
            return false;
        return check(!popup.isVisible() && editGroupVisible(true) && !open(), "classic restoration closes popup")
            && testDrumContextToolbar(engine);
    }

    static bool testDrumContextToolbar(te::Engine& engine)
    {
        using Edit = ContextToolbar::Edit;
        const auto check = [](bool ok, const char* step) {
            if (!ok)
                std::fprintf(stderr, "Drum Context Toolbar: %s\n", step);
            return ok;
        };
        const ju::TemporaryFile path(".tracktionedit"), settingsFile(".settings");
        MainGui gui(engine, settingsFile.getFile());
        gui.setVisible(true);
        auto project = te::createEmptyEdit(engine, path.getFile());
        auto* master = drumMaster::provision(*project, true);
        if (!master)
            return false;
        gui.setProject(std::move(project), path.getFile());
        gui.selectTrack(0);
        if (!gui.insertMidi(4))
            return false;
        auto* clip = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!clip)
            return false;
        const auto clipID = clip->itemID;
        const auto notes = [&] { return gui.makeDrumEditor(*clip).notes(); };
        auto model = gui.makeDrumEditor(*clip);
        const auto inserted = model.insert({ { {}, 0.0, 0.0625, 36, 84 }, { {}, 0.0625, 0.0625, 36, 84 },
                                             { {}, 0.25, 0.25, 38, 63 } }, "Context test drum hits");
        if (!check(inserted.size() == 3, "create selected hits, including subdivisions"))
            return false;
        auto& popup = gui.contextToolbar;
        const auto mouse = [](ju::Component& component, ju::Point<int> point, int buttons = 0) {
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), point.toFloat(),
                ju::ModifierKeys(buttons), 0, 0, 0, 0, 0, &component, &component, now, point.toFloat(), now, 1, false);
        };
        const auto middle = ju::ModifierKeys::middleButtonModifier;
        const auto openTrack = [&] {
            gui.arrangement.mouseDown(mouse(gui.arrangement, { 30, 60 }, middle));
            return popup.isVisible();
        };
        const auto drumPoint = ju::Point<int>(drumNameWidth + 15, drumHeaderHeight + 10);
        const auto openDrum = [&] {
            auto event = mouse(gui.drumEditor, drumPoint, middle);
            gui.drumEditor.mouseDown(event);
            gui.drumEditor.mouseUp(event);
            return popup.isVisible();
        };
        const auto escape = [&] { return gui.keyPressed(ju::KeyPress(ju::KeyPress::escapeKey), &gui.drumEditor); };
        const auto invoke = [&](Edit action) {
            const auto i = std::find(ContextToolbar::edits.begin(), ContextToolbar::edits.end(), action)
                - ContextToolbar::edits.begin();
            popup.buttons[static_cast<size_t>(i)].onClick();
        };
        const auto matchesUAT = [&] {
            for (size_t i = 0; i < ContextToolbar::edits.size(); ++i)
                if (popup.buttons[i].isEnabled() != gui.canDispatch(contextAction(ContextToolbar::edits[i])))
                    return false;
            return true;
        };

        gui.setContextMode(true);
        gui.selectTrack(0);
        if (!check(openTrack() && matchesUAT()
            && std::none_of(popup.buttons.begin(), popup.buttons.end(), [](const auto& b) { return b.isEnabled(); }),
            "Master track stays protected across all Edit actions"))
            return false;
        invoke(Edit::remove);
        if (!check(drumMaster::find(*gui.edit) == master, "disabled Delete preserves the Master track"))
            return false;
        gui.selectTrack(1);
        if (!check(openTrack() && matchesUAT() && popup.buttons[1].isEnabled()
            && !popup.buttons[0].isEnabled() && !popup.buttons[2].isEnabled() && !popup.buttons[3].isEnabled()
            && escape(), "lane allows Delete only in Edit"))
            return false;

        gui.selection = 0;
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.actionSelection.clips = { clipID };
        if (!check(openTrack() && matchesUAT() && popup.buttons[2].isEnabled(), "Timeline can copy a Master pattern"))
            return false;
        invoke(Edit::copy);
        const auto* clipClipboard = gui.clipClipboard.get();
        if (!check(clipClipboard && clipClipboard->clips.size() == 1, "Timeline uses the clip clipboard"))
            return false;
        gui.editCursor = gui.barTime(5);
        if (!check(openTrack() && popup.buttons[3].isEnabled(), "compatible Timeline paste on the Master is enabled"))
            return false;
        invoke(Edit::paste);
        auto* placed = dynamic_cast<te::MidiClip*>(gui.selectedClip());
        if (!check(placed && placed != clip && placed->getTrack() == master
            && placed->getMidiChannel() == te::MidiChannel(drumMaster::midiChannel),
            "Timeline Paste on the Master uses channel 10"))
            return false;
        gui.useHistory(false);
        if (!check(master->getClips().size() == 1 && notes().size() == 3, "Timeline Undo removes the pasted pattern"))
            return false;
        gui.selectTrack(1);
        gui.actionSelection.requestedArea = EditingArea::timeline;
        if (!check(openTrack() && matchesUAT() && !popup.buttons[3].isEnabled() && escape(),
            "Timeline cannot paste clips onto a drum lane"))
            return false;

        gui.openDrumEditor(*clip);
        gui.actionSelection.requestedArea = EditingArea::timeline;
        if (!check(openDrum() && gui.activeEditingArea() == EditingArea::timeline && escape(),
            "middle-click over an unfocused drum grid preserves Timeline focus"))
            return false;
        gui.focusDrumEditor();
        if (!check(openDrum() && matchesUAT()
            && std::none_of(popup.buttons.begin(), popup.buttons.end(), [](const auto& b) { return b.isEnabled(); })
            && escape(), "empty drum selection and clipboard disable Edit"))
            return false;

        const std::set<DrumStep> selection { { 36, 0 }, { 38, 1 } };
        gui.drumSteps = selection;
        gui.drumPasteStep = 4;
        gui.updateControls();
        const auto before = notes();
        const auto beforeCursor = gui.editCursor;
        const auto beforeView = gui.drumLeftQuarter;
        const auto beforeLane = gui.drumFirstLane;
        const auto focus = ju::Component::getCurrentlyFocusedComponent();
        if (!check(openDrum() && matchesUAT() && gui.drumGridHasFocus() && gui.drumSteps == selection
            && gui.drumPasteStep == 4 && gui.editCursor == beforeCursor
            && ju::Component::getCurrentlyFocusedComponent() == focus, "open preserves selected hits and both edit cursors"))
            return false;
        const auto wheel = [&](ju::Component& component, ju::Point<int> point, float x, float y) {
            component.mouseWheelMove(mouse(component, point), { x, y, false, false, false });
        };
        const auto wheelPoint = gui.getLocalPoint(&gui.drumEditor, drumPoint);
        if (!check(gui.getComponentAt(wheelPoint) == &popup, "popup owns the wheel over the grid"))
            return false;
        gui.drumEditor.mouseWheelMove(mouse(gui.drumEditor, drumPoint), { 0.0f, 1.0f, false, true, true });
        if (!check(popup.page == 0 && notes() == before, "an inertial wheel event on the previous grid target is ignored"))
            return false;
        wheel(popup, wheelPoint, 0.05f, 1.0f);
        if (!check(popup.page == 1 && notes() == before && gui.drumLeftQuarter == beforeView
            && gui.drumFirstLane == beforeLane, "wheel cycles without altering velocity or scroll position"))
            return false;
        wheel(popup, wheelPoint, 1.0f, 0.0f);
        if (!check(popup.page == 1 && gui.drumLeftQuarter == beforeView, "horizontal wheel is consumed"))
            return false;
        wheel(popup, wheelPoint, 0.0f, -1.0f);
        if (!check(escape() && !popup.isVisible() && gui.drumGridHasFocus() && gui.drumSteps == selection,
            "first Escape dismisses Context UI and keeps the Drum Pattern Editor open"))
            return false;
        wheel(gui.drumEditor, drumPoint, 0.05f, 1.0f);
        const auto louder = notes();
        if (!check(louder.size() == 3 && louder[0].velocity == 96 && louder[1].velocity == 96
            && louder[2].velocity == 75, "closed popup restores grouped velocity wheel behavior"))
            return false;
        gui.useHistory(false);
        if (!check(notes() == before && openDrum(), "velocity Undo restores the original hits"))
            return false;

        invoke(Edit::copy);
        if (!check(gui.drumClipboard.size() == 3 && gui.clipClipboard.get() == clipClipboard
            && gui.drumSteps == selection && openDrum() && popup.buttons[3].isEnabled(),
            "drum Copy preserves selection and uses a separate clipboard"))
            return false;
        gui.drumPasteStep = 5;
        invoke(Edit::paste);
        if (!check(!popup.isVisible() && notes() == before, "changed paste step invalidates the popup"))
            return false;
        gui.drumPasteStep = 4;
        if (!openDrum())
            return false;
        invoke(Edit::paste);
        const auto pasted = notes();
        if (!check(pasted.size() == 6 && pasted[3].start == 1.0 && pasted[4].start == 1.0625
            && pasted[4].velocity == 84 && pasted[5].pitch == 38, "drum Paste preserves subdivision timing and velocity"))
            return false;
        gui.useHistory(false);
        if (!check(notes() == before, "one Undo removes the whole paste"))
            return false;
        gui.useHistory(true);
        if (!check(notes().size() == 6, "Redo restores pasted hits"))
            return false;
        gui.useHistory(false);
        gui.drumSteps = selection;
        if (!openDrum())
            return false;
        gui.drumSteps.erase({ 36, 0 });
        invoke(Edit::remove);
        if (!check(!popup.isVisible() && notes() == before, "stale hit selection blocks Delete"))
            return false;
        gui.drumSteps = selection;
        if (!openDrum())
            return false;
        invoke(Edit::cut);
        if (!check(notes().empty() && gui.drumClipboard.size() == 3, "drum Cut copies and removes the hits"))
            return false;
        gui.useHistory(false);
        gui.drumSteps = selection;
        if (!check(notes() == before && openDrum(), "Undo restores Cut hits"))
            return false;
        invoke(Edit::remove);
        if (!check(notes().empty() && gui.drumClipboard.size() == 3, "drum Delete leaves the clipboard intact"))
            return false;
        gui.useHistory(false);
        if (!check(notes() == before && openDrum(), "Undo restores deleted hits"))
            return false;
        gui.menuItemSelected(enableDrumMasterMenuItem, 5);
        invoke(Edit::paste);
        return check(!popup.isVisible() && !gui.drumEditor.isVisible() && !master->isProcessing(false)
            && notes() == before, "disabling the Master invalidates the drum popup without editing the pattern");
    }
