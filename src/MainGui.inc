// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    enum Command
    {
        newProject = 100, openProject, saveProject, saveProjectAs, importAudio, exportAudio, exitApp,
        undo, redo, cut, copy, paste, duplicate, deleteItem,
        audioTrack, midiTrack, midiItem, devices, about, armAudio, armMidi, synth, reverb, vst3,
        audioFile, locateFile, monitoringReport, playStop, startRecording
    };

    enum class EditingArea { window, inspector, timeline, tracks };
    enum class UniversalAction
    {
        cut, remove, copy, paste, up, down, left, right,
        startMinus, startPlus, endMinus, endPlus, split, trim, reserved
    };

    using ItemSelection = std::set<te::EditItemID>;

    struct ActionSelection
    {
        std::optional<EditingArea> requestedArea;
        ItemSelection clips, tracks;

        EditingArea activeArea(bool hasWindow, bool hasInspector) const
        {
            if (requestedArea && (*requestedArea != EditingArea::window || hasWindow)
                && (*requestedArea != EditingArea::inspector || hasInspector))
                return *requestedArea;
            // Only existing selections can supply a fallback. Both panels currently have none.
            return clips.empty() && !tracks.empty() ? EditingArea::tracks : EditingArea::timeline;
        }

        static void click(ItemSelection& items, te::EditItemID id, bool toggle)
        {
            if (!toggle)
                items.clear();
            if (id.isValid() && (!toggle || items.erase(id) == 0))
                items.insert(id);
        }
    };

    struct ActionButton
    {
        ju::TextButton* button;
        UniversalAction action;
        ju::Colour colour;
    };

    struct ActionGroup
    {
        ju::GroupComponent frame;
        std::vector<ju::Component*> buttons;
    };

    struct AreaLabel final : ju::Label
    {
        void paintOverChildren(ju::Graphics& g) override
        {
            if (active)
            {
                g.setColour(ju::Colours::white);
                g.drawRect(getLocalBounds(), 2);
            }
        }

        void setActive(bool next)
        {
            if (active != next)
            {
                active = next;
                repaint();
            }
        }

        bool active = false;
    };

    struct PanelState
    {
        explicit PanelState(ju::PropertiesFile& settings)
            : props(settings), inspectorWidth(ju::jlimit(200, 600, props.getIntValue("inspectorWidth", 270))),
              workspaceHeight(ju::jlimit(100, 600, props.getIntValue("workspaceHeight", 190))),
              inspectorOpen(props.getBoolValue("inspectorOpen", true)),
              workspaceOpen(props.getBoolValue("workspaceOpen", true)),
              tallTracks(props.getBoolValue("tallTracks", false)) {}

        void save() const
        {
            props.setValue("inspectorWidth", inspectorWidth);
            props.setValue("workspaceHeight", workspaceHeight);
            props.setValue("inspectorOpen", inspectorOpen);
            props.setValue("workspaceOpen", workspaceOpen);
            props.setValue("tallTracks", tallTracks);
            props.saveIfNeeded();
        }

        ju::PropertiesFile& props;
        int inspectorWidth, workspaceHeight;
        bool inspectorOpen, workspaceOpen, tallTracks;
    };

    static ju::PropertiesFile::Options settingsOptions()
    {
        ju::PropertiesFile::Options options;
        options.applicationName = "SimpleDAW";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }

    struct Arrangement final : ju::Component, ju::ChangeListener
    {
        explicit Arrangement(MainGui& gui) : owner(gui)
        {
            formats.registerBasicFormats();
            setWantsKeyboardFocus(true);
            setTitle("Track list and arrangement");
        }

        struct Wave
        {
            Wave(Arrangement& arrangement, const ju::File& source)
                : file(source), modified(file.getLastModificationTime()), size(file.getSize()), thumbnail(512, arrangement.formats, arrangement.cache)
            {
                thumbnail.addChangeListener(&arrangement);
                thumbnail.setSource(new ju::FileInputSource(file, true));
            }

            ju::File file;
            ju::Time modified;
            ju::int64 size;
            ju::AudioThumbnail thumbnail;
        };

        void paint(ju::Graphics& g) override { owner.paintArrangement(g); }
        void paintOverChildren(ju::Graphics& g) override { owner.paintArrangementSelection(g); }
        void changeListenerCallback(ju::ChangeBroadcaster*) override { repaint(); }
        void mouseDown(const ju::MouseEvent& event) override { owner.arrangementDown(event); }
        void mouseDrag(const ju::MouseEvent& event) override { owner.arrangementDrag(event); }
        void mouseUp(const ju::MouseEvent& event) override { owner.arrangementUp(event); }
        void mouseWheelMove(const ju::MouseEvent&, const ju::MouseWheelDetails& wheel) override
        {
            if (wheel.deltaX != 0.0f)
                owner.scrollTime(-wheel.deltaX * getWidth() / owner.pixelsPerSecond);
            else if (wheel.deltaY != 0.0f)
                owner.scrollRows(wheel.deltaY > 0 ? -1 : 1);
        }

        ju::AudioThumbnail& waveform(te::EditItemID id, const ju::File& file)
        {
            auto& wave = waves[id];
            if (!wave || wave->file != file || wave->size != file.getSize()
                || wave->modified != file.getLastModificationTime())
                wave = std::make_unique<Wave>(*this, file);
            return wave->thumbnail;
        }

        MainGui& owner;
        ju::AudioFormatManager formats;
        ju::AudioThumbnailCache cache { 48 };
        std::map<te::EditItemID, std::unique_ptr<Wave>> waves;
    };

    enum class Arrow { screenLeft, screenRight, left, right, up, down };

    struct ArrowButton final : ju::TextButton
    {
        explicit ArrowButton(Arrow direction) : arrow(direction) {}

        void paintButton(ju::Graphics& g, bool hovered, bool pressed) override
        {
            ju::TextButton::paintButton(g, hovered, pressed);
            const bool horizontal = arrow != Arrow::up && arrow != Arrow::down;
            const bool reverse = arrow == Arrow::left || arrow == Arrow::screenLeft || arrow == Arrow::up;
            const bool doubleArrow = arrow == Arrow::screenLeft || arrow == Arrow::screenRight;
            const float sign = reverse ? -1.0f : 1.0f;
            const auto centre = getLocalBounds().toFloat().getCentre();
            g.setColour(ju::Colours::white);
            const auto drawTriangle = [&](float offset) {
                const auto x = centre.x + offset;
                const auto y = centre.y;
                ju::Path triangle;
                if (horizontal)
                    triangle.addTriangle(x - 5 * sign, y - 7, x - 5 * sign, y + 7, x + 6 * sign, y);
                else
                    triangle.addTriangle(x - 7, y - 5 * sign, x + 7, y - 5 * sign, x, y + 6 * sign);
                g.fillPath(triangle);
            };
            drawTriangle(doubleArrow ? -6.0f : 0.0f);
            if (doubleArrow)
                drawTriangle(6.0f);
        }

        Arrow arrow;
    };

    struct Splitter final : ju::Component
    {
        Splitter(MainGui& gui, bool vertical) : owner(gui), side(vertical)
        {
            setMouseCursor(side ? ju::MouseCursor::LeftRightResizeCursor
                                : ju::MouseCursor::UpDownResizeCursor);
        }
        void paint(ju::Graphics& g) override { g.fillAll(ju::Colour(0xff444444)); }
        void mouseDown(const ju::MouseEvent&) override
        {
            initial = side ? owner.inspector.getWidth() : owner.workspace.getHeight();
        }
        void mouseDrag(const ju::MouseEvent& event) override
        {
            if (side)
                owner.panels.inspectorWidth = ju::jlimit(200, owner.maxInspector(), initial - event.getDistanceFromDragStartX());
            else
                owner.panels.workspaceHeight = ju::jlimit(100, owner.maxWorkspace(), initial - event.getDistanceFromDragStartY());
            owner.resized();
        }
        void mouseUp(const ju::MouseEvent&) override { owner.panels.save(); }
        MainGui& owner;
        bool side;
        int initial = 0;
    };

    struct Meter
    {
        explicit Meter(te::LevelMeterPlugin& plugin) : source(&plugin.measurer) { source->addClient(client); }
        ~Meter() { if (source != nullptr) source->removeClient(client); }
        ju::WeakReference<te::LevelMeasurer> source;
        te::LevelMeasurer::Client client;
        float level = 0.0f;
    };

public:
    explicit MainGui(te::Engine& e, ju::File preferences = {})
        : engine(e), settings(preferences == ju::File{} ? settingsOptions().getDefaultFile() : preferences,
                              settingsOptions()), panels(settings)
    {
        setWantsKeyboardFocus(true);
        commandManager.registerAllCommandsForTarget(this);
        commandManager.setFirstCommandTarget(this);
        setApplicationCommandManagerToWatch(&commandManager);
        addAndMakeVisible(menuBar);
        addAndMakeVisible(arrangement);
        addAndMakeVisible(inspector);
        addAndMakeVisible(workspace);
        addAndMakeVisible(inspectorDivider);
        addAndMakeVisible(workspaceDivider);
        for (ju::Label* label : std::array<ju::Label*, 4> { &position, &status, &inspector, &workspace })
        {
            addAndMakeVisible(*label);
            label->setColour(ju::Label::textColourId, ju::Colours::white);
            label->setFont(ju::FontOptions(16.0f, ju::Font::bold));
        }
        position.setFont(ju::FontOptions(21.0f, ju::Font::bold));
        position.setJustificationType(ju::Justification::centred);
        position.setColour(ju::Label::backgroundColourId, ju::Colour(0xff122844));
        inspector.setColour(ju::Label::backgroundColourId, ju::Colour(0xff292929));
        inspector.setJustificationType(ju::Justification::centredTop);
        workspace.setColour(ju::Label::backgroundColourId, ju::Colour(0xff252525));
        workspace.setJustificationType(ju::Justification::centred);

        for (auto* panel : { &inspector, &workspace })
        {
            panel->setInterceptsMouseClicks(true, false);
            panel->addMouseListener(this, false);
        }

        button(first, "RECORD", [this] { record(); }, 0xffa60000);
        button(first, "STOP", [this] { stop(); });
        playButton = &button(first, "PLAY", [this] { play(); }, 0xff006e17);
        pauseButton = &button(first, "PAUSE", [this] { pause(); }, 0xff735100);
        loopButton = &button(first, "LOOP", [this] { toggleLoop(); }, 0xff18395c);
        first.push_back(&position);
        addAndMakeVisible(tempo);
        tempo.setJustification(ju::Justification::centred);
        tempo.setFont(ju::FontOptions(16.0f, ju::Font::bold));
        tempo.setTitle("Tempo");
        tempo.setTooltip("Tempo in BPM. Press Enter to apply.");
        tempo.onFocusLost = [this] { updateControls(); };
        tempo.onReturnKey = [this] { applyTempo(); };
        first.push_back(&tempo);
        combo(signature, first, { "2/4", "3/4", "4/4", "5/4", "6/8", "7/8", "9/8", "12/8" }, "Time signature");
        signature.onChange = [this] {
            if (ready() && signature.getSelectedId() != 0)
            {
                edit->getUndoManager().beginNewTransaction("Time signature");
                edit->tempoSequence.getTimeSigAt(editCursor).setStringTimeSig(signature.getText());
            }
        };

        button(second, "START", [this] { seek(0.0); });
        button(second, "PREV BAR", [this] { seekBar(-1); });
        button(second, "NEXT BAR", [this] { seekBar(1); });
        button(second, "END", [this] { if (edit) seek(edit->getLength().inSeconds(), false); });
        combo(recordMode, second, { "NORMAL", "TIME-SELECTION PUNCH" }, "Recording mode");
        recordMode.onChange = [this] {
            if (ready())
            {
                edit->recordingPunchInOut = recordMode.getSelectedId() == 2;
                edit->markAsChanged();
                if (edit->recordingPunchInOut)
                    edit->getTransport().looping = false;
            }
        };
        clickButton = &button(second, "METRONOME", [this] {
            if (edit)
            {
                edit->clickTrackEnabled = !edit->clickTrackEnabled.get();
                edit->markAsChanged();
            }
        });
        countButton = &button(second, "COUNT-IN", [this] {
            if (ready())
                edit->setCountInMode(edit->getCountInMode() == te::Edit::CountIn::none
                    ? te::Edit::CountIn::oneBar : te::Edit::CountIn::none);
        });
        combo(snap, second, { "SNAP OFF", "SNAP 1/4", "SNAP 1/8", "SNAP 1/16" }, "Timeline snap");
        snap.setSelectedId(4, ju::dontSendNotification);

        using Action = UniversalAction;
        addActionGroup(0, "EDIT", 0xff263e53, { { "CUT", Action::cut }, { "DELETE", Action::remove },
                                                { "COPY", Action::copy }, { "PASTE", Action::paste } });
        addActionGroup(1, "MOVE", 0xff2b4257, { { "↑  UP", Action::up }, { "↓  DOWN", Action::down },
                                                { "←  LEFT", Action::left }, { "→  RIGHT", Action::right } });
        addActionGroup(2, "SIZE", 0xff30465a, { { "START −", Action::startMinus }, { "START +", Action::startPlus },
                                                { "END −", Action::endMinus }, { "END +", Action::endPlus } });
        addActionGroup(3, "MODIFY", 0xff354a5e, { { "SPLIT", Action::split }, { "", Action::reserved },
                                                  { "TRIM", Action::trim }, { "", Action::reserved } });
        undoButton = &button(historyButtons, "UNDO", [this] { useHistory(false); });
        redoButton = &button(historyButtons, "REDO", [this] { useHistory(true); });
        for (auto* control : historyButtons)
            control->setWantsKeyboardFocus(false);
        snap.onChange = [this] { updateControls(); };
        button(panelButtons, "INSPECTOR", [this] { panels.inspectorOpen = !panels.inspectorOpen; saveLayout(); });
        button(panelButtons, "WORKSPACE", [this] { panels.workspaceOpen = !panels.workspaceOpen; saveLayout(); });

        navigationButton(Arrow::screenLeft, "Scroll left one screen", [this] { scrollTime(-visibleSeconds()); });
        navigationButton(Arrow::screenRight, "Scroll right one screen", [this] { scrollTime(visibleSeconds()); });
        navigationButton(Arrow::left, "Scroll left one bar", [this] { scrollBar(-1); });
        navigationButton(Arrow::right, "Scroll right one bar", [this] { scrollBar(1); });
        auto& zoomOut = button(navigation, "-", [this] { zoom(0.8); }, 0xff193c57);
        zoomOut.setTooltip("Zoom out horizontally");
        zoomOut.setRepeatSpeed(330, 110, 65);
        auto& zoomIn = button(navigation, "+", [this] { zoom(1.25); }, 0xff193c57);
        zoomIn.setTooltip("Zoom in horizontally");
        zoomIn.setRepeatSpeed(330, 110, 65);
        navigationButton(Arrow::up, "Scroll up", [this] { scrollRows(-1); });
        navigationButton(Arrow::down, "Scroll down", [this] { scrollRows(1); });
        heightButton = &button(navigation, "MIN", [this] {
            panels.tallTracks = !panels.tallTracks;
            saveLayout();
        });
        followButton = &button(navigation, "FOLLOW", [this] { follow = !follow; });
        for (auto label : { "MIXER", "EFFECTS", "PIANO ROLL", "DRUM EDITOR" })
        {
            const auto index = tabs.size();
            button(tabs, label, [this, index] { activeTab = index; updateControls(); });
        }
        report("Create or open a project to begin");
        setSize(1440, 850);
        updateControls();
        startTimerHz(30);
    }

    ~MainGui() override
    {
        stopTimer();
        if (shortcutHost != nullptr)
            shortcutHost->removeKeyListener(commandManager.getKeyMappings());
        setApplicationCommandManagerToWatch(nullptr);
        menuBar.setModel(nullptr);
        stop();
        meters.clear();
    }

    void installWindowShortcuts(ju::Component& host)
    {
        shortcutHost = &host;
        host.addKeyListener(commandManager.getKeyMappings());
    }

    bool prepareToQuit()
    {
        if (edit && edit->getTransport().isPlaying())
            stop();
        return confirmProjectChanges();
    }

    void paint(ju::Graphics& g) override { g.fillAll(ju::Colours::black); }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5);
        menuBar.setBounds(area.removeFromTop(28));
        layoutRow(first, area.removeFromTop(42), { 10, 10, 10, 10, 10, 25, 12, 13 }, { 5, 6 });
        layoutRow(second, area.removeFromTop(42), { 10, 10, 10, 10, 24, 13, 11, 12 }, { 4, 5 });
        area.removeFromTop(4);
        auto toolbar = area.removeFromTop(112);
        auto panelControls = toolbar.removeFromRight(122).withTrimmedLeft(10).withTrimmedTop(18);
        for (auto* control : panelButtons)
        {
            control->setBounds(panelControls.removeFromTop(42));
            panelControls.removeFromTop(4);
        }
        auto historyControls = toolbar.removeFromRight(106).withTrimmedLeft(7).withTrimmedTop(17);
        for (auto* control : historyButtons)
        {
            control->setBounds(historyControls.removeFromTop(42).reduced(2));
            historyControls.removeFromTop(4);
        }
        for (size_t i = 0; i < actionGroups.size(); ++i)
        {
            auto& group = actionGroups[i];
            auto bounds = toolbar.removeFromLeft(toolbar.getWidth() / static_cast<int>(actionGroups.size() - i));
            group.frame.setBounds(bounds.withTrimmedRight(5));
            bounds = bounds.withTrimmedTop(17).withTrimmedBottom(6).reduced(9, 0);
            layoutRow({ group.buttons[0], group.buttons[1] }, bounds.removeFromTop(42));
            bounds.removeFromTop(4);
            layoutRow({ group.buttons[2], group.buttons[3] }, bounds.removeFromTop(42));
        }
        area.removeFromTop(4);
        status.setBounds(area.removeFromBottom(26));
        layoutRow(navigation, area.removeFromBottom(40), { 8, 8, 8, 8, 8, 8, 8, 8, 13, 13 }, { 4, 6, 8 });

        inspector.setVisible(panels.inspectorOpen);
        inspectorDivider.setVisible(panels.inspectorOpen);
        if (panels.inspectorOpen)
        {
            inspector.setBounds(area.removeFromRight(ju::jmin(panels.inspectorWidth, maxInspector())));
            inspectorDivider.setBounds(area.removeFromRight(8));
        }

        workspace.setVisible(panels.workspaceOpen);
        workspaceDivider.setVisible(panels.workspaceOpen);
        for (auto* tab : tabs)
            tab->setVisible(panels.workspaceOpen);
        if (panels.workspaceOpen)
        {
            auto bottom = area.removeFromBottom(ju::jmin(panels.workspaceHeight, maxWorkspace()));
            auto tabBar = bottom.removeFromTop(36);
            layoutRow(tabs, tabBar.removeFromLeft(ju::jmin(600, tabBar.getWidth())));
            workspace.setBounds(bottom);
            workspaceDivider.setBounds(area.removeFromBottom(8));
        }

        arrangement.setBounds(area);
        scrollRows(0);
    }

    static bool test(te::Engine& engine)
    {
        auto directory = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-gui-test", {});
        if (directory.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { directory.deleteRecursively(); } };
        auto preferences = directory.getChildFile("gui.settings");
        auto project = directory.getChildFile("test.tracktionedit");
        {
            MainGui gui(engine, preferences);
            gui.setProject(te::createEmptyEdit(engine, project), project);
            const auto initialTracks = te::getAudioTracks(*gui.edit).size();
            if (!gui.arrangement.isVisible() || !gui.inspector.isVisible() || gui.arrangement.getHeight() < 200)
                return false;
            gui.commandManager.invokeDirectly(audioTrack, false);
            gui.commandManager.invokeDirectly(midiTrack, false);
            if (te::getAudioTracks(*gui.edit).size() != initialTracks + 2 || gui.selection != initialTracks + 1)
                return false;
            gui.seekBar(1);
            if (gui.editCursor.inSeconds() <= 0 || gui.edit->getTransport().getPosition() != gui.editCursor)
                return false;
            gui.toggleLoop();
            if (!gui.edit->getTransport().looping.get())
                return false;
            gui.commandManager.invokeDirectly(midiItem, false);
            if (gui.trackAt(gui.selection)->getClips().size() != 1 || !gui.selectedClip())
                return false;
            const auto cursorX = gui.timeX(gui.editCursor);
            gui.zoom(2.0);
            if (std::abs(gui.timeX(gui.editCursor) - cursorX) > 1)
                return false;
            if (!gui.save())
                return false;
            gui.panels.inspectorOpen = false;
            gui.panels.workspaceHeight = 160;
            gui.saveLayout();
        }
        MainGui gui(engine, preferences);
        gui.setProject(te::loadEditFromFile(engine, project), project);
        const auto tracks = te::getAudioTracks(*gui.edit);
        return !gui.inspector.isVisible() && gui.workspace.getHeight() == 124
            && tracks.size() >= 2 && tracks.getLast()->getClips().size() == 1;
    }

    static bool testUniversalActions(te::Engine& engine)
    {
        using Action = UniversalAction;
        auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-universal-actions", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        const auto project = dir.getChildFile("test.tracktionedit");
        const auto audio = dir.getChildFile("test.wav");
        const auto check = [](bool passed, const char* step) {
            if (!passed)
                std::fprintf(stderr, "Universal Actions: %s\n", step);
            return passed;
        };
        MainGui gui(engine, dir.getChildFile("gui.settings"));
        if (!check(gui.actionButtons.size() == 16 && std::none_of(gui.actionButtons.begin(), gui.actionButtons.end(),
                [](const auto& slot) { return slot.button->isEnabled(); }), "four fixed groups and inert initial state"))
            return false;

        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.edit->ensureNumberOfAudioTracks(2);
        gui.snap.setSelectedId(1, ju::dontSendNotification);
        gui.panels.inspectorOpen = gui.panels.workspaceOpen = true;
        gui.resized();
        auto* first = gui.trackAt(0);
        auto* second = gui.trackAt(1);
        const auto seconds = te::TimePosition::fromSeconds<double>;
        auto midi = te::insertMIDIClip(*first, "Test MIDI", { seconds(1), seconds(2) });
        if (!check(midi && writeTestWave(audio) && insertAudio(*second, audio, seconds(3)), "create MIDI and WAV"))
            return false;
        const auto midiID = midi->itemID;
        midi = nullptr; // The test replaces the Edit, so keep only the fixture's ID.
        const auto audioID = second->getClips().getFirst()->itemID;
        gui.updateControls();

        const auto mouse = [&](ju::Component& component, ju::Point<int> where, int modifiers,
                               ju::Point<int> down, bool dragged = false) {
            const auto now = ju::Time::getCurrentTime();
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), where.toFloat(),
                ju::ModifierKeys(modifiers), 0, 0, 0, 0, 0, &component, &component, now, down.toFloat(), now, 1, dragged);
        };
        const auto click = [&](ju::Point<int> at, bool right = false) {
            gui.arrangement.mouseDown(mouse(gui.arrangement, at, right ? ju::ModifierKeys::rightButtonModifier
                                                                       : ju::ModifierKeys::leftButtonModifier, at));
            gui.arrangement.mouseUp(mouse(gui.arrangement, at, 0, at));
        };
        const auto rowY = [&](int row) { return rulerHeight + row * gui.rowHeight() + gui.rowHeight() / 2; };
        const auto midiPoint = ju::Point<int>(gui.timeX(seconds(1.1)), rowY(0));
        const auto audioPoint = ju::Point<int>(gui.timeX(seconds(3.05)), rowY(1));
        click(midiPoint);
        if (!check(gui.activeEditingArea() == EditingArea::timeline && gui.canDispatch(Action::copy)
                && gui.canDispatch(Action::remove) && !gui.canDispatch(Action::paste), "single clip enables clipboard and Delete"))
            return false;
        click(audioPoint, true);
        if (!check(gui.actionSelection.clips == ItemSelection { midiID, audioID }
                && gui.canDispatch(Action::copy) && gui.canDispatch(Action::remove), "right-click adds a second clip"))
            return false;
        click(audioPoint, true);
        if (!check(gui.actionSelection.clips == ItemSelection { midiID }, "second right-click toggles the clip off"))
            return false;

        const auto from = ju::Point<int>(gui.timeX(seconds(0.5)), rowY(0));
        const auto to = ju::Point<int>(gui.timeX(seconds(3.15)), rowY(1));
        gui.arrangement.mouseDown(mouse(gui.arrangement, from, ju::ModifierKeys::leftButtonModifier, from));
        gui.arrangement.mouseDrag(mouse(gui.arrangement, to, ju::ModifierKeys::leftButtonModifier, from, true));
        if (!check(!gui.selectionBox.isEmpty() && gui.actionSelection.clips == ItemSelection { midiID, audioID },
                   "empty-space drag previews intersecting clips"))
            return false;
        gui.arrangement.mouseUp(mouse(gui.arrangement, to, 0, from, true));
        if (!check(gui.selectionBox.isEmpty() && gui.actionSelection.clips.size() == 2, "release retains box selection"))
            return false;

        click({ 30, rowY(0) });
        click({ 30, rowY(1) }, true);
        if (!check(gui.activeEditingArea() == EditingArea::tracks && gui.actionSelection.tracks.size() == 2
                && gui.actionSelection.clips.size() == 2 && gui.canDispatch(Action::remove)
                && !gui.canDispatch(Action::copy), "separate track focus and selection"))
            return false;
        const auto trackFrom = ju::Point<int>(20, rulerHeight + 1);
        const auto trackTo = ju::Point<int>(70, rowY(1));
        gui.arrangement.mouseDown(mouse(gui.arrangement, trackFrom, ju::ModifierKeys::leftButtonModifier, trackFrom));
        gui.arrangement.mouseUp(mouse(gui.arrangement, trackTo, 0, trackFrom, true));
        if (!check(gui.actionSelection.tracks.size() == 2, "track box selects both headers"))
            return false;

        click(midiPoint);
        gui.dispatchAction(Action::copy);
        if (!check(gui.actionSelection.clips == ItemSelection { midiID } && gui.canDispatch(Action::paste), "copy retains selection"))
            return false;
        gui.mouseDown(mouse(gui.inspector, { 10, 10 }, ju::ModifierKeys::leftButtonModifier, { 10, 10 }));
        if (!check(gui.activeEditingArea() == EditingArea::inspector && !gui.canDispatch(Action::paste),
                   "Inspector focus blocks timeline editing"))
            return false;
        gui.activeTab = 1;
        gui.mouseDown(mouse(gui.workspace, { 10, 10 }, ju::ModifierKeys::leftButtonModifier, { 10, 10 }));
        if (!check(gui.activeEditingArea() == EditingArea::window && !gui.canDispatch(Action::copy),
                   "Effects can take focus without exposing selectable items"))
            return false;
        gui.panels.workspaceOpen = false;
        gui.saveLayout();
        if (!check(gui.activeEditingArea() == EditingArea::timeline && gui.actionSelection.clips == ItemSelection { midiID },
                   "hiding Effects falls back to Timeline before Tracks"))
            return false;
        gui.panels.workspaceOpen = true;
        gui.saveLayout();
        if (!check(gui.activeEditingArea() == EditingArea::timeline, "showing Effects preserves current focus"))
            return false;

        click({ gui.timeX(seconds(4.13)), rowY(1) });
        if (!check(gui.actionSelection.clips.empty() && gui.canDispatch(Action::paste),
                   "empty lane click clears clips and sets paste destination"))
            return false;
        const auto destination = gui.editCursor;
        gui.dispatchAction(Action::paste);
        auto* pasted = gui.selectedClip();
        if (!check(dynamic_cast<te::MidiClip*>(pasted) && pasted->itemID != midiID
                && second->getClips().size() == 2 && std::abs((pasted->getPosition().getStart() - destination).inSeconds()) < 0.000001,
                   "MIDI Paste creates and selects an independent clip at the unsnapped cursor"))
            return false;
        const auto pasteID = pasted->itemID;
        gui.commandManager.invokeDirectly(undo, false);
        if (!check(second->getClips().size() == 1 && gui.actionSelection.clips.empty(), "one undo removes paste"))
            return false;
        gui.commandManager.invokeDirectly(redo, false);
        if (!check(te::findClipForID(*gui.edit, pasteID) && second->getClips().size() == 2, "redo restores paste"))
            return false;

        click(audioPoint);
        gui.dispatchAction(Action::copy);
        gui.dispatchAction(Action::remove);
        if (!check(!te::findClipForID(*gui.edit, audioID) && gui.actionSelection.clips.empty()
                && te::findClipForID(*gui.edit, pasteID)->getPosition().getStart() == destination, "Delete leaves the gap"))
            return false;
        gui.commandManager.invokeDirectly(undo, false);
        if (!check(te::findClipForID(*gui.edit, audioID) && second->getClips().size() == 2, "undo restores audio clip"))
            return false;

        gui.snap.setSelectedId(4, ju::dontSendNotification);
        click({ gui.timeX(seconds(2.71)), rowY(0) });
        gui.seek(2.71, false);
        const auto snapped = gui.pasteStart();
        gui.dispatchAction(Action::paste);
        auto* pastedWave = dynamic_cast<te::WaveAudioClip*>(gui.selectedClip());
        if (!check(pastedWave && pastedWave->itemID != audioID && pastedWave->getOriginalFile() == audio
                && pastedWave->getPosition().getStart() == snapped && first->getClips().size() == 2,
                   "WAV Paste preserves media and snaps on the destination lane"))
            return false;

        const auto clipCount = first->getClips().size();
        for (const auto& slot : gui.actionButtons)
            if (slot.action == Action::split || slot.action == Action::trim || slot.action == Action::reserved)
            {
                if (!check(!slot.button->isEnabled() && !gui.canDispatch(slot.action), "deferred actions stay disabled"))
                    return false;
                gui.dispatchAction(slot.action);
            }
        if (!check(first->getClips().size() == clipCount, "disabled actions leave clips alone"))
            return false;
        gui.edit->ensureNumberOfAudioTracks(4);
        gui.actionSelection.clips = { midiID, audioID };
        gui.selection = 3;
        gui.dispatchAction(Action::copy);
        if (!check(!gui.canDispatch(Action::paste) && !gui.canDispatch(Action::up),
                   "missing destination rejects a group before mutation"))
            return false;
        gui.dispatchAction(Action::paste);
        if (!check(gui.trackAt(3)->getClips().isEmpty(), "rejected paste leaves the destination empty"))
            return false;

        gui.dispatchAction(Action::cut);
        if (!check(!te::findClipForID(*gui.edit, midiID) && !te::findClipForID(*gui.edit, audioID),
                   "cut removes both selected clips"))
            return false;
        gui.useHistory(false);
        if (!check(te::findClipForID(*gui.edit, midiID) && te::findClipForID(*gui.edit, audioID),
                   "one global undo restores both cut clips"))
            return false;
        gui.selection = 2;
        gui.seek(5.15, false);
        const auto groupAnchor = gui.pasteStart();
        gui.dispatchAction(Action::paste);
        if (!check(gui.actionSelection.clips.size() == 2 && gui.trackAt(2)->getClips().size() == 1
                && gui.trackAt(3)->getClips().size() == 1
                && gui.trackAt(2)->getClips().getFirst()->getPosition().getStart() == groupAnchor
                && std::abs((gui.trackAt(3)->getClips().getFirst()->getPosition().getStart() - groupAnchor).inSeconds() - 2.0) < 0.000001,
                   "paste preserves timing, track offsets and selection"))
            return false;

        gui.snap.setSelectedId(1, ju::dontSendNotification);
        if (!check(!gui.canDispatch(Action::left) && !gui.canDispatch(Action::endMinus)
                && gui.canDispatch(Action::up), "Snap Off leaves vertical movement available"))
            return false;
        gui.snap.setSelectedId(4, ju::dontSendNotification);
        if (!check(!gui.canDispatch(Action::endPlus), "source limit rejects SIZE for the entire group"))
            return false;

        gui.dispatchAction(Action::left);
        const auto movedStart = gui.trackAt(2)->getClips().getFirst()->getPosition().getStart();
        if (!check(movedStart < groupAnchor, "horizontal MOVE steps left"))
            return false;
        gui.dispatchAction(Action::up);
        const auto clipsOnSecond = gui.trackAt(1)->getClips();
        if (!check(gui.trackAt(3)->getClips().isEmpty()
                && std::any_of(clipsOnSecond.begin(), clipsOnSecond.end(),
                               [movedStart](auto* clip) { return clip->getPosition().getStart() == movedStart; }),
                   "vertical MOVE preserves time and changes rows"))
            return false;
        gui.useHistory(false);
        if (!check(gui.trackAt(3)->getClips().size() == 1, "one undo restores both rows"))
            return false;
        gui.useHistory(false);
        const auto* pastedMidi = gui.trackAt(2)->getClips().getFirst();
        if (!check(pastedMidi && pastedMidi->getPosition().getStart() == groupAnchor, "one undo restores group timing"))
            return false;

        gui.actionSelection.clips = { pastedMidi->itemID };
        const auto oldPosition = pastedMidi->getPosition();
        gui.dispatchAction(Action::startPlus);
        if (!check(pastedMidi->getPosition().getStart() > oldPosition.getStart()
                && pastedMidi->getPosition().getEnd() == oldPosition.getEnd()
                && pastedMidi->getPosition().getStartOfSource() == oldPosition.getStartOfSource(),
                   "START + preserves end and source alignment"))
            return false;
        gui.useHistory(false);
        if (!check(pastedMidi->getPosition() == oldPosition && gui.canUseHistory(true), "one undo restores trim"))
            return false;

        const auto secondTrackID = gui.trackAt(1)->itemID;
        const auto fourthTrackID = gui.trackAt(3)->itemID;
        gui.actionSelection.requestedArea = EditingArea::tracks;
        gui.actionSelection.tracks = { secondTrackID, fourthTrackID };
        gui.dispatchAction(Action::up);
        if (!check(gui.trackAt(0)->itemID == secondTrackID && gui.trackAt(1)->itemID == fourthTrackID
                && gui.actionSelection.clips.size() == 1 && !gui.canDispatch(Action::startPlus),
                   "track MOVE makes a contiguous block and preserves the Timeline selection"))
            return false;
        gui.useHistory(false);
        if (!check(gui.trackAt(1)->itemID == secondTrackID && gui.trackAt(3)->itemID == fourthTrackID,
                   "one global undo restores track order"))
            return false;
        gui.setProject(te::createEmptyEdit(engine, project), project);
        return check(!gui.clipClipboard && gui.actionSelection.clips.empty(), "opening a project clears clipboard and selection");
    }

    static bool testCorrections(te::Engine& engine)
    {
        auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory).getNonexistentChildFile("SimpleDAW-corrections", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        MainGui gui(engine, dir.getChildFile("ui.settings"));
        const auto project = dir.getChildFile("corrections.tracktionedit");
        const auto audio = dir.getChildFile("source.wav");
        if (!writeTestWave(audio))
            return false;

        const auto check = [](bool passed, const char* name) {
            if (!passed)
                std::fprintf(stderr, "Correction check failed: %s\n", name);
            return passed;
        };
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.addTrack(false);
        const int firstIndex = gui.selection;
        auto* firstTrack = gui.trackAt(firstIndex);
        if (!check(firstTrack && insertBuiltIn<te::ReverbPlugin>(*firstTrack), "install track effect"))
            return false;
        gui.addTrack(false);
        const int secondIndex = gui.selection;
        if (!check(gui.selectedEffects().contains("No effects"), "new track effect independence"))
            return false;
        gui.selectTrack(firstIndex);
        if (!check(!gui.selectedEffects().contains("No effects"), "original effect chain remains")
            || !check(gui.hasProjectChanges(), "track edits mark project dirty")
            || !check(gui.save(), "save project after track edits")
            || !check(!gui.hasProjectChanges(), "saving clears dirty state"))
            return false;

        const auto from = te::TimePosition::fromSeconds(gui.snapSeconds(0.68));
        if (!check(insertAudio(*firstTrack, audio, from), "import WAV"))
            return false;
        auto* wave = dynamic_cast<te::WaveAudioClip*>(firstTrack->getClips().getLast());
        if (!check(wave && wave->getPosition().getStart() == from, "import at snapped edit cursor"))
            return false;
        const auto id = wave->itemID;
        gui.actionSelection.clips = { id };
        gui.dragItem = id;
        gui.dragAnchor = 0.0;
        gui.updateDragPreview(0.27);
        const auto moved = te::TimePosition::fromSeconds(gui.snapSeconds(from.inSeconds() + 0.27));
        if (!check(gui.dragPreview == moved && moved != from && wave->getPosition().getStart() == from,
                   "snapped live preview leaves edit alone"))
            return false;
        gui.finishDrag();
        if (!check(wave->getPosition().getStart() == moved, "commit clip preview"))
            return false;
        gui.dragItem = {};
        gui.dragPreview.reset();
        gui.draggingRange = true;
        gui.dragAnchor = 0.125;
        const auto previousRange = gui.timeSelection;
        gui.updateDragPreview(1.01);
        if (!check(gui.rangePreview.has_value() && gui.timeSelection == previousRange, "live range preview"))
            return false;
        gui.finishDrag();
        if (!check(gui.timeSelection == gui.rangePreview, "commit selected time range"))
            return false;

        const auto clipPosition = wave->getPosition();
        const auto missing = dir.getChildFile("missing.wav");
        replaceWaveSource(*wave, missing);
        ju::PluginDescription unavailable;
        unavailable.name = "Unavailable test VST3";
        unavailable.pluginFormatName = "VST3";
        unavailable.fileOrIdentifier = dir.getChildFile("Unavailable.vst3").getFullPathName();
        unavailable.uniqueId = 121;
        auto pluginState = te::ExternalPlugin::create(engine, unavailable);
        pluginState.setProperty(te::IDs::wet, 0.42f, nullptr);
        if (!check(gui.trackAt(secondIndex)->pluginList.insertPlugin(pluginState, 0) != nullptr
                   && gui.save(), "save unresolved media and plugin"))
            return false;

        gui.setProject(te::loadEditFromFile(engine, project), project);
        wave = missingWave(te::findClipForID(*gui.edit, id));
        if (!check(wave && wave->getOriginalFile() == missing && wave->getPosition() == clipPosition,
                   "reopen missing audio with original reference and position")
            || !check(unavailablePlugins(*gui.edit).joinIntoString(" ").contains(unavailable.name),
                      "reopen unavailable plugin"))
            return false;
        replaceWaveSource(*wave, audio);
        if (!check(!missingWave(wave) && wave->getPosition() == clipPosition && gui.save(), "relink and save"))
            return false;

        gui.setProject(te::loadEditFromFile(engine, project), project);
        wave = dynamic_cast<te::WaveAudioClip*>(te::findClipForID(*gui.edit, id));
        if (!check(wave && !missingWave(wave) && wave->getOriginalFile() == audio, "reopen relinked audio"))
            return false;
        if (!check(unavailablePlugins(*gui.edit).joinIntoString(" ").contains(unavailable.name),
                   "keep unavailable plugin through save"))
            return false;
        for (auto* plugin : gui.trackAt(secondIndex)->pluginList.getPlugins())
            if (plugin->getName() == unavailable.name
                && !check(std::abs(static_cast<double>(plugin->state[te::IDs::wet]) - 0.42) < 0.001,
                          "retain unavailable plugin setting"))
                return false;

        auto& thumbnail = gui.arrangement.waveform(id, wave->getOriginalFile());
        const auto timeout = ju::Time::getMillisecondCounter() + 3000;
        while (!thumbnail.isFullyLoaded() && ju::Time::getMillisecondCounter() < timeout)
            ju::MessageManager::getInstance()->runDispatchLoopUntil(20);
        if (!check(thumbnail.getNumChannels() == 1 && thumbnail.getApproximatePeak() > 0.01f,
                   "reload non-flat waveform"))
            return false;
        std::fprintf(stderr, "Checking Delete shortcuts\n");
        if (!check(gui.testDeleteShortcut(), "Delete edits items and ranges while preserving text editing and undo"))
            return false;
        gui.stopTimer();
        gui.meters.clear();
        gui.arrangement.waves.clear();
        gui.edit.reset();
        std::fprintf(stderr, "Checking hosted input routes\n");
        return check(testInputRoutes(engine, dir), "Space transport and independent shared-input audio and recording");
    }

private:
    bool invokeShortcut(Command command, int key, bool down = true, ju::Component* origin = nullptr)
    {
        InvocationInfo invocation(command);
        invocation.invocationMethod = InvocationInfo::fromKeyPress;
        invocation.keyPress = ju::KeyPress(key);
        invocation.isKeyDown = down;
        invocation.originatingComponent = origin;
        return commandManager.invoke(invocation, false);
    }

    bool testDeleteShortcut()
    {
        const auto key = ju::KeyPress::deleteKey;
        if (commandManager.getKeyMappings()->findCommandForKeyPress(ju::KeyPress(key)) != deleteItem
            || !timeSelection)
            return false;
        selectTrack(0);
        insertMidi();
        auto* clip = selectedClip();
        if (!clip)
            return false;
        const auto id = clip->itemID;
        const auto range = timeSelection;
        if (!invokeShortcut(deleteItem, key, true, &tempo) || !selectedClip()
            || !invokeShortcut(deleteItem, key) || te::findClipForID(*edit, id) || timeSelection != range)
            return false;
        if (!commandManager.invokeDirectly(undo, false) || !te::findClipForID(*edit, id)
            || !invokeShortcut(deleteItem, key, false) || timeSelection != range)
            return false;
        if (!invokeShortcut(deleteItem, key) || timeSelection || !edit->getTransport().getLoopRange().isEmpty())
            return false;
        const auto clipCount = trackAt(0)->getClips().size();
        invokeShortcut(deleteItem, key);
        return trackAt(0)->getClips().size() == clipCount;
    }

    static bool testInputRoutes(te::Engine& engine, const ju::File& directory)
    {
        auto& manager = engine.getDeviceManager();
        auto& host = manager.getHostedAudioDeviceInterface();
        const te::HostedAudioDeviceInterface::Parameters params;
        host.initialise(params);
        host.prepareToPlay(params.sampleRate, params.blockSize);
        manager.dispatchPendingUpdates();
        for (int i = 0; i < manager.getNumWaveInDevices(); ++i)
            if (auto* wave = manager.getWaveInDevice(i))
                wave->setEnabled(true);
        manager.deviceManager.dispatchPendingMessages();
        manager.dispatchPendingUpdates();
        ju::MessageManager::getInstance()->runDispatchLoopUntil(100);

        MainGui gui(engine, directory.getChildFile("routes.settings"));
        const auto path = directory.getChildFile("routes.tracktionedit");
        gui.setProject(te::createEmptyEdit(engine, path), path);
        auto& routeEdit = gui.edit;
        routeEdit->ensureNumberOfAudioTracks(2);
        auto& transport = routeEdit->getTransport();
        transport.ensureContextAllocated();
        auto tracks = te::getAudioTracks(*routeEdit);
        auto inputs = routeEdit->getAllInputDevices();
        inputs.removeIf([](auto* input) { return input->getInputDevice().getDeviceType() != te::InputDevice::waveDevice; });
        if (inputs.isEmpty())
            return false;
        auto& input = *inputs.getFirst();
        auto& first = *tracks[0];
        auto& second = *tracks[1];
        input.getInputDevice().setMonitorMode(te::InputDevice::MonitorMode::automatic);
        std::fprintf(stderr, "Assigning two hosted routes\n");
        if (toggleInput(input, first).failed() || toggleInput(input, second).failed()
            || !isArmed(input, first.itemID) || !isArmed(input, second.itemID))
            return false;
        if (toggleInput(input, first).failed() || isArmed(input, first.itemID)
            || !isArmed(input, second.itemID) || toggleInput(input, first).failed())
            return false;
        auto* firstMeterPlugin = first.getLevelMeterPlugin();
        auto* secondMeterPlugin = second.getLevelMeterPlugin();
        if (!firstMeterPlugin || !secondMeterPlugin)
        {
            std::fprintf(stderr, "Tracks have no level meter plugins\n");
            return false;
        }
        Meter firstMeter(*firstMeterPlugin);
        Meter secondMeter(*secondMeterPlugin);
        routeEdit->setCountInMode(te::Edit::CountIn::none);
        std::fprintf(stderr, "Starting hosted audio and transport\n");
        const auto deviceID = input.getInputDevice().getDeviceID();
        std::jthread processing([&](std::stop_token stop) {
            ju::AudioBuffer<float> buffer(2, params.blockSize);
            ju::MidiBuffer midi;
            while (!stop.stop_requested())
            {
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                        buffer.setSample(channel, sample, sample % 32 < 16 ? 0.2f : -0.2f);
                midi.clear();
                host.processBlock(buffer, midi);
                ju::Thread::sleep(4);
            }
        });
        if (gui.commandManager.getKeyMappings()->findCommandForKeyPress(ju::KeyPress(ju::KeyPress::spaceKey)) != playStop
            || !gui.invokeShortcut(playStop, ju::KeyPress::spaceKey) || !transport.isPlaying())
            return false;
        const auto waitForLevels = [&](bool firstAudible) {
            for (int i = 0; i < 50; ++i)
            {
                ju::MessageManager::getInstance()->runDispatchLoopUntil(20);
                const auto audible = [](Meter& meter) {
                    return meter.client.getAndClearAudioLevel(0).dB > -55.0f;
                };
                const bool firstHasSignal = audible(firstMeter);
                const bool secondHasSignal = audible(secondMeter);
                if (firstHasSignal == firstAudible && secondHasSignal)
                    return true;
            }
            return false;
        };
        std::fprintf(stderr, "Checking live levels before route removal\n");
        if (!waitForLevels(true) || !transport.isPlaying())
        {
            std::fprintf(stderr, "Hosted playback failed to settle with both inputs active (playing=%d)\n",
                         static_cast<int>(transport.isPlaying()));
            return false;
        }
        const auto findInput = [&]() -> te::InputDeviceInstance* {
            for (auto* current : routeEdit->getAllInputDevices())
                if (current->getInputDevice().getDeviceID() == deviceID)
                    return current;
            return nullptr;
        };
        const auto hasRoutes = [&](bool firstArmed) {
            const auto current = findInput();
            return current && isArmed(*current, first.itemID) == firstArmed
                && isArmed(*current, second.itemID);
        };
        std::fprintf(stderr, "Removing one route during playback\n");
        auto* currentInput = findInput();
        if (!currentInput || toggleInput(*currentInput, first).failed() || !hasRoutes(false))
            return false;
        std::fprintf(stderr, "Checking live levels after route removal\n");
        const bool levelsCorrect = waitForLevels(false);
        const bool routesCorrect = hasRoutes(false);
        if (!levelsCorrect || !routesCorrect || !transport.isPlaying())
        {
            std::fprintf(stderr, "After removal: levels=%d routes=%d playing=%d\n",
                         static_cast<int>(levelsCorrect), static_cast<int>(routesCorrect),
                         static_cast<int>(transport.isPlaying()));
            return false;
        }
        std::fprintf(stderr, "Stopping playback and starting hosted recording\n");
        if (!gui.invokeShortcut(playStop, ju::KeyPress::spaceKey, false) || !transport.isPlaying()
            || !gui.invokeShortcut(playStop, ju::KeyPress::spaceKey) || transport.isPlaying())
            return false;
        currentInput = findInput();
        if (!currentInput || toggleInput(*currentInput, first).failed())
            return false;
        transport.record(false);
        bool bothRecording = false;
        for (auto* current : routeEdit->getAllInputDevices())
            if (current->getInputDevice().getDeviceID() == deviceID)
            {
                const auto firstFile = current->getRecordingFile(first.itemID);
                const auto secondFile = current->getRecordingFile(second.itemID);
                bothRecording = transport.isRecording() && firstFile != ju::File{}
                    && secondFile != ju::File{} && firstFile != secondFile;
                break;
            }
        transport.stop(true, false);
        return bothRecording;
    }

    ju::TextButton& button(std::vector<ju::Component*>& row, const ju::String& label,
                           std::function<void()> action, uint32_t colour = 0xff292929)
    {
        auto& control = *buttons.add(new ju::TextButton(label));
        control.setColour(ju::TextButton::buttonColourId, ju::Colour(colour));
        control.setColour(ju::TextButton::buttonOnColourId, ju::Colour(0xff075ba8));
        control.setColour(ju::TextButton::textColourOffId, ju::Colours::white);
        control.setColour(ju::TextButton::textColourOnId, ju::Colours::white);
        control.onClick = std::move(action);
        addAndMakeVisible(control);
        row.push_back(&control);
        return control;
    }

    void addActionGroup(size_t index, const ju::String& title, uint32_t colour,
                        std::initializer_list<std::pair<const char*, UniversalAction>> actions)
    {
        auto& group = actionGroups[index];
        group.frame.setText(title);
        group.frame.setColour(ju::GroupComponent::outlineColourId, ju::Colour(0xff56616a));
        group.frame.setColour(ju::GroupComponent::textColourId, ju::Colours::white);
        group.frame.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(group.frame);
        for (const auto& [label, action] : actions)
        {
            auto& control = button(group.buttons, ju::String::fromUTF8(label), {}, colour);
            control.setWantsKeyboardFocus(false);
            if (action == UniversalAction::reserved)
                control.setTitle("Reserved");
            else
                control.onClick = [this, action] { dispatchAction(action); };
            actionButtons.push_back({ &control, action, ju::Colour(colour) });
        }
    }

    void navigationButton(Arrow arrow, const ju::String& title, std::function<void()> action)
    {
        auto& control = *buttons.add(new ArrowButton(arrow));
        control.setTitle(title);
        control.setTooltip(title);
        control.setColour(ju::TextButton::buttonColourId, ju::Colour(0xff292929));
        control.setRepeatSpeed(330, 110, 65);
        control.onClick = std::move(action);
        addAndMakeVisible(control);
        navigation.push_back(&control);
    }

    void combo(ju::ComboBox& control, std::vector<ju::Component*>& row,
               std::initializer_list<const char*> items, const ju::String& title)
    {
        int id = 0;
        for (auto item : items)
            control.addItem(item, ++id);
        control.setTitle(title);
        control.setTooltip(title);
        control.setJustificationType(ju::Justification::centred);
        addAndMakeVisible(control);
        row.push_back(&control);
    }

    static void layoutRow(const std::vector<ju::Component*>& row, ju::Rectangle<int> bounds,
                          std::initializer_list<int> weights = {},
                          std::initializer_list<size_t> breaks = {})
    {
        constexpr int groupGap = 8;
        const auto width = bounds.getWidth() - static_cast<int>(breaks.size()) * groupGap;
        const auto total = weights.size() == 0 ? static_cast<int>(row.size())
                                              : std::accumulate(weights.begin(), weights.end(), 0);
        int accumulated = 0;
        auto weight = weights.begin();
        auto groupBreak = breaks.begin();
        int offset = bounds.getX();
        for (size_t i = 0; i < row.size(); ++i)
        {
            if (groupBreak != breaks.end() && *groupBreak == i)
            {
                offset += groupGap;
                ++groupBreak;
            }
            const auto left = accumulated * width / total;
            accumulated += weights.size() == 0 ? 1 : *weight++;
            const auto right = accumulated * width / total;
            row[i]->setBounds(ju::Rectangle<int>(offset + left, bounds.getY(),
                                                   right - left, bounds.getHeight()).reduced(2));
        }
    }

    int maxInspector() const { return ju::jlimit(200, 600, getWidth() - 740); }
    int maxWorkspace() const { return ju::jlimit(100, 600, getHeight() - 475); }

    void saveLayout()
    {
        panels.save();
        resized();
        updateControls();
    }

    ju::StringArray getMenuBarNames() override { return { "FILE", "EDIT", "INSERT", "OPTIONS", "HELP" }; }

    ju::PopupMenu getMenuForIndex(int index, const ju::String&) override
    {
        ju::PopupMenu menu;
        const auto add = [&](std::initializer_list<Command> list) {
            for (auto id : list) menu.addCommandItem(&commandManager, id);
        };
        switch (index)
        {
            case 0: add({ newProject, openProject, saveProject, saveProjectAs, importAudio, exportAudio, exitApp }); break;
            case 1: add({ undo, redo, cut, copy, paste, duplicate, deleteItem, locateFile }); break;
            case 2: add({ audioTrack, midiTrack, midiItem, audioFile, synth, reverb, vst3 }); break;
            case 3: add({ devices, armAudio, armMidi, monitoringReport }); break;
            case 4: add({ about }); break;
            default: break;
        }
        return menu;
    }

    void menuItemSelected(int, int) override {}
    ju::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }

    void getAllCommands(ju::Array<ju::CommandID>& list) override
    {
        for (int id = newProject; id <= startRecording; ++id)
            list.add(id);
    }

    void getCommandInfo(ju::CommandID id, ju::ApplicationCommandInfo& info) override
    {
        static const char* names[] = {
            "New", "Open", "Save", "Save As", "Import Audio", "Export Audio", "Exit",
            "Undo", "Redo", "Cut", "Copy", "Paste", "Duplicate", "Delete",
            "Audio Track", "MIDI Track", "MIDI Item", "Audio and MIDI Devices", "About SimpleDAW",
            "Arm Audio Input", "Arm MIDI Input", "Add MIDI Synth", "Add Reverb", "Load VST3",
            "Audio File", "Locate File", "Audio Monitoring Report", "Play / Stop", "Record"
        };
        if (id < newProject || id > startRecording)
        {
            info.setActive(false);
            return;
        }
        const auto name = names[id - newProject];
        info.setInfo(name, name, "SimpleDAW", 0);
        const auto stopped = edit && !edit->getTransport().isPlaying();
        const auto hasTrack = trackAt(selection) != nullptr;
        switch (id)
        {
            case saveProject: case saveProjectAs: case exportAudio: info.setActive(stopped); break;
            case undo: info.setActive(canUseHistory(false)); break;
            case redo: info.setActive(canUseHistory(true)); break;
            case cut: info.setActive(canDispatch(UniversalAction::cut)); break;
            case duplicate: info.setActive(false); break;
            case copy: info.setActive(canDispatch(UniversalAction::copy)); break;
            case paste: info.setActive(canDispatch(UniversalAction::paste)); break;
            case deleteItem: info.setActive(canDispatch(UniversalAction::remove) || canClearTimeSelection()); break;
            case locateFile:
                info.setActive(stopped && activeEditingArea() == EditingArea::timeline && missingWave(selectedClip()));
                break;
            case audioTrack: case midiTrack: info.setActive(stopped && trackCount() < maxTracks); break;
            case armAudio: info.setActive(edit && !edit->getTransport().isRecording() && hasTrack); break;
            case midiItem: case audioFile: case importAudio: case armMidi: case synth: case reverb: case vst3:
                info.setActive(stopped && hasTrack); break;
            case playStop: case startRecording: info.setActive(edit != nullptr); break;
            default: break;
        }
        if (id == saveProject) info.addDefaultKeypress('s', ju::ModifierKeys::ctrlModifier);
        if (id == playStop || id == deleteItem)
        {
            info.flags |= ju::ApplicationCommandInfo::wantsKeyUpDownCallbacks;
            info.addDefaultKeypress(id == playStop ? ju::KeyPress::spaceKey : ju::KeyPress::deleteKey, {});
        }
        if (id == startRecording) info.addDefaultKeypress('r', {});
    }

    bool perform(const InvocationInfo& invocation) override
    {
        if ((invocation.commandID == playStop || invocation.commandID == deleteItem)
            && invocation.invocationMethod == InvocationInfo::fromKeyPress)
        {
            auto* focused = ju::Component::getCurrentlyFocusedComponent();
            if (!invocation.isKeyDown
                || dynamic_cast<ju::TextInputTarget*>(focused ? focused : invocation.originatingComponent))
                return true;
        }

        switch (invocation.commandID)
        {
            case newProject: chooseProject(true); break;
            case openProject: chooseProject(false); break;
            case saveProject: save(); break;
            case saveProjectAs: saveAs(); break;
            case audioFile: case importAudio: importWave(); break;
            case exportAudio: exportProject(); break;
            case exitApp: ju::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case undo: useHistory(false); break;
            case redo: useHistory(true); break;
            case cut: dispatchAction(UniversalAction::cut); break;
            case copy: dispatchAction(UniversalAction::copy); break;
            case paste: dispatchAction(UniversalAction::paste); break;
            case deleteItem:
                if (canClearTimeSelection())
                    clearTimeSelection();
                else
                    dispatchAction(UniversalAction::remove);
                break;
            case locateFile: locateSelectedFile(); break;
            case audioTrack: addTrack(false); break;
            case midiTrack: addTrack(true); break;
            case midiItem: insertMidi(); break;
            case devices: configureDevices(); break;
            case monitoringReport: showMonitoringReport(); break;
            case about: ju::AlertWindow::showMessageBoxAsync(ju::MessageBoxIconType::InfoIcon,
                "About SimpleDAW", ju::String(applicationTitle) + "\nJUCE and Tracktion Engine"); break;
            case armAudio: armInput(false); break;
            case armMidi: armInput(true); break;
            case synth:
                if (auto* track = selectedTrack())
                    report(insertBuiltIn<te::FourOscPlugin>(*track, true) ? "Synth inserted" : "Could not insert synth");
                break;
            case reverb:
                if (auto* track = selectedTrack())
                    report(insertBuiltIn<te::ReverbPlugin>(*track) ? "Reverb inserted" : "Could not insert reverb");
                break;
            case vst3: loadVst3(); break;
            case playStop: if (edit && edit->getTransport().isPlaying()) stop(); else play(); break;
            case startRecording: record(); break;
            default: return false;
        }
        updateControls();
        return true;
    }

    int trackCount() const { return edit ? te::getAudioTracks(*edit).size() : 0; }
    te::AudioTrack* trackAt(int index) const { return edit ? te::getAudioTracks(*edit)[index] : nullptr; }
    te::Clip* selectedClip() const
    {
        if (edit && actionSelection.clips.size() == 1)
            return te::findClipForID(*edit, *actionSelection.clips.begin());
        return nullptr;
    }

    static te::WaveAudioClip* missingWave(te::Clip* clip)
    {
        auto* wave = dynamic_cast<te::WaveAudioClip*>(clip);
        return wave && !wave->getOriginalFile().existsAsFile() ? wave : nullptr;
    }

    static void replaceWaveSource(te::WaveAudioClip& clip, const ju::File& file)
    {
        auto& undoManager = clip.edit.getUndoManager();
        undoManager.beginNewTransaction("Locate missing audio");
        auto& reference = clip.getSourceFileReference();
        const auto previous = reference.source.get();
        const auto path = te::SourceFileReference::findPathFromFile(clip.edit, file, true);
        for (auto take : clip.state.getChildWithName(te::IDs::TAKES))
            if (take.getProperty(te::IDs::source).toString() == previous)
                take.setProperty(te::IDs::source, path, &undoManager);
        reference.setToDirectFileReference(file, true);
        clip.edit.restartPlayback();
    }

    void locateSelectedFile()
    {
        if (!ready())
            return;
        auto* clip = missingWave(selectedClip());
        if (!clip)
            return;
        const auto id = clip->itemID;
        chooseFile("Locate " + clip->getOriginalFile().getFileName(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3",
            ju::FileBrowserComponent::openMode | ju::FileBrowserComponent::canSelectFiles,
            [this, id](const ju::File& file) {
                if (!ready())
                    return;
                auto* wave = missingWave(te::findClipForID(*edit, id));
                if (!wave)
                    return;
                const te::AudioFile replacement(engine, file);
                if (!replacement.isValid() || replacement.getLength() <= 0.0)
                    return report("Choose a readable audio file");
                replaceWaveSource(*wave, file);
                arrangement.waves.erase(id);
                report("Audio relinked");
                updateControls();
            });
    }

    EditingArea activeEditingArea() const
    {
        // Effects exposes the existing chain read-only. The other workspace tabs are placeholders.
        return actionSelection.activeArea(panels.workspaceOpen && activeTab == 1, panels.inspectorOpen);
    }

    bool editingStopped() const
    {
        return edit && !edit->getTransport().isPlaying() && !edit->getTransport().isRecording();
    }

    bool canEditTimeline() const
    {
        return editingStopped() && activeEditingArea() == EditingArea::timeline;
    }

    bool canUseHistory(bool redoAction) const
    {
        return editingStopped() && (redoAction ? edit->getUndoManager().canRedo()
                                               : edit->getUndoManager().canUndo());
    }

    void useHistory(bool redoAction)
    {
        if (!canUseHistory(redoAction))
            return;
        const auto* primary = trackAt(selection);
        const auto primaryID = primary ? primary->itemID : te::EditItemID{};
        auto& history = edit->getUndoManager();
        if (redoAction ? history.redo() : history.undo())
        {
            selection = te::getAudioTracks(*edit).indexOf(trackForID(primaryID));
            edit->restartPlayback();
        }
        updateControls();
    }

    te::AudioTrack* trackForID(te::EditItemID id) const
    {
        return id.isValid() ? dynamic_cast<te::AudioTrack*>(te::findTrackForID(*edit, id)) : nullptr;
    }

    te::TimePosition pasteStart() const
    {
        return te::TimePosition::fromSeconds(snapSeconds(editCursor.inSeconds()));
    }

    static bool ordinaryClip(te::Clip& clip)
    {
        return !clip.isGrouped() && !clip.isLinked() && !clip.getPosition().time.isEmpty()
            && (dynamic_cast<te::WaveAudioClip*>(&clip) || dynamic_cast<te::MidiClip*>(&clip));
    }

    bool destinationAvailable(te::AudioTrack* track, int added) const
    {
        return track && !track->isFrozen(te::Track::anyFreeze)
            && track->getClips().size() + added <= engine.getEngineBehaviour().getEditLimits().maxClipsInTrack;
    }

    struct Placement
    {
        te::AudioTrack* track;
        te::ClipPosition position;
    };

    std::vector<Placement> pastePlan() const
    {
        if (!clipClipboard || !trackAt(selection))
            return {};
        const auto tracks = te::getAudioTracks(*edit);
        const auto anchor = pasteStart();
        std::map<te::AudioTrack*, int> added;
        std::vector<Placement> plan;
        for (const auto& item : clipClipboard->clips)
        {
            auto* track = tracks[selection + item.trackOffset];
            const auto start = anchor + te::TimeDuration::fromSeconds(double(item.state[te::IDs::start]));
            const auto& tempoSequence = edit->tempoSequence;
            const auto end = item.hasBeatTimes ? tempoSequence.toTime(tempoSequence.toBeats(start) + item.lengthBeats)
                : start + te::TimeDuration::fromSeconds(double(item.state[te::IDs::length]));
            const auto offset = te::TimeDuration::fromSeconds(item.hasBeatTimes
                ? item.offsetBeats.inBeats() / tempoSequence.getBeatsPerSecondAt(start)
                : double(item.state[te::IDs::offset]));
            if (!destinationAvailable(track, ++added[track]) || start < te::TimePosition{}
                || end <= start || end > te::Edit::getMaximumEditEnd())
                return {};
            plan.push_back({ track, { { start, end }, offset } });
        }
        return plan;
    }

    bool snapshotClips(const std::vector<te::Clip*>& clips)
    {
        const auto tracks = te::getAudioTracks(*edit);
        auto first = tracks.size();
        te::SelectableList items;
        for (auto* clip : clips)
        {
            first = std::min(first, tracks.indexOf(dynamic_cast<te::AudioTrack*>(clip->getTrack())));
            items.add(clip);
        }
        auto snapshot = std::make_unique<te::Clipboard::Clips>();
        snapshot->addSelectedClips(items, {}, te::Clipboard::Clips::AutomationLocked::no);
        if (first < 0 || snapshot->clips.size() != clips.size())
            return false;

        // Tracktion records offsets in the full edit tree. The arrangement uses visible audio rows.
        const auto allTracks = te::getAllTracks(*edit);
        auto firstFull = allTracks.size();
        for (auto* clip : clips)
            firstFull = std::min(firstFull, allTracks.indexOf(clip->getTrack()));
        for (auto& item : snapshot->clips)
            item.trackOffset = tracks.indexOf(dynamic_cast<te::AudioTrack*>(allTracks[firstFull + item.trackOffset])) - first;
        clipClipboard = std::move(snapshot);
        return true;
    }

    void pasteClips(const std::vector<Placement>& plan)
    {
        auto& history = edit->getUndoManager();
        history.beginNewTransaction("Paste clips");
        ItemSelection pasted;
        std::map<te::EditItemID, te::EditItemID> remappedIDs;
        for (size_t i = 0; i < plan.size(); ++i)
        {
            auto state = clipClipboard->clips[i].state.createCopy();
            te::EditItemID::remapIDs(state, nullptr, *edit, &remappedIDs);
            const auto& position = plan[i].position;
            state.setProperty(te::IDs::start, position.getStart().inSeconds(), nullptr);
            state.setProperty(te::IDs::length, position.getLength().inSeconds(), nullptr);
            state.setProperty(te::IDs::offset, position.getOffset().inSeconds(), nullptr);
            if (auto* clip = plan[i].track->insertClipWithState(state))
                pasted.insert(clip->itemID);
            else
            {
                history.undoCurrentTransactionOnly();
                report("Could not paste clips");
                return;
            }
        }
        actionSelection.clips = std::move(pasted);
    }

    std::vector<te::Clip*> selectedClips() const
    {
        std::vector<te::Clip*> clips;
        for (auto id : actionSelection.clips)
        {
            auto* clip = te::findClipForID(*edit, id);
            if (!clip)
                return {};
            clips.push_back(clip);
        }
        return clips;
    }

    struct ClipChange
    {
        te::Clip* clip;
        te::AudioTrack* destination;
        te::ClipPosition position;
    };

    std::vector<ClipChange> clipChanges(UniversalAction action, const std::vector<te::Clip*>& clips) const
    {
        using Action = UniversalAction;
        const auto tracks = te::getAudioTracks(*edit);
        std::vector<ClipChange> changes;
        if (clips.empty())
            return changes;
        const bool vertical = action == Action::up || action == Action::down;
        const bool move = action == Action::left || action == Action::right;
        const bool start = action == Action::startMinus || action == Action::startPlus;
        const bool earlier = action == Action::left || action == Action::startMinus || action == Action::endMinus;
        te::TimeDuration delta;
        if (!vertical)
        {
            const auto grid = snap.getSelectedId();
            if (grid <= 1)
                return {};
            auto anchor = te::Edit::getMaximumEditEnd();
            for (auto* clip : clips)
                anchor = std::min(anchor, (move || start) ? clip->getPosition().getStart() : clip->getPosition().getEnd());
            const auto& sequence = edit->tempoSequence;
            const auto step = sequence.getTimeSigAt(anchor).denominator.get() / (4.0 * (1 << (grid - 2)));
            const auto targetBeat = sequence.toBeats(anchor) + te::BeatDuration::fromBeats(earlier ? -step : step);
            if (targetBeat < te::BeatPosition{})
                return {};
            delta = sequence.toTime(targetBeat) - anchor;
        }

        std::map<te::AudioTrack*, int> outgoing, incoming;
        if (vertical)
            for (auto* clip : clips)
                ++outgoing[dynamic_cast<te::AudioTrack*>(clip->getTrack())];
        for (auto* clip : clips)
        {
            if (!ordinaryClip(*clip))
                return {};
            auto position = clip->getPosition();
            auto* source = dynamic_cast<te::AudioTrack*>(clip->getTrack());
            te::AudioTrack* destination = nullptr;
            if (!source || source->isFrozen(te::Track::anyFreeze))
                return {};
            if (vertical)
            {
                destination = tracks[tracks.indexOf(source) + (action == Action::up ? -1 : 1)];
                if (!destinationAvailable(destination, ++incoming[destination] - outgoing[destination]) || !clip->canBeAddedTo(*destination))
                    return {};
            }
            else if (move)
            {
                position.time = position.time + delta;
            }
            else
            {
                if (clip->isLooping() || clip->getNumTakes(true) > 1)
                    return {};
                if ((start && position.getStart() + delta >= position.getEnd())
                    || (!start && position.getEnd() + delta <= position.getStart()))
                    return {};
                if (start)
                {
                    position.time = position.time.withStart(position.getStart() + delta);
                    position.offset = position.offset + delta;
                }
                else
                    position.time = position.time.withEnd(position.getEnd() + delta);

                if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
                {
                    const te::AudioFile sourceFile(engine, wave->getOriginalFile());
                    if (!sourceFile.isValid() || wave->getWarpTime() || wave->getAutoTempo()
                        || (position.offset + position.time.getLength()).inSeconds() * wave->getSpeedRatio()
                            > sourceFile.getLength() + 1.0e-9)
                        return {};
                }
            }
            if (position.getStart() < te::TimePosition{} || position.time.isEmpty()
                || position.getEnd() > te::Edit::getMaximumEditEnd() || position.offset < te::TimeDuration{})
                return {};
            changes.push_back({ clip, destination, position });
        }
        // Move outer rows first, so outgoing clips vacate a lane before incoming clips arrive.
        if (vertical)
            std::stable_sort(changes.begin(), changes.end(), [&](const auto& a, const auto& b) {
                return action == Action::up ? tracks.indexOf(a.destination) < tracks.indexOf(b.destination)
                                            : tracks.indexOf(a.destination) > tracks.indexOf(b.destination);
            });
        return changes;
    }

    std::vector<te::AudioTrack*> trackOrder(UniversalAction action) const
    {
        const auto tracks = te::getAudioTracks(*edit);
        std::vector<te::AudioTrack*> chosen, remaining;
        if (actionSelection.tracks.empty())
            return {};
        for (auto* track : tracks)
        {
            if (track->state.getParent() != edit->state)
                return {};
            (actionSelection.tracks.contains(track->itemID) ? chosen : remaining).push_back(track);
        }
        if (chosen.size() != actionSelection.tracks.size())
            return {};
        const bool up = action == UniversalAction::up;
        const int anchor = up ? tracks.indexOf(chosen.front()) : tracks.indexOf(chosen.back());
        if ((up && anchor == 0) || (!up && anchor == tracks.size() - 1))
            return {};
        auto neighbor = std::find(remaining.begin(), remaining.end(), tracks[anchor + (up ? -1 : 1)]);
        remaining.insert(up ? neighbor : std::next(neighbor), chosen.begin(), chosen.end());
        return remaining;
    }

    bool canDispatch(UniversalAction action) const
    {
        using Action = UniversalAction;
        if (!editingStopped())
            return false;
        if (activeEditingArea() == EditingArea::tracks)
        {
            if (action == Action::remove)
                return !actionSelection.tracks.empty()
                    && std::all_of(actionSelection.tracks.begin(), actionSelection.tracks.end(),
                                   [this](auto id) { return trackForID(id) != nullptr; });
            return (action == Action::up || action == Action::down) && !trackOrder(action).empty();
        }
        if (!canEditTimeline())
            return false;
        if (action == Action::paste)
            return !pastePlan().empty();
        if (action == Action::split || action == Action::trim || action == Action::reserved)
            return false;
        const auto clips = selectedClips();
        if (action == Action::remove)
            return !clips.empty();
        if (action == Action::copy || action == Action::cut)
            return !clips.empty() && std::all_of(clips.begin(), clips.end(), [](auto* clip) { return ordinaryClip(*clip); });
        return !clipChanges(action, clips).empty();
    }

    void deleteSelection(bool tracks, bool cutting)
    {
        auto* project = edit.get();
        const auto area = tracks ? EditingArea::tracks : EditingArea::timeline;
        auto& selectionIDs = tracks ? actionSelection.tracks : actionSelection.clips;
        const auto ids = selectionIDs;
        const auto clips = tracks ? std::vector<te::Clip*>{} : selectedClips();
        const bool missing = std::any_of(clips.begin(), clips.end(), [](auto* clip) { return missingWave(clip); });
        if ((tracks || missing) && !ju::AlertWindow::showOkCancelBox(ju::MessageBoxIconType::WarningIcon,
                tracks ? "Delete tracks?" : "Remove missing audio?",
                tracks ? "Remove the selected tracks and their contents from the project?"
                       : "The selection includes missing audio. Remove the selected clips from the project?",
                cutting ? "Cut Clips" : "Delete", "Cancel", this))
            return;
        const auto action = cutting ? UniversalAction::cut : UniversalAction::remove;
        if (edit.get() != project || activeEditingArea() != area || selectionIDs != ids || !canDispatch(action))
            return;
        if (cutting && !snapshotClips(selectedClips()))
            return;

        edit->getUndoManager().beginNewTransaction(tracks ? "Delete tracks" : cutting ? "Cut clips" : "Delete clips");
        if (tracks)
        {
            const auto* primary = trackAt(selection);
            const auto primaryID = primary ? primary->itemID : te::EditItemID{};
            for (auto id : ids)
                edit->deleteTrack(trackForID(id));
            selection = te::getAudioTracks(*edit).indexOf(trackForID(primaryID));
        }
        else
        {
            for (auto id : ids)
                te::findClipForID(*edit, id)->removeFromParent();
        }
        selectionIDs.clear();
        edit->restartPlayback();
    }

    void dispatchAction(UniversalAction action)
    {
        using Action = UniversalAction;
        if (!canDispatch(action))
            return;
        const bool tracks = activeEditingArea() == EditingArea::tracks;
        auto& history = edit->getUndoManager();
        if (action == Action::remove || action == Action::cut)
            deleteSelection(tracks, action == Action::cut);
        else if (action == Action::copy)
            snapshotClips(selectedClips());
        else if (action == Action::paste)
            pasteClips(pastePlan());
        else if (tracks)
        {
            const auto order = trackOrder(action);
            auto* primary = trackAt(selection);
            history.beginNewTransaction("Reorder tracks");
            for (size_t i = 0; i < order.size(); ++i)
            {
                auto parent = edit->state;
                const int oldIndex = parent.indexOf(order[i]->state);
                const int newIndex = parent.indexOf(trackAt(static_cast<int>(i))->state);
                if (oldIndex != newIndex)
                    parent.moveChild(oldIndex, newIndex, &history);
            }
            selection = te::getAudioTracks(*edit).indexOf(primary);
        }
        else
        {
            const auto changes = clipChanges(action, selectedClips());
            const bool moving = action == Action::up || action == Action::down || action == Action::left || action == Action::right;
            history.beginNewTransaction(moving ? "Move clips" : "Resize clips");
            for (const auto& change : changes)
            {
                if (!change.destination)
                    change.clip->setPosition(change.position);
                else if (!change.clip->moveTo(*change.destination) || change.clip->getTrack() != change.destination)
                {
                    history.undoCurrentTransactionOnly();
                    report("Could not move clips");
                    break;
                }
            }
        }
        updateControls();
    }

    bool canClearTimeSelection() const
    {
        return canEditTimeline() && actionSelection.clips.empty() && timeSelection.has_value();
    }

    void clearTimeSelection()
    {
        timeSelection.reset();
        auto& transport = edit->getTransport();
        transport.looping = false;
        transport.setLoopRange({});
        edit->markAsChanged();
        report("Time selection cleared");
    }

    static ju::StringArray unavailablePlugins(te::Edit& target)
    {
        ju::StringArray result;
        for (auto* plugin : te::getAllPlugins(target, true))
            if (auto* external = dynamic_cast<te::ExternalPlugin*>(plugin); external && external->isMissing())
            {
                const auto* track = external->getOwnerTrack();
                result.add((track ? track->getName() : ju::String("Master")) + " / " + external->getName());
            }
        return result;
    }

    void warnUnavailablePlugins()
    {
        const auto missing = unavailablePlugins(*edit);
        if (missing.isEmpty())
            return;
        report(ju::String(missing.size()) + " unavailable plugin(s). Project references retained.");
        ju::AlertWindow::showMessageBoxAsync(ju::MessageBoxIconType::WarningIcon, "Unavailable plugins",
            "These plugins could not be loaded. Their saved references and settings remain in the project.\n\n"
                + missing.joinIntoString("\n"));
    }

    ju::String selectedEffects() const
    {
        const auto* track = trackAt(selection);
        if (!track)
            return "EFFECTS - Select a track";
        ju::StringArray names;
        for (auto* plugin : track->pluginList.getPlugins())
        {
            if (dynamic_cast<te::VolumeAndPanPlugin*>(plugin) || dynamic_cast<te::LevelMeterPlugin*>(plugin))
                continue;
            names.add(plugin->getName() + (plugin->isMissing() ? " (unavailable)" : ""));
        }
        return track->getName() + "\n" + (names.isEmpty() ? "No effects or instruments" : names.joinIntoString("\n"));
    }

    void setProject(std::unique_ptr<te::Edit> next, const ju::File& file)
    {
        meters.clear();
        arrangement.waves.clear();
        edit = std::move(next);
        edit->playInStopEnabled = true;
        projectFile = file;
        selection = trackCount() == 0 ? -1 : 0;
        actionSelection = {};
        if (auto* track = trackAt(selection))
            actionSelection.tracks.insert(track->itemID);
        clipClipboard.reset();
        boxAnchor.reset();
        selectionBox = {};
        dragItem = {};
        dragPreview.reset();
        rangePreview.reset();
        draggingRange = false;
        editCursor = edit->getTransport().getPosition();
        const auto savedRange = edit->getTransport().getLoopRange();
        timeSelection = savedRange.getLength().inSeconds() > 0.001 ? std::optional(savedRange) : std::nullopt;
        paused = false;
        firstRow = 0;
        viewStart = 0.0;
        edit->getUndoManager().clearUndoHistory();
        edit->resetChangedStatus();
        if (auto* window = findParentComponentOfClass<ju::DocumentWindow>())
            window->setName(ju::String(applicationTitle) + " - " + file.getFileName());
        if (arrangement.isShowing())
            arrangement.grabKeyboardFocus();
        updateControls();
    }

    void saveAs()
    {
        if (!ready())
            return;
        chooseFile("Save project as", "*.tracktionedit",
            ju::FileBrowserComponent::saveMode | ju::FileBrowserComponent::canSelectFiles,
            [this](const ju::File& choice) {
                if (!ready())
                    return;
                const auto file = choice.withFileExtension("tracktionedit");
                if (file.exists())
                    return report("Choose a new project filename");
                if (file.getParentDirectory() != projectFile.getParentDirectory())
                    return report("Save As must use the current project folder so relative media paths stay valid");
                if (!te::EditFileOperations(*edit).saveAs(file))
                    return report("Could not save project copy");
                edit->editFileRetriever = [file] { return file; };
                projectFile = file;
                if (auto* window = findParentComponentOfClass<ju::DocumentWindow>())
                    window->setName(ju::String(applicationTitle) + " - " + file.getFileName());
                report("Project saved");
            });
    }

    static ju::Colour palette(int index)
    {
        constexpr uint32_t colours[] = { 0xff168cff, 0xff25bc48, 0xff14c6cd, 0xfff0d20c,
                                         0xffec872b, 0xffe44ab7, 0xffa95cfc, 0xfffc5151 };
        return ju::Colour(colours[index % static_cast<int>(std::size(colours))]);
    }

    void addTrack(bool midi)
    {
        if (!ready())
            return;
        const auto count = trackCount();
        if (count >= maxTracks)
            return report("Track limit reached");
        edit->getUndoManager().beginNewTransaction(midi ? "Add MIDI track" : "Add audio track");
        edit->ensureNumberOfAudioTracks(count + 1);
        if (auto* track = trackAt(count))
        {
            track->setName(ju::String(midi ? "MIDI " : "Audio ") + ju::String(count + 1));
            track->setColour(palette(count));
            edit->markAsChanged();
            selectTrack(count);
            if (count >= firstRow + rowsOnScreen())
                scrollRows(count - firstRow - rowsOnScreen() + 1);
        }
    }

    void selectTrack(int index)
    {
        selection = index;
        auto* track = trackAt(index);
        ActionSelection::click(actionSelection.tracks, track ? track->itemID : te::EditItemID{}, false);
        actionSelection.requestedArea = EditingArea::tracks;
        updateControls();
    }

    te::TimePosition barTime(int bar) const
    {
        return edit->tempoSequence.toTime(te::tempo::BarsAndBeats { bar, {}, 0 });
    }

    void insertMidi()
    {
        if (auto* track = selectedTrack())
        {
            const auto nextBar = barTime(edit->tempoSequence.toBarsAndBeats(editCursor).bars + 1);
            edit->getUndoManager().beginNewTransaction("Insert MIDI item");
            if (auto clip = te::insertMIDIClip(*track, "MIDI item", { editCursor, nextBar }))
            {
                actionSelection.clips = { clip->itemID };
                actionSelection.requestedArea = EditingArea::timeline;
            }
        }
    }

    void applyTempo()
    {
        auto value = tempo.getText().trim();
        if (value.startsWithIgnoreCase("BPM "))
            value = value.substring(4).trim();
        const auto bpm = value.getDoubleValue();
        if (ready() && value.containsOnly("0123456789.") && value.indexOfChar('.') == value.lastIndexOfChar('.')
            && bpm >= 20.0 && bpm <= 300.0)
        {
            edit->getUndoManager().beginNewTransaction("Tempo");
            edit->tempoSequence.getTempoAt(editCursor).setBpm(bpm);
        }
        else if (edit)
            report("Enter a tempo from 20 to 300 BPM while stopped");
        arrangement.grabKeyboardFocus();
        updateControls();
    }

    double snapSeconds(double seconds) const
    {
        seconds = ju::jlimit(0.0, te::Edit::getMaximumEditEnd().inSeconds(), seconds);
        if (!edit || snap.getSelectedId() <= 1)
            return seconds;
        const double subdivisions = 1 << (snap.getSelectedId() - 2);
        auto& sequence = edit->tempoSequence;
        const auto time = te::TimePosition::fromSeconds(seconds);
        const auto barStart = barTime(sequence.toBarsAndBeats(time).bars);
        const auto beat = sequence.toBeats(time);
        const auto origin = sequence.toBeats(barStart);
        const auto increment = sequence.getTimeSigAt(time).denominator.get() / (4.0 * subdivisions);
        const auto steps = std::round((beat - origin).inBeats() / increment);
        return sequence.toTime(origin + te::BeatDuration::fromBeats(steps * increment)).inSeconds();
    }

    void seek(double seconds, bool quantise = true)
    {
        if (!edit || edit->getTransport().isRecording())
            return;
        const auto target = quantise ? snapSeconds(seconds) : seconds;
        editCursor = te::TimePosition::fromSeconds(ju::jlimit(0.0, te::Edit::getMaximumEditEnd().inSeconds(), target));
        edit->getTransport().setPosition(editCursor);
        reveal(editCursor);
        updateControls();
    }

    void seekBar(int delta)
    {
        if (!edit)
            return;
        auto bar = edit->tempoSequence.toBarsAndBeats(editCursor).bars;
        if (delta < 0 && editCursor > barTime(bar) + te::TimeDuration::fromSeconds(0.001))
            ++bar;
        seek(barTime(ju::jmax(0, bar + delta)).inSeconds(), false);
    }

    void play()
    {
        if (!edit)
            return report("Create or open a project first");
        auto& transport = edit->getTransport();
        if (!transport.isPlaying())
        {
            if (!paused)
                transport.setPosition(editCursor);
            transport.play(false);
            paused = false;
        }
    }

    void pause()
    {
        if (!edit || edit->getTransport().isRecording())
            return;
        auto& transport = edit->getTransport();
        if (transport.isPlaying())
        {
            const auto head = transport.getPosition();
            transport.stop(false, false);
            transport.setPosition(head);
            paused = true;
        }
    }

    te::TimeRange loopRange() const
    {
        if (timeSelection)
            return *timeSelection;
        const auto current = edit->getTransport().getLoopRange();
        if (current.getLength().inSeconds() > 0.001)
            return current;
        return { {}, ju::jmax(te::TimePosition{} + edit->getLength(), barTime(1)) };
    }

    void toggleLoop()
    {
        if (!edit || edit->getTransport().isRecording())
            return;
        if (edit->recordingPunchInOut)
            return report("Select Normal recording mode to enable Loop");
        auto& transport = edit->getTransport();
        transport.setLoopRange(loopRange());
        transport.looping = !transport.looping.get();
        edit->markAsChanged();
    }

    int rowHeight() const { return panels.tallTracks ? 76 : 44; }
    static constexpr int headerWidth = 244, rulerHeight = 42;
    int rowsOnScreen() const { return ju::jmax(1, (arrangement.getHeight() - rulerHeight) / rowHeight()); }
    double visibleSeconds() const { return ju::jmax(1, arrangement.getWidth() - headerWidth) / pixelsPerSecond; }
    double totalSeconds() const
    {
        const auto contentEnd = edit ? edit->getLength().inSeconds() : 0.0;
        const auto head = edit ? edit->getTransport().getPosition().inSeconds() : 0.0;
        return ju::jmax(120.0, ju::jmax(contentEnd, editCursor.inSeconds(), head) + visibleSeconds());
    }

    void scrollRows(int rows)
    {
        firstRow = ju::jlimit(0, ju::jmax(0, trackCount() - rowsOnScreen()), firstRow + rows);
        arrangement.repaint();
    }

    void setViewStart(double seconds)
    {
        viewStart = ju::jlimit(0.0, ju::jmax(0.0, totalSeconds() - visibleSeconds()), seconds);
        arrangement.repaint();
    }

    void scrollTime(double seconds) { setViewStart(viewStart + seconds); }

    void scrollBar(int delta)
    {
        if (!edit)
            return scrollTime(delta * 2.0);
        const auto start = te::TimePosition::fromSeconds(viewStart);
        const auto bar = edit->tempoSequence.toBarsAndBeats(start).bars;
        if (bar == 0 && delta < 0)
            return setViewStart(0.0);
        const auto end = barTime(bar + delta);
        scrollTime((end - barTime(bar)).inSeconds());
    }

    void reveal(te::TimePosition time)
    {
        const auto seconds = time.inSeconds();
        if (seconds < viewStart || seconds >= viewStart + visibleSeconds())
            setViewStart(seconds - visibleSeconds() * 0.1);
    }

    void zoom(double factor)
    {
        const auto offset = (editCursor.inSeconds() - viewStart) * pixelsPerSecond;
        const auto anchor = ju::jlimit(0.0, static_cast<double>(ju::jmax(0, arrangement.getWidth() - headerWidth)), offset);
        pixelsPerSecond = ju::jlimit(8.0, 1200.0, pixelsPerSecond * factor);
        setViewStart(editCursor.inSeconds() - anchor / pixelsPerSecond);
    }

    int timeX(te::TimePosition time) const
    {
        return headerWidth + ju::roundToInt((time.inSeconds() - viewStart) * pixelsPerSecond);
    }

    double timeAt(int x) const { return viewStart + (x - headerWidth) / pixelsPerSecond; }

    static ju::String elapsed(double seconds, bool milliseconds)
    {
        const auto ms = static_cast<int64_t>(std::abs(seconds) * 1000.0);
        auto text = ju::String(ms / 60000) + ":" + ju::String(ms / 1000 % 60).paddedLeft('0', 2);
        if (milliseconds)
            text += "." + ju::String(ms % 1000).paddedLeft('0', 3);
        return seconds < 0 ? "-" + text : text;
    }

    static bool isArmed(te::InputDeviceInstance& input, te::EditItemID track)
    {
        return input.getInputDevice().isEnabled() && input.isRecordingEnabled(track);
    }

    static ju::Result toggleInput(te::InputDeviceInstance& input, te::AudioTrack& track)
    {
        const auto id = track.itemID;
        const bool remove = isArmed(input, id);
        auto& undoManager = track.edit.getUndoManager();
        undoManager.beginNewTransaction(remove ? "Remove input assignment" : "Assign input");
        if (remove)
        {
            const auto result = input.removeTarget(id, &undoManager);
            if (result.failed())
                return result;

            // Tracktion can skip rebuilding on removal when other destinations remain.
            // Request the swap of the audio graph without stopping transport or changing other routes.
            track.edit.restartPlayback();
        }
        else
        {
            // A physical input can have several independent destinations.
            const auto target = input.setTarget(id, false, &undoManager, 0);
            if (!target)
                return ju::Result::fail(target.error());
            input.setRecordingEnabled(id, true);
            track.edit.restartPlayback();
        }
        track.edit.markAsChanged();
        return ju::Result::ok();
    }

    struct InputBadges
    {
        bool armed = false;
        std::array<bool, 2> channels {};
    };

    InputBadges inputBadges(const te::AudioTrack& track) const
    {
        InputBadges badges;
        for (auto* input : edit->getAllInputDevices())
        {
            if (!isArmed(*input, track.itemID))
                continue;
            badges.armed = true;
            if (const auto* wave = dynamic_cast<te::WaveInputDevice*>(&input->getInputDevice()))
                for (const auto& channel : wave->getChannels())
                    if (channel.indexInDevice >= 0 && channel.indexInDevice < 2)
                        badges.channels[static_cast<size_t>(channel.indexInDevice)] = true;
        }
        return badges;
    }

    void pollMeters()
    {
        std::erase_if(meters, [](const auto& entry) { return entry.second->source == nullptr; });
        for (auto* track : te::getAudioTracks(*edit))
            if (auto* plugin = track->getLevelMeterPlugin())
            {
                auto& meter = meters[track->itemID];
                if (!meter || meter->source.get() != &plugin->measurer)
                    meter = std::make_unique<Meter>(*plugin);
                auto db = -100.0f;
                for (int ch = 0; ch < meter->client.getNumChannelsUsed(); ++ch)
                    db = ju::jmax(db, meter->client.getAndClearAudioLevel(ch).dB);
                meter->level = ju::jmax(meter->level - 0.035f, ju::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f));
            }
    }

    void paintTrack(ju::Graphics& g, te::AudioTrack& track, int row, int y, ju::Colour colour)
    {
        const auto selected = actionSelection.tracks.contains(track.itemID);
        const auto primaryTrack = selection == row;
        auto bounds = trackBounds(y);
        g.setColour(ju::Colour(0xff242424).interpolatedWith(colour, (selected || primaryTrack) ? 0.25f : 0.12f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        if (selected || primaryTrack)
        {
            g.setColour(selected && activeEditingArea() == EditingArea::tracks ? ju::Colours::white : colour);
            g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 2.0f);
        }
        auto indicators = bounds.removeFromRight(24).reduced(3, 2);
        const auto indicatorHeight = indicators.getHeight() / 3;
        const auto inputs = inputBadges(track);
        const bool states[] = { track.isMuted(false), track.isSolo(false), inputs.armed };
        const ju::Colour light[] = { ju::Colours::yellow, ju::Colours::limegreen, ju::Colours::red };
        const ju::String letters[] = { "M", "S", "R" };
        g.setFont(ju::FontOptions(10.0f, ju::Font::bold));
        for (int i = 0; i < 3; ++i)
        {
            auto led = indicators.removeFromTop(indicatorHeight).reduced(0, 1);
            g.setColour(states[i] ? light[i] : ju::Colour(0xff484848));
            g.fillRect(led);
            g.setColour(states[i] && i != 2 ? ju::Colours::black : ju::Colours::white);
            g.drawText(letters[i], led, ju::Justification::centred);
        }
        auto inputArea = bounds.removeFromRight(46).withSizeKeepingCentre(46, 20);
        g.setFont(ju::FontOptions(13.0f, ju::Font::bold));
        for (size_t i = 0; i < inputs.channels.size(); ++i)
        {
            auto badge = inputArea.removeFromLeft(23).reduced(2, 0);
            g.setColour(inputs.channels[i] ? ju::Colour(0xffab1f29) : ju::Colour(0xff484848));
            g.fillRoundedRectangle(badge.toFloat(), 3.0f);
            g.setColour(ju::Colours::white);
            g.drawText(ju::String(static_cast<int>(i) + 1), badge, ju::Justification::centred);
        }
        auto body = bounds.reduced(5, 3);
        auto meter = body.removeFromBottom(8).reduced(1, 1);
        g.setColour(ju::Colours::black);
        g.fillRect(meter);
        if (auto found = meters.find(track.itemID); found != meters.end())
        {
            g.setColour(ju::Colours::limegreen);
            g.fillRect(meter.withWidth(ju::roundToInt(meter.getWidth() * found->second->level)));
        }
        g.setColour(colour.withMultipliedBrightness(selected ? 0.95f : 0.45f));
        g.fillRect(body.removeFromLeft(7));
        g.setColour(ju::Colours::white);
        g.setFont(ju::FontOptions(17.0f, ju::Font::bold));
        g.drawFittedText(ju::String(row + 1) + "   " + track.getName(), body.reduced(5, 0),
                         ju::Justification::centredLeft, 1);
    }

    void paintClip(ju::Graphics& g, te::Clip& clip, int y, ju::Colour colour)
    {
        auto positionInEdit = clip.getPosition();
        positionInEdit.time = displayedClipRange(clip);
        const auto left = timeX(positionInEdit.getStart());
        const auto bounds = clipBounds(positionInEdit.time, y);
        if (bounds.isEmpty())
            return;
        const auto selected = actionSelection.clips.contains(clip.itemID);
        const auto* missing = missingWave(&clip);
        if (missing)
            colour = ju::Colours::grey;
        g.setColour(colour.withMultipliedBrightness(selected ? 0.56f : 0.27f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        g.setColour(colour);
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, selected ? 2.0f : 1.0f);

        if (missing)
        {
            const ju::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(bounds.reduced(2));
            g.setColour(ju::Colour(0xff6b6b6b));
            for (int x = bounds.getX() - bounds.getHeight(); x < bounds.getRight(); x += 8)
                g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getBottom()),
                           static_cast<float>(x + bounds.getHeight()), static_cast<float>(bounds.getY()));
        }
        auto content = bounds.reduced(1, 2);
        const auto title = content.removeFromTop(16);
        {
            const ju::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(content);
            g.setColour(colour);
            if (auto* audio = dynamic_cast<te::AudioClipBase*>(&clip); audio && !missing)
            {
                auto file = audio->getPlaybackFile().getFile();
                if (!file.existsAsFile())
                    file = audio->getOriginalFile();
                if (file.existsAsFile())
                {
                    auto& thumb = arrangement.waveform(clip.itemID, file);
                    const auto fileStart = positionInEdit.getOffset().inSeconds();
                    const auto start = fileStart + timeAt(content.getX()) - positionInEdit.getStart().inSeconds();
                    const auto end = start + content.getWidth() / pixelsPerSecond;
                    thumb.drawChannels(g, content, start, end, 1.0f);
                }
            }
            else if (auto* midi = dynamic_cast<te::MidiClip*>(&clip))
            {
                auto& seq = edit->tempoSequence;
                const auto origin = seq.toBeats(positionInEdit.getStartOfSource());
                for (auto* note : midi->getSequence().getNotes())
                {
                    const auto a = timeX(seq.toTime(origin + (note->getStartBeat() - te::BeatPosition{})));
                    const auto b = timeX(seq.toTime(origin + (note->getEndBeat() - te::BeatPosition{})));
                    const auto pitch = content.getBottom() - 2 - note->getNoteNumber() * ju::jmax(1, content.getHeight() - 3) / 127;
                    g.fillRect(a, pitch, ju::jmax(2, b - a), 2);
                }
            }
        }
        g.setFont(ju::FontOptions(13.0f, ju::Font::bold));
        g.setColour(ju::Colours::white);
        auto name = clip.getName();
        if (missing)
        {
            const auto filename = missing->getOriginalFile().getFileName();
            name = "Missing: " + (filename.isNotEmpty() ? filename : name);
        }
        g.drawFittedText(name, title.reduced(4, 0), ju::Justification::centredLeft, 1);
    }

    void paintRecording(ju::Graphics& g, te::AudioTrack& track, int y, ju::Colour colour)
    {
        const ju::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(arrangement.getLocalBounds().withTrimmedLeft(headerWidth));
        for (auto* input : edit->getAllInputDevices())
        {
            const auto file = input->getRecordingFile(track.itemID);
            if (file == ju::File{})
                continue;
            auto live = engine.getRecordingThumbnailManager().getThumbnailFor(file);
            const auto duration = live->thumb->getTotalLength();
            if (duration <= 0.0)
                continue;
            const auto start = timeX(live->punchInTime);
            const auto end = timeX(live->punchInTime + te::TimeDuration::fromSeconds(duration));
            const auto bounds = ju::Rectangle<int>(start, y + 4, ju::jmax(1, end - start), rowHeight() - 8)
                .getIntersection(arrangement.getLocalBounds().withTrimmedLeft(headerWidth));
            if (bounds.isEmpty())
                continue;
            const auto visibleStart = timeAt(bounds.getX()) - live->punchInTime.inSeconds();
            g.setColour(colour.darker(0.8f));
            g.fillRect(bounds);
            g.setColour(ju::Colours::salmon);
            live->thumb->drawChannels(g, bounds, visibleStart, visibleStart + bounds.getWidth() / pixelsPerSecond, 1.0f);
        }
    }

    void paintArrangement(ju::Graphics& g)
    {
        const auto area = arrangement.getLocalBounds();
        const auto width = area.getWidth();
        g.fillAll(ju::Colour(0xff101010));
        g.setColour(ju::Colour(0xff242424));
        g.fillRect(0, 0, width, rulerHeight);
        g.setFont(ju::FontOptions(14.0f, ju::Font::bold));
        g.setColour(ju::Colours::white);
        g.drawText("TRACKS", 10, 0, headerWidth, rulerHeight, ju::Justification::centredLeft);
        if (!edit)
        {
            g.drawText("Choose File > New or Open", area, ju::Justification::centred);
            return;
        }

        const auto start = te::TimePosition::fromSeconds(viewStart);
        const auto end = te::TimePosition::fromSeconds(viewStart + visibleSeconds());
        const auto firstBar = edit->tempoSequence.toBarsAndBeats(start).bars;
        const auto lastBar = edit->tempoSequence.toBarsAndBeats(end).bars;
        {
            const ju::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(area.withTrimmedLeft(headerWidth));
            g.setFont(ju::FontOptions(13.0f));
            for (int bar = firstBar; bar <= lastBar; ++bar)
            {
                const auto barStart = barTime(bar);
                const auto x = timeX(barStart);
                const auto next = timeX(barTime(bar + 1));
                g.setColour(ju::Colour(0xff454545));
                g.drawVerticalLine(x, 0, static_cast<float>(area.getHeight()));
                if (next - x >= 28 || bar % 4 == 0)
                {
                    g.setColour(ju::Colours::white);
                    g.drawText(ju::String(bar + 1), x + 4, 0, 50, 20, ju::Justification::centredLeft);
                }
                const auto signatureAtBar = edit->tempoSequence.getTimeSigAt(barStart).numerator.get();
                const auto startBeat = edit->tempoSequence.toBeats(barStart);
                if (signatureAtBar > 0 && (next - x) / signatureAtBar >= 9)
                    for (int beat = 1; beat < signatureAtBar; ++beat)
                    {
                        const auto at = edit->tempoSequence.toTime(startBeat + te::BeatDuration::fromBeats(beat));
                        g.setColour(ju::Colour(0xff282828));
                        g.drawVerticalLine(timeX(at), 8, static_cast<float>(area.getHeight()));
                    }
            }
            const auto step = pixelsPerSecond >= 100.0 ? 1.0 : pixelsPerSecond >= 25.0 ? 2.0 : 10.0;
            for (auto second = std::ceil(viewStart / step) * step; second <= end.inSeconds(); second += step)
            {
                g.setColour(ju::Colour(0xffdedede));
                g.drawText(elapsed(second, false), timeX(te::TimePosition::fromSeconds(second)) + 4,
                           20, 62, 21, ju::Justification::centredLeft);
            }
        }
        const auto tracks = te::getAudioTracks(*edit);
        for (int row = firstRow, y = rulerHeight; row < tracks.size() && y < area.getBottom(); ++row, y += rowHeight())
        {
            auto& track = *tracks[row];
            const auto colour = track.getColour().isTransparent() ? palette(row) : track.getColour();
            paintTrack(g, track, row, y, colour);
            for (auto* clip : track.getClips())
                paintClip(g, *clip, y, colour);
            if (edit->getTransport().isRecording())
                paintRecording(g, track, y, colour);
            g.setColour(ju::Colour(0xff353535));
            g.drawHorizontalLine(y + rowHeight() - 1, 0, static_cast<float>(width));
        }
        const ju::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(area.withTrimmedLeft(headerWidth));
        if (const auto& range = rangePreview ? rangePreview : timeSelection)
        {
            const auto left = timeX(range->getStart());
            const auto right = timeX(range->getEnd());
            g.setColour(ju::Colour(0x504273aa));
            g.fillRect(left, 0, right - left, area.getHeight());
            g.setColour(ju::Colour(0xff6b9ce0));
            g.drawVerticalLine(left, 0, static_cast<float>(area.getHeight()));
            g.drawVerticalLine(right, 0, static_cast<float>(area.getHeight()));
        }
        g.setColour(ju::Colours::white);
        const auto cursorX = timeX(editCursor);
        g.drawVerticalLine(cursorX, 0, static_cast<float>(area.getHeight()));
        ju::Path cursorMarker;
        cursorMarker.addTriangle(static_cast<float>(cursorX - 5), 0.0f,
                                 static_cast<float>(cursorX + 5), 0.0f,
                                 static_cast<float>(cursorX), 7.0f);
        g.fillPath(cursorMarker);
        if (edit->getTransport().isPlaying() || paused)
        {
            g.setColour(edit->getTransport().isRecording() ? ju::Colours::red : ju::Colours::limegreen);
            const auto x = static_cast<float>(timeX(edit->getTransport().getPosition()));
            g.drawLine(x, 0, x, static_cast<float>(area.getHeight()), 2.0f);
        }
    }

    void mouseDown(const ju::MouseEvent& event) override
    {
        if (!edit || (!event.mods.isLeftButtonDown() && !event.mods.isRightButtonDown()))
            return;
        if (event.eventComponent == &inspector)
            actionSelection.requestedArea = EditingArea::inspector;
        else if (event.eventComponent == &workspace && activeTab == 1)
            actionSelection.requestedArea = EditingArea::window;
        updateControls();
    }

    ju::Rectangle<int> trackBounds(int y) const
    {
        return { 2, y + 2, headerWidth - 5, rowHeight() - 4 };
    }

    te::TimeRange displayedClipRange(const te::Clip& clip) const
    {
        const auto range = clip.getPosition().time;
        return dragItem == clip.itemID && dragPreview ? range + (*dragPreview - range.getStart()) : range;
    }

    ju::Rectangle<int> clipBounds(te::TimeRange range, int y) const
    {
        const auto left = timeX(range.getStart());
        const auto right = timeX(range.getEnd());
        return ju::Rectangle<int>(left, y + 3, ju::jmax(2, right - left), rowHeight() - 6)
            .getIntersection({ headerWidth, y + 3, arrangement.getWidth() - headerWidth, rowHeight() - 6 });
    }

    ju::Rectangle<int> arrangementAreaBounds(EditingArea area) const
    {
        auto bounds = arrangement.getLocalBounds();
        if (area == EditingArea::tracks)
            return bounds.removeFromLeft(headerWidth);
        return bounds.withTrimmedLeft(headerWidth);
    }

    void paintArrangementSelection(ju::Graphics& g) const
    {
        if (!edit)
            return;
        if (!selectionBox.isEmpty())
        {
            g.setColour(ju::Colours::white.withAlpha(0.13f));
            g.fillRect(selectionBox);
            g.setColour(ju::Colours::white);
            g.drawRect(selectionBox);
        }
        const auto active = activeEditingArea();
        if (active != EditingArea::timeline && active != EditingArea::tracks)
            return;
        g.setColour(ju::Colours::white);
        const auto area = arrangementAreaBounds(active);
        g.drawRect(area, 2);
        if (active != EditingArea::timeline || actionSelection.clips.empty())
            return;

        // Keep selection outlines visible above the ruler range and overlapping clips.
        const auto tracks = te::getAudioTracks(*edit);
        for (int row = firstRow, y = rulerHeight; row < tracks.size() && y < area.getBottom(); ++row, y += rowHeight())
            for (auto* clip : tracks[row]->getClips())
                if (actionSelection.clips.contains(clip->itemID))
                    if (const auto bounds = clipBounds(displayedClipRange(*clip), y); !bounds.isEmpty())
                        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, 2.0f);
    }

    void arrangementDown(const ju::MouseEvent& event)
    {
        if (!edit || (!event.mods.isLeftButtonDown() && !event.mods.isRightButtonDown()))
            return;
        if (arrangement.isShowing())
            arrangement.grabKeyboardFocus();
        const bool toggle = event.mods.isRightButtonDown();
        const auto area = event.x < headerWidth ? EditingArea::tracks : EditingArea::timeline;
        actionSelection.requestedArea = area;
        draggingRange = false;
        rangePreview.reset();
        dragPreview.reset();
        dragItem = {};
        boxAnchor.reset();
        selectionBox = {};
        const auto row = firstRow + ju::jmax(0, event.y - rulerHeight) / rowHeight();
        const auto y = rulerHeight + (row - firstRow) * rowHeight();
        auto* track = event.y >= rulerHeight ? trackAt(row) : nullptr;

        if (area == EditingArea::tracks)
        {
            if (track && trackBounds(y).contains(event.getPosition()))
            {
                ActionSelection::click(actionSelection.tracks, track->itemID, toggle);
                if (actionSelection.tracks.contains(track->itemID))
                    selection = row;
                else if (selection == row)
                    selectFirstSelectedTrack();
            }
            else if (!toggle)
            {
                actionSelection.tracks.clear();
                selection = -1;
                if (event.y >= rulerHeight)
                    boxAnchor = event.getPosition();
            }
        }
        else if (event.y < rulerHeight || event.mods.isShiftDown())
        {
            if (!toggle)
            {
                draggingRange = !edit->getTransport().isPlaying();
                dragAnchor = timeAt(event.x);
                seek(dragAnchor);
            }
        }
        else
        {
            if (track)
            {
                const auto clips = track->getClips();
                for (int index = clips.size(); --index >= 0;)
                {
                    auto* clip = clips[index];
                    if (!clipBounds(clip->getPosition().time, y).contains(event.getPosition()))
                        continue;
                    selection = row;
                    ActionSelection::click(actionSelection.clips, clip->itemID, toggle);
                    if (!toggle && (dynamic_cast<te::WaveAudioClip*>(clip) || dynamic_cast<te::MidiClip*>(clip)))
                    {
                        dragItem = clip->itemID;
                        dragAnchor = timeAt(event.x);
                    }
                    updateControls();
                    return;
                }
            }
            if (!toggle)
            {
                selection = track ? row : -1;
                actionSelection.clips.clear();
                boxAnchor = event.getPosition();
            }
        }
        updateControls();
    }

    void selectFirstSelectedTrack()
    {
        selection = -1;
        const auto tracks = te::getAudioTracks(*edit);
        for (int row = 0; row < tracks.size(); ++row)
            if (actionSelection.tracks.contains(tracks[row]->itemID))
            {
                selection = row;
                break;
            }
    }

    void updateSelectionBox(ju::Point<int> position)
    {
        const auto area = *actionSelection.requestedArea;
        const auto bounds = arrangementAreaBounds(area).withTrimmedTop(rulerHeight);
        selectionBox = ju::Rectangle<int>(std::min(boxAnchor->x, position.x), std::min(boxAnchor->y, position.y),
            std::abs(boxAnchor->x - position.x) + 1, std::abs(boxAnchor->y - position.y) + 1).getIntersection(bounds);
        auto& items = area == EditingArea::timeline ? actionSelection.clips : actionSelection.tracks;
        items.clear();
        const auto tracks = te::getAudioTracks(*edit);
        for (int row = firstRow, y = rulerHeight; row < tracks.size() && y < bounds.getBottom(); ++row, y += rowHeight())
        {
            if (area == EditingArea::tracks)
            {
                if (trackBounds(y).intersects(selectionBox))
                    items.insert(tracks[row]->itemID);
            }
            else
            {
                for (auto* clip : tracks[row]->getClips())
                    if (clipBounds(clip->getPosition().time, y).intersects(selectionBox))
                        items.insert(clip->itemID);
            }
        }
        if (area == EditingArea::tracks)
            selectFirstSelectedTrack();
    }

    void updateDragPreview(double pointerTime)
    {
        if (draggingRange)
        {
            const auto a = te::TimePosition::fromSeconds(snapSeconds(dragAnchor));
            const auto b = te::TimePosition::fromSeconds(snapSeconds(pointerTime));
            rangePreview = te::TimeRange(std::min(a, b), std::max(a, b));
        }
        else if (auto* clip = selectedClip(); clip && clip->itemID == dragItem)
        {
            const auto destination = clip->getPosition().getStart().inSeconds() + pointerTime - dragAnchor;
            dragPreview = te::TimePosition::fromSeconds(snapSeconds(destination));
        }
    }

    void arrangementDrag(const ju::MouseEvent& event)
    {
        if (!edit)
            return;
        if (boxAnchor && event.getDistanceFromDragStart() >= 5)
        {
            updateSelectionBox(event.getPosition());
            updateControls();
        }
        else if (!edit->getTransport().isPlaying() && std::abs(event.getDistanceFromDragStartX()) >= 5)
        {
            updateDragPreview(timeAt(event.x));
            arrangement.repaint();
        }
    }

    void finishDrag()
    {
        if (rangePreview && !rangePreview->isEmpty())
        {
            timeSelection = rangePreview;
            edit->getTransport().setLoopRange(*timeSelection);
            edit->markAsChanged();
            report("Time selection set");
        }
        else if (auto* clip = selectedClip(); clip && clip->itemID == dragItem && dragPreview
                 && clip->getPosition().getStart() != *dragPreview)
        {
            edit->getUndoManager().beginNewTransaction("Move item");
            clip->setStart(*dragPreview, false, true);
        }
    }

    void arrangementUp(const ju::MouseEvent& event)
    {
        if (edit && boxAnchor)
        {
            if (event.getDistanceFromDragStart() >= 5 || !selectionBox.isEmpty())
                updateSelectionBox(event.getPosition());
            else if (*actionSelection.requestedArea == EditingArea::timeline)
                seek(timeAt(event.x));
        }
        else if (edit && !edit->getTransport().isPlaying() && std::abs(event.getDistanceFromDragStartX()) >= 5)
        {
            updateDragPreview(timeAt(event.x));
            finishDrag();
        }
        boxAnchor.reset();
        selectionBox = {};
        dragItem = {};
        draggingRange = false;
        dragPreview.reset();
        rangePreview.reset();
        updateControls();
    }

    void updateControls()
    {
        playButton->setToggleState(edit && edit->getTransport().isPlaying(), ju::dontSendNotification);
        pauseButton->setToggleState(paused, ju::dontSendNotification);
        loopButton->setToggleState(edit && edit->getTransport().looping.get(), ju::dontSendNotification);
        clickButton->setToggleState(edit && edit->clickTrackEnabled.get(), ju::dontSendNotification);
        countButton->setToggleState(edit && edit->getCountInMode() != te::Edit::CountIn::none, ju::dontSendNotification);
        heightButton->setButtonText(panels.tallTracks ? "MAX" : "MIN");
        followButton->setToggleState(follow, ju::dontSendNotification);
        for (size_t i = 0; i < tabs.size(); ++i)
            static_cast<ju::TextButton*>(tabs[i])->setToggleState(i == activeTab, ju::dontSendNotification);
        workspace.setText(activeTab == 1 ? selectedEffects()
            : static_cast<ju::TextButton*>(tabs[activeTab])->getButtonText() + " - coming in a later release",
            ju::dontSendNotification);
        inspector.setText(trackAt(selection) ? "INSPECTOR\n\n" + trackAt(selection)->getName()
            + "\n\nTrack and item controls\ncoming in v0.22" : "INSPECTOR\n\nSelect a track", ju::dontSendNotification);
        if (edit)
        {
            std::erase_if(actionSelection.clips, [this](auto id) { return !te::findClipForID(*edit, id); });
            std::erase_if(actionSelection.tracks, [this](auto id) { return !te::findTrackForID(*edit, id); });
        }
        const auto active = activeEditingArea();
        if (actionSelection.requestedArea && actionSelection.requestedArea != active)
            actionSelection.requestedArea = active;
        inspector.setActive(edit && active == EditingArea::inspector);
        workspace.setActive(edit && active == EditingArea::window);
        for (const auto& slot : actionButtons)
        {
            const bool enabled = canDispatch(slot.action);
            slot.button->setColour(ju::TextButton::buttonColourId, enabled ? slot.colour : ju::Colour(0xff444444));
            slot.button->setEnabled(enabled);
        }

        undoButton->setEnabled(canUseHistory(false));
        redoButton->setEnabled(canUseHistory(true));

        const bool canChangePlaybackSettings = edit && !edit->getTransport().isPlaying();
        signature.setEnabled(canChangePlaybackSettings);
        recordMode.setEnabled(canChangePlaybackSettings);
        tempo.setEnabled(canChangePlaybackSettings);
        countButton->setEnabled(canChangePlaybackSettings);
        pauseButton->setEnabled(edit && !edit->getTransport().isRecording());

        if (edit)
        {
            auto& transport = edit->getTransport();
            const auto point = transport.getPosition();
            const auto bars = edit->tempoSequence.toBarsAndBeats(point);
            position.setText(ju::String(bars.bars + 1) + "." + ju::String(bars.getWholeBeats() + 1)
                + "." + ju::String(static_cast<int>(bars.getFractionalBeats().inBeats() * 100)).paddedLeft('0', 2)
                + "   |   " + elapsed(point.inSeconds(), true), ju::dontSendNotification);
            if (!tempo.hasKeyboardFocus(true))
                tempo.setText("BPM " + ju::String(edit->tempoSequence.getTempoAt(editCursor).getBpm(), 1), false);
            signature.setText(edit->tempoSequence.getTimeSigAt(editCursor).getStringTimeSig(), ju::dontSendNotification);
            recordMode.setSelectedId(edit->recordingPunchInOut ? 2 : 1, ju::dontSendNotification);
        }
        else
        {
            position.setText("1.1.00   |   0:00.000", ju::dontSendNotification);
            tempo.setText("BPM", false);
            signature.setText("4/4", ju::dontSendNotification);
            recordMode.setSelectedId(1, ju::dontSendNotification);
        }
        arrangement.repaint();
    }

    void timerCallback() override
    {
        if (edit)
        {
            auto& transport = edit->getTransport();
            if (follow && transport.isPlaying())
                reveal(transport.getPosition());
            pollMeters();
            std::erase_if(arrangement.waves, [this](const auto& entry) {
                return !te::findClipForID(*edit, entry.first);
            });
        }
        updateControls();
    }

    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;
    ju::File projectFile;
    ju::PropertiesFile settings;
    PanelState panels;
    ju::Component::SafePointer<ju::Component> shortcutHost;
    ju::ApplicationCommandManager commandManager;
    ju::MenuBarComponent menuBar { this };
    Arrangement arrangement { *this };
    Splitter inspectorDivider { *this, true }, workspaceDivider { *this, false };
    ju::Label position, status;
    AreaLabel inspector, workspace;
    ju::TooltipWindow tooltips { this };
    ju::TextEditor tempo;
    ju::ComboBox signature, recordMode, snap;
    ju::OwnedArray<ju::TextButton> buttons;
    ju::TextButton *playButton = nullptr, *pauseButton = nullptr, *loopButton = nullptr,
        *clickButton = nullptr, *countButton = nullptr, *heightButton = nullptr, *followButton = nullptr;
    ju::TextButton *undoButton = nullptr, *redoButton = nullptr;
    std::vector<ju::Component*> first, second, panelButtons, historyButtons, navigation, tabs;
    std::array<ActionGroup, 4> actionGroups;
    std::vector<ActionButton> actionButtons;
    ActionSelection actionSelection;
    std::unique_ptr<te::Clipboard::Clips> clipClipboard;
    std::map<te::EditItemID, std::unique_ptr<Meter>> meters;
    te::EditItemID dragItem;
    std::optional<ju::Point<int>> boxAnchor;
    ju::Rectangle<int> selectionBox;
    te::TimePosition editCursor;
    std::optional<te::TimeRange> timeSelection, rangePreview;
    std::optional<te::TimePosition> dragPreview;
    double dragAnchor = 0.0;
    int selection = -1, firstRow = 0;
    size_t activeTab = 0;
    double viewStart = 0.0, pixelsPerSecond = 100.0;
    bool follow = true, paused = false, draggingRange = false;
