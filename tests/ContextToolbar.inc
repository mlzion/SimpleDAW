// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    static auto& contextEditButton(ContextToolbar& popup, ContextToolbar::Command action)
    {
        const auto index = std::find(ContextToolbar::edits.begin(), ContextToolbar::edits.end(), action)
            - ContextToolbar::edits.begin();
        return popup.buttons[static_cast<size_t>(index)];
    }

    static bool testContextToolbar(te::Engine& engine)
    {
        using EditAction = ContextToolbar::Command;
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
            gui.refreshContextToolbar();
            if (popup.isVisible())
                return true;
            auto click = mouse(gui.arrangement, { 25, 12 }, middle);
            gui.arrangement.mouseDown(click);
            gui.arrangement.mouseUp(click);
            return popup.isVisible() && popup.page == ContextToolbar::movePage;
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
            popup.page = ContextToolbar::editPage;
            popup.showPage();
            contextEditButton(popup, action).onClick();
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
        if (!check(editGroupVisible(false) && !gui.actionGroups[1].frame.isVisible() && gui.actionGroups[2].frame.isVisible() && open()
            && gui.activeEditingArea() == EditingArea::timeline && gui.actionSelection.clips == originalSelection
            && gui.editCursor == seconds(6) && ju::Component::getCurrentlyFocusedComponent() == focus
            && availabilityMatchesUAT() && !contextEditButton(popup, EditAction::paste).isEnabled(), "opening over a track keeps timeline context"))
            return false;
        if (!check(popup.page == ContextToolbar::movePage && popup.moveButtons.front().isVisible()
            && !popup.buttons.front().isVisible(), "Move opens first"))
            return false;
        popup.page = ContextToolbar::editPage;
        popup.showPage();
        const std::array<ju::String, 4> labels { "Cut", "Copy", "Paste", "Delete" };
        int previousX = -1;
        for (size_t i = 0; i < labels.size(); ++i)
        {
            const auto& button = popup.buttons[i];
            if (!check(button.getButtonText() == labels[i] && button.getX() > previousX,
                       "Edit displays Cut, Copy, Paste and Delete from left to right"))
                return false;
            previousX = button.getX();
        }
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
        if (!check(popup.page == ContextToolbar::movePage && !popup.buttons.front().isVisible(), "up cycles from Edit to Move"))
            return false;
        wheel(-1, false);
        if (!check(popup.page == ContextToolbar::editPage && popup.buttons.front().isVisible(), "down cycles to Edit"))
            return false;
        wheel(-1, true);
        if (!check(popup.page == ContextToolbar::movePage, "down wraps from Edit to Move"))
            return false;
        wheel(1, false);
        if (!check(popup.page == ContextToolbar::editPage && popup.panel == anchor && gui.firstRow == row
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
        if (!check(popup.isVisible() && popup.page == ContextToolbar::editPage && popup.panel == anchor
            && gui.actionSelection.clips == originalSelection && contextEditButton(popup, EditAction::paste).isEnabled(),
            "popup Copy preserves anchor, page and selection and enables Paste"))
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
        if (!check(open() && availabilityMatchesUAT() && !contextEditButton(popup, EditAction::cut).isEnabled(),
            "Track List disables Cut"))
            return false;
        invoke(EditAction::cut);
        if (!check(track->getClips().size() == 2 && open(), "disabled dispatch cannot mutate tracks"))
            return false;
        invoke(EditAction::copy);
        gui.selectTrack(1);
        if (!check(open() && contextEditButton(popup, EditAction::paste).isEnabled() && availabilityMatchesUAT(), "Track List clipboard"))
            return false;
        invoke(EditAction::paste);
        if (!check(gui.trackCount() == 3, "popup Paste duplicates a track below the anchor"))
            return false;

        gui.dismissContextToolbar();
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
        if (!check(gui.pianoRoll.clipboard.size() == 1 && popup.isVisible() && contextEditButton(popup, EditAction::paste).isEnabled(),
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
        return check(!popup.isVisible() && editGroupVisible(true) && gui.actionGroups[1].frame.isVisible()
            && !open(), "classic restoration closes the popup and restores Edit and Move")
            && testDrumContextToolbar(engine) && testContextMove(engine);
    }

    static bool testDrumContextToolbar(te::Engine& engine)
    {
        using Edit = ContextToolbar::Command;
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
            gui.refreshContextToolbar();
            if (popup.isVisible())
                return true;
            gui.arrangement.mouseDown(mouse(gui.arrangement, { 30, 60 }, middle));
            return popup.isVisible();
        };
        const auto drumPoint = ju::Point<int>(drumNameWidth + 15, drumHeaderHeight + 10);
        const auto openDrum = [&] {
            gui.refreshContextToolbar();
            if (popup.isVisible())
                return true;
            auto event = mouse(gui.drumEditor, drumPoint, middle);
            gui.drumEditor.mouseDown(event);
            gui.drumEditor.mouseUp(event);
            return popup.isVisible();
        };
        const auto escape = [&] { return gui.keyPressed(ju::KeyPress(ju::KeyPress::escapeKey), &gui.drumEditor); };
        const auto invoke = [&](Edit action) {
            popup.page = ContextToolbar::editPage;
            popup.showPage();
            contextEditButton(popup, action).onClick();
        };
        const auto matchesUAT = [&] {
            for (size_t i = 0; i < ContextToolbar::edits.size(); ++i)
                if (popup.buttons[i].isEnabled() != gui.canDispatch(contextAction(ContextToolbar::edits[i])))
                    return false;
            return true;
        };

        gui.edit->ensureNumberOfAudioTracks(12);
        gui.setSize(1200, 580);
        gui.scrollRows(2);
        if (!check(gui.trackAt(0) == master && gui.firstRow > 0 && gui.rowTop(0) < rulerHeight,
            "Master is pinned in order and scrolls out of view"))
            return false;
        gui.scrollRows(-100);
        gui.setSize(1440, 850);
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
        if (!check(openTrack() && matchesUAT() && contextEditButton(popup, Edit::remove).isEnabled()
            && !contextEditButton(popup, Edit::cut).isEnabled() && !contextEditButton(popup, Edit::copy).isEnabled() && !contextEditButton(popup, Edit::paste).isEnabled()
            && escape(), "lane allows Delete only in Edit"))
            return false;

        gui.selection = 0;
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.actionSelection.clips = { clipID };
        if (!check(openTrack() && matchesUAT() && contextEditButton(popup, Edit::copy).isEnabled(), "Timeline can copy a Master pattern"))
            return false;
        invoke(Edit::copy);
        const auto* clipClipboard = gui.clipClipboard.get();
        if (!check(clipClipboard && clipClipboard->clips.size() == 1, "Timeline uses the clip clipboard"))
            return false;
        gui.editCursor = gui.barTime(5);
        if (!check(openTrack() && contextEditButton(popup, Edit::paste).isEnabled(), "compatible Timeline paste on the Master is enabled"))
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
        if (!check(openTrack() && matchesUAT() && !contextEditButton(popup, Edit::paste).isEnabled() && escape(),
            "Timeline cannot paste clips onto a drum lane"))
            return false;

        gui.openDrumEditor(*clip);
        gui.setSize(1200, 580);
        if (!check(gui.editingPanelArea.getY() >= gui.arrangement.getY() + rulerHeight,
            "the enlarged ruler remains visible above the drum dock"))
            return false;
        gui.setSize(1440, 850);
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
        if (!check(notes() == before && openDrum(), "wheel navigation leaves hit velocities unchanged"))
            return false;

        if (!check(!popup.finer.isEnabled() && popup.snapText == "1/16 (fixed)"
            && popup.moveButtons[1].isEnabled(), "drum movement uses a fixed sixteenth grid"))
            return false;
        popup.moveButtons[1].onClick();
        if (!check(popup.isVisible() && gui.drumSteps.contains({ 36, 1 })
            && gui.drumSteps.contains({ 38, 2 }), "drum Right shifts the selected hits and keeps popup"))
            return false;
        popup.moveButtons[0].onClick();
        if (!check(popup.isVisible() && notes() == before && gui.drumSteps == selection,
            "drum Left restores the subdivided hits"))
            return false;

        invoke(Edit::copy);
        if (!check(gui.drumClipboard.size() == 3 && gui.clipClipboard.get() == clipClipboard
            && gui.drumSteps == selection && popup.isVisible() && contextEditButton(popup, Edit::paste).isEnabled(),
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

    static bool testContextMove(te::Engine& engine)
    {
        using Action = UniversalAction;
        using Popup = ContextToolbar;
        const auto check = [](bool ok, const char* step) {
            if (!ok)
                std::fprintf(stderr, "Context Move: %s\n", step);
            return ok;
        };
        const ju::TemporaryFile path(".tracktionedit"), settingsFile(".settings");
        MainGui gui(engine, settingsFile.getFile());
        gui.setVisible(true);
        gui.setSize(1440, 900);
        gui.setProject(te::createEmptyEdit(engine, path.getFile()), path.getFile());
        gui.edit->ensureNumberOfAudioTracks(12);
        auto* track = gui.trackAt(0);
        auto* next = gui.trackAt(1);
        track->setName("Guitar 1");
        next->setName("Keyboard");
        next->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        const auto seconds = te::TimePosition::fromSeconds<double>;
        track->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        const auto clip = te::insertMIDIClip(*track, "Clip", { seconds(2), seconds(4) });
        if (!clip)
            return false;
        track->state.setProperty(trackTypeProperty, "Audio", nullptr);
        if (!check(gui.trackListTitle(*track) == "1A Guitar 1" && gui.trackListTitle(*next) == "2M Keyboard",
            "compact Track List identifiers preserve names"))
            return false;
        track->state.setProperty(trackTypeProperty, "MIDI", nullptr);

        auto& popup = gui.contextToolbar;
        const auto mouse = [](ju::Component& component, ju::Point<int> point, int modifier) {
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), point.toFloat(),
                ju::ModifierKeys(modifier), 0, 0, 0, 0, 0, &component, &component, now, point.toFloat(), now, 1, false);
        };
        const auto middle = ju::ModifierKeys::middleButtonModifier;
        const auto click = [&](ju::Point<int> point) {
            const auto event = mouse(gui.arrangement, point, ju::ModifierKeys::leftButtonModifier);
            gui.arrangement.mouseDown(event);
            gui.arrangement.mouseUp(event);
        };
        const auto open = [&] {
            const auto event = mouse(gui.arrangement, { headerWidth + 25, rulerHeight + 20 }, middle);
            gui.arrangement.mouseDown(event);
            gui.arrangement.mouseUp(event);
            return popup.isVisible() && popup.page == Popup::movePage;
        };
        gui.setContextMode(true);
        gui.selection = 0;
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.actionSelection.clips = { clip->itemID };
        gui.editCursor = seconds(0);
        if (!check(gui.snap.getSelectedId() == snapBar && open() && popup.snapText == "1 Bar", "startup defaults to bar snap"))
            return false;
        const auto anchor = popup.panel;
        popup.coarser.onClick();
        if (!check(gui.snap.getSelectedId() == snapOff && popup.snapText == "OFF" && popup.panel == anchor,
            "coarser rolls from bar to OFF"))
            return false;
        popup.finer.onClick();
        if (!check(gui.snap.getSelectedId() == snapBar, "finer rolls from OFF to bar"))
            return false;
        for (auto id : { snapHalf, 2, 3, 4, snapThirtySecond, snapOff, snapBar })
        {
            popup.finer.onClick();
            if (!check(gui.snap.getSelectedId() == id && popup.isVisible(), "all seven snap choices roll in order"))
                return false;
        }
        const auto arrowPoints = [](auto& button) {
            ju::Image image(ju::Image::RGB, button.getWidth(), button.getHeight(), true);
            {
                // Native images finish the drawing session when Graphics is destroyed.
                ju::Graphics graphics(image);
                button.paintButton(graphics, false, false);
            }
            const auto centre = button.getLocalBounds().getCentre();
            const auto count = [&](int x, int y) {
                int bright = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        bright += image.getPixelAt(x + dx, y + dy).getBrightness() > 0.7f;
                return bright;
            };
            return std::pair { count(centre.x - 3, centre.y), count(centre.x + 3, centre.y) };
        };
        const auto finer = arrowPoints(popup.finer);
        const auto coarser = arrowPoints(popup.coarser);
        if (finer.first <= finer.second || coarser.second <= coarser.first)
            std::fprintf(stderr, "Chevron tip pixels (left/right) finer %d/%d, coarser %d/%d\n",
                finer.first, finer.second, coarser.first, coarser.second);
        if (!check(finer.first > finer.second && coarser.second > coarser.first,
            "snap arrows paint opposing vector tips in the existing hit areas"))
            return false;

        const auto original = clip->getPosition();
        popup.moveButtons[1].onClick();
        if (!check(popup.isVisible() && popup.panel == anchor && clip->getPosition().getStart() == gui.barTime(2),
            "clip Right uses a whole bar and retains popup"))
            return false;
        popup.moveButtons[0].onClick();
        if (!check(popup.isVisible() && clip->getPosition() == original, "clip Left reverses the bar movement"))
            return false;
        popup.moveButtons[3].onClick();
        if (!check(popup.isVisible() && clip->getTrack() == next, "clip Down uses compatible placement and retains popup"))
            return false;
        gui.dismissContextToolbar();
        gui.useHistory(false);
        if (!check(clip->getTrack() == track, "Undo reverses one popup Move"))
            return false;

        gui.selectTrack(1);
        if (!check(open() && !popup.finer.isEnabled() && !popup.moveButtons[0].isEnabled(),
            "Track List disables snap and horizontal Move"))
            return false;
        popup.moveButtons[2].onClick();
        if (!check(popup.isVisible() && gui.trackAt(0) == next && gui.trackListTitle(*next) == "1M Keyboard",
            "Track List Up reorders and updates compact numbering"))
            return false;
        gui.dismissContextToolbar();

        auto& meter = gui.edit->tempoSequence.getTimeSigAt(seconds(0));
        meter.setStringTimeSig("7/8");
        const auto firstBar = gui.barTime(1);
        const auto wholeBar = gui.timelineMoveDelta(seconds(0), false);
        if (!check(firstBar > seconds(0) && wholeBar && seconds(0) + *wholeBar == firstBar
            && gui.snapSeconds(firstBar.inSeconds() * 0.8) == firstBar.inSeconds(),
            "bar snap follows the 7/8 meter"))
            return false;
        meter.setStringTimeSig("4/4");

        const auto selected = gui.actionSelection.clips;
        const auto checkThresholds = [&](int grid, double increment, double anchor) {
            gui.snap.setSelectedId(grid, ju::dontSendNotification);
            gui.draggingRange = true;
            gui.dragAnchor = anchor;
            const auto fixed = seconds(8.0);
            // Increase and then decrease the same drag without resetting its anchor.
            for (const auto direction : { 1, -1 })
                for (const auto& [steps, length] : std::array<std::pair<double, int>, 13> {{
                    { 0.49, 0 }, { 0.50, 1 }, { 0.51, 1 }, { 1.49, 1 }, { 1.50, 2 }, { 1.51, 2 },
                    { 2.49, 2 }, { 2.50, 3 }, { 2.51, 3 }, { 2.49, 2 }, { 1.49, 1 }, { 0.49, 0 }, { 0.0, 0 }
                }})
                {
                    gui.updateDragPreview(anchor + direction * steps * increment);
                    const auto& range = gui.rangePreview;
                    if (length == 0)
                    {
                        if (!check(!range, "below halfway creates no range, including when shrinking"))
                            return false;
                    }
                    else
                    {
                        const auto endpoint = seconds(8.0 + direction * length * increment);
                        if (!check(range
                            && std::abs((range->getStart() - std::min(fixed, endpoint)).inSeconds()) < 1.0e-9
                            && std::abs((range->getEnd() - std::max(fixed, endpoint)).inSeconds()) < 1.0e-9,
                            "halfway snaps to the next division in either direction"))
                            return false;
                    }
                }
            gui.draggingRange = false;
            return true;
        };
        if (!checkThresholds(snapBar, gui.barTime(1).inSeconds(), 8.2)
            || !checkThresholds(4, 0.125, 8.02)
            || !checkThresholds(snapOff, 0.010, 8.002))
            return false;

        gui.snap.setSelectedId(snapOff, ju::dontSendNotification);
        gui.seek(1.234);
        if (!check(gui.editCursor == seconds(1.234), "Snap Off leaves cursor positioning unquantized"))
            return false;
        gui.dismissContextToolbar();
        gui.actionSelection.requestedArea = EditingArea::timeline;
        using Handle = RulerHandle;
        const auto drag = [&](ju::Point<int> start, std::initializer_list<int> offsets, auto inspect) {
            const auto now = ju::Time::getCurrentTime();
            const auto event = [&](int dx, bool moved) {
                return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(),
                    (start + ju::Point<int>(dx, 0)).toFloat(), ju::ModifierKeys(ju::ModifierKeys::leftButtonModifier),
                    0, 0, 0, 0, 0, &gui.arrangement, &gui.arrangement, now, start.toFloat(), now, 1, moved);
            };
            gui.arrangement.mouseDown(event(0, false));
            for (const auto offset : offsets)
            {
                gui.arrangement.mouseDrag(event(offset, true));
                inspect(offset);
            }
            gui.arrangement.mouseUp(event(*(offsets.end() - 1), true));
        };
        const auto ignorePreview = [](int) {};
        const auto rulerPoint = [&](double time) { return ju::Point<int>(gui.timeX(seconds(time)), 22); };
        const auto paintPixel = [&](ju::Point<int> point) {
            ju::Image image(ju::Image::RGB, gui.arrangement.getWidth(), gui.arrangement.getHeight(), true);
            {
                ju::Graphics graphics(image);
                gui.paintArrangement(graphics);
            }
            return image.getPixelAt(point.x, point.y);
        };
        const auto savedScale = gui.pixelsPerSecond;
        gui.pixelsPerSecond = 1000.0;
        drag(rulerPoint(0.202), {4}, ignorePreview);
        if (!check(!gui.timeSelection, "a real 4 ms ruler drag creates no Snap Off window"))
            return false;
        drag(rulerPoint(0.202), {5}, ignorePreview);
        if (!check(gui.timeSelection && std::abs(gui.timeSelection->getLength().inSeconds() - 0.01) < 1.0e-9,
            "a real 5 ms ruler drag commits a 10 ms Snap Off window"))
            return false;
        gui.pixelsPerSecond = 100.0;
        gui.snap.setSelectedId(snapBar, ju::dontSendNotification);
        auto previewFollowed = true;
        const auto first = rulerPoint(0.2);
        drag(first, {50, 110, 50}, [&](int offset) {
            const auto feedback = gui.displayedWindow();
            previewFollowed &= feedback
                && gui.timeX(feedback->getStart()) == first.x
                && gui.timeX(feedback->getEnd()) == first.x + offset
                && paintPixel({first.x + offset, rulerHeight + 3}) == ju::Colour(0xff6b9ce0);
        });
        if (!check(previewFollowed && !gui.timeSelection,
            "rendered feedback follows the raw pointer across halfway and reversal; short release clears the old range"))
            return false;
        drag(rulerPoint(4.0), {-90, -110}, ignorePreview);
        if (!check(gui.timeSelection == te::TimeRange(seconds(2), seconds(4)),
            "leftward ruler drag past halfway commits one snapped bar"))
            return false;
        drag(rulerPoint(0.2), {110, 0}, ignorePreview);
        if (!check(!gui.timeSelection, "returning to the origin clears the previous window"))
            return false;

        gui.snap.setSelectedId(snapOff, ju::dontSendNotification);
        gui.setTimeSelection({seconds(2), seconds(4)});
        gui.editCursor = seconds(1);
        auto& transport = gui.edit->getTransport();
        transport.looping = true;
        const auto centre = gui.rulerHandle(Handle::centre);
        const auto cursor = gui.rulerHandle(Handle::cursor);
        const auto left = gui.rulerHandle(Handle::start);
        const auto right = gui.rulerHandle(Handle::end);
        if (!check(centre.getWidth() >= 85 && cursor.getWidth() >= 70 && centre.getHeight() >= 25
            && left.getWidth() >= 25 && right.getWidth() >= 25
            && centre.getY() == left.getY() && centre.getY() == right.getY()
            && centre.getY() >= cursor.getBottom() && centre.getBottom() <= rulerHeight
            && gui.rulerHandleAt(left.getCentre()) == Handle::start
            && gui.rulerHandleAt(right.getCentre()) == Handle::end,
            "cursor, centre and aligned resize handles have generous distinct hit areas"))
            return false;
        const auto viewBeforeEdge = gui.viewStart;
        gui.setViewStart(10);
        if (!check(gui.rulerHandleAt(gui.rulerHandle(Handle::centre).getCentre()) == Handle::centre,
            "off-screen range keeps a reachable centre handle"))
            return false;
        gui.setViewStart(viewBeforeEdge);

        drag(centre.getCentre(), {50, 100}, ignorePreview);
        if (!check(gui.timeSelection == te::TimeRange(seconds(3), seconds(5))
            && transport.getLoopRange() == *gui.timeSelection && transport.looping,
            "centre drag retains duration and updates the active loop"))
            return false;
        drag(gui.rulerHandle(Handle::start).getCentre(), {50, 70, 50}, ignorePreview);
        drag(gui.rulerHandle(Handle::end).getCentre(), {50, 70, 50}, ignorePreview);
        if (!check(gui.timeSelection == te::TimeRange(seconds(3.5), seconds(5.5)),
            "either boundary can resize with direction reversal"))
            return false;
        drag(gui.rulerHandle(Handle::end).getCentre(), {-200}, ignorePreview);
        if (!check(!gui.timeSelection && !transport.looping, "coincident boundaries clear the window and disable looping"))
            return false;

        gui.setTimeSelection({seconds(2), seconds(4)});
        drag(gui.rulerHandle(Handle::start).getCentre(), {200}, ignorePreview);
        if (!check(!gui.timeSelection, "the left boundary can also collapse the window"))
            return false;

        const auto doubleClick = [&](Handle handle) {
            const auto position = gui.rulerHandle(handle).getCentre();
            click(position);
            click(position);
            gui.arrangement.mouseDoubleClick(mouse(gui.arrangement, position, ju::ModifierKeys::leftButtonModifier));
        };
        gui.setTimeSelection({seconds(2), seconds(4)});
        doubleClick(Handle::centre);
        const auto playback = transport.getPosition();
        drag(gui.rulerHandle(Handle::cursor).getCentre(), {40, 100}, ignorePreview);
        if (!check(!gui.timeSelection && gui.editCursor == seconds(2) && transport.getPosition() == playback,
            "centre double-click clears the range and cursor drag moves independently of playback"))
            return false;
        doubleClick(Handle::cursor);
        if (!check(gui.editCursor == seconds(0) && transport.getPosition() == playback
            && gui.actionSelection.clips == selected && gui.canDispatch(Action::remove)
            && gui.canDispatch(Action::copy) && open() && popup.moveButtons[1].isEnabled(),
            "cursor double-click resets to zero and ruler gestures preserve existing clip command routing"))
            return false;
        gui.dismissContextToolbar();
        click({headerWidth + 40, 20});
        if (!check(gui.editCursor > seconds(0) && gui.editCursor < seconds(1), "ordinary ruler click still positions the cursor"))
            return false;
        gui.setTimeSelection({seconds(2), seconds(4)});
        const auto stoppedCursor = gui.editCursor;
        transport.play(false);
        const auto playing = transport.isPlaying();
        drag(gui.rulerHandle(Handle::cursor).getCentre(), {100}, ignorePreview);
        drag(gui.rulerHandle(Handle::centre).getCentre(), {100}, ignorePreview);
        drag(gui.rulerHandle(Handle::start).getCentre(), {200}, ignorePreview);
        doubleClick(Handle::centre);
        if (!check(playing && gui.timeSelection == te::TimeRange(seconds(2), seconds(4))
            && gui.editCursor == stoppedCursor, "active transport blocks direct handle editing"))
            return false;
        transport.stop(false, false);
        for (const bool experimental : {false, true})
        {
            gui.setContextMode(experimental);
            gui.actionSelection.requestedArea = EditingArea::tracks;
            gui.actionSelection.clips = selected;
            click({headerWidth + 60, 22});
            if (!check(gui.activeEditingArea() == EditingArea::tracks && gui.actionSelection.clips == selected,
                "ruler click preserves effective focus and selected clips in either mode"))
                return false;
            const auto cursorBefore = gui.editCursor;
            const auto rangeBefore = gui.timeSelection;
            const auto point = ju::Point<int>(headerWidth + 15, rulerHeight + 15);
            const auto shiftClick = mouse(gui.arrangement, point,
                ju::ModifierKeys::leftButtonModifier | ju::ModifierKeys::shiftModifier);
            gui.arrangement.mouseDown(shiftClick);
            gui.arrangement.mouseUp(shiftClick);
            click(point);
            if (!check(gui.editCursor == cursorBefore && gui.timeSelection == rangeBefore,
                "ordinary and Shift clicks in the clip area leave the ruler alone"))
                return false;
        }

        gui.setContextMode(false);
        gui.clearTimeSelection();
        const auto second = te::insertMIDIClip(*track, "Off-grid clip", {seconds(5.03), seconds(6.03)});
        if (!second)
            return false;
        const auto rowForClip = gui.visibleTracks().indexOf(track);
        const auto rowY = gui.rowTop(rowForClip) + 12;
        const auto firstBefore = clip->getPosition().time;
        const auto secondBefore = second->getPosition().time;
        gui.actionSelection.clips = {clip->itemID, second->itemID};
        drag({gui.timeX(firstBefore.getEnd()) - 3, rowY}, {12, 32, 25}, ignorePreview);
        if (!check(clip->getPosition().time == firstBefore.withEnd(firstBefore.getEnd() + te::TimeDuration::fromSeconds(0.25))
            && second->getPosition().time == secondBefore.withEnd(secondBefore.getEnd() + te::TimeDuration::fromSeconds(0.25)),
            "direct group resize preserves opposite edges and off-grid spacing"))
            return false;
        gui.useHistory(false);
        if (!check(clip->getPosition().time == firstBefore && second->getPosition().time == secondBefore,
            "one Undo restores both resized clips"))
            return false;

        gui.actionSelection.clips = {clip->itemID, second->itemID};
        const auto empty = ju::Point<int>(gui.timeX(seconds(7)), rowY);
        const auto emptyPress = mouse(gui.arrangement, empty, ju::ModifierKeys::rightButtonModifier);
        const auto onClip = mouse(gui.arrangement, {gui.timeX(firstBefore.getStart()) + 15, rowY},
            ju::ModifierKeys::rightButtonModifier);
        const auto beforePan = gui.viewStart;
        gui.arrangement.mouseDown(emptyPress);
        gui.arrangement.mouseWheelMove(onClip, {0, -0.1f, false, false, false});
        gui.arrangement.mouseUp(onClip);
        if (!check(gui.viewStart > beforePan && gui.actionSelection.clips.size() == 2,
            "right-wheel stays horizontal when a clip crosses the pointer and release preserves selection"))
            return false;
        gui.setViewStart(beforePan);
        second->removeFromParent();
        gui.actionSelection.clips = selected;
        gui.pixelsPerSecond = savedScale;

        gui.setContextMode(false);
        gui.resized();
        int row = 1;
        while (row + 1 < gui.trackCount() && gui.rowTop(row + 1) + 10 < gui.arrangement.getHeight())
            ++row;
        gui.panels.inspectorOpen = true;
        gui.resized();
        gui.inspectorViewport.setSize(gui.inspectorViewport.getWidth(), 70);
        gui.inspectorViewport.setViewPosition(0, 0);
        const auto inspectorWheel = mouse(gui.inspectorViewport, {30, 25}, 0);
        gui.inspectorViewport.mouseWheelMove(inspectorWheel, {0, -1, false, false, false});
        if (!check(gui.inspectorViewport.getViewPositionY() > 0, "wheel scrolls the Inspector viewport"))
            return false;
        gui.resized();
        auto* lowerTrack = gui.trackAt(row);
        lowerTrack->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        const auto lowerClip = te::insertMIDIClip(*lowerTrack, "Low MIDI clip", { seconds(1), seconds(3) });
        if (!lowerClip)
            return false;
        const auto column = gui.timeX(seconds(1.2));
        const auto view = gui.viewStart;
        const auto scale = gui.pixelsPerSecond;
        const auto scrollBeforeDock = gui.firstRow;
        const auto event = mouse(gui.arrangement, { column, gui.rowTop(row) + 10 }, ju::ModifierKeys::leftButtonModifier);
        gui.openClipEditor(event);
        if (!check(gui.pianoRollHasFocus() && gui.pianoRollClip == lowerClip->itemID
            && gui.rowTop(row) >= rulerHeight && gui.rowTop(row) + gui.rowHeight(row) <= gui.arrangement.getHeight()
            && gui.arrangement.getBottom() == gui.editingPanelArea.getY() && gui.firstRow > scrollBeforeDock
            && gui.viewStart == view && gui.pixelsPerSecond == scale,
            "opening Piano Roll scrolls the edited row into the actual unobscured viewport"))
            return false;

        gui.setContextMode(true);
        const auto midiNote = MidiNoteEditor(*lowerClip).insert({ { {}, 0.5, 0.25, 60, 90 } }, "Context move fixture");
        gui.pianoRoll.selected = midiNote;
        const auto beforeSnap = gui.snap.getSelectedId();
        if (!check(open() && popup.finer.isEnabled(), "Piano Roll has independent popup snap"))
            return false;
        const auto pianoSnap = gui.pianoRoll.snap.getSelectedId();
        popup.finer.onClick();
        if (!check(gui.snap.getSelectedId() == beforeSnap && gui.pianoRoll.snap.getSelectedId() != pianoSnap,
            "Piano Roll snap rollover keeps timeline snap"))
            return false;
        popup.moveButtons[2].onClick();
        const auto notes = MidiNoteEditor(*lowerClip).notes();
        if (!check(popup.isVisible() && notes.size() == 1 && notes.front().pitch == 61,
            "Piano Roll Up keeps the popup and transposes one semitone"))
            return false;

        gui.setContextMode(false);
        gui.closePianoRoll();
        gui.scrollRows(-100);
        const auto clipPoint = [&](int index, double time) {
            return mouse(gui.arrangement, { gui.timeX(seconds(time)), gui.rowTop(index) + 10 },
                         ju::ModifierKeys::leftButtonModifier);
        };
        gui.scrollRows(1);
        const auto firstRow = gui.firstRow;
        gui.openClipEditor(clipPoint(1, 2.2));
        if (!check(gui.pianoRollClip == clip->itemID && gui.firstRow == firstRow,
            "opening an already-visible Piano Roll target leaves vertical scroll alone"))
            return false;
        gui.closePianoRoll();
        gui.panels.tallTracks = true;
        gui.scrollRows(-100);
        const int tallRow = 6;
        auto* tallTrack = gui.trackAt(tallRow);
        tallTrack->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        const auto tallClip = te::insertMIDIClip(*tallTrack, "Tall MIDI clip", { seconds(1), seconds(3) });
        if (!tallClip)
            return false;
        gui.openClipEditor(clipPoint(tallRow, 1.2));
        gui.setSize(1060, 580);
        const auto rowBottom = gui.arrangement.getY() + gui.rowTop(tallRow) + gui.rowHeight(tallRow);
        if (!check(gui.pianoRollClip == tallClip->itemID && rowBottom <= gui.editingPanelArea.getY()
            && gui.rowTop(tallRow) >= rulerHeight, "post-layout Piano Roll reserves room for a full tall row"))
            return false;
        gui.closePianoRoll();
        return check(gui.arrangement.getBottom() > rowBottom, "closing Piano Roll restores the full arrangement");
    }
