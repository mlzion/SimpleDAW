// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    enum Command
    {
        newProject = 100, openProject, saveProject, saveProjectAs, importAudio, exportAudio, exitApp,
        undo, redo, cut, copy, paste, duplicate, deleteItem,
        audioTrack, midiTrack, midiItem, devices, about, armAudio, armMidi, synth, reverb, vst3,
        audioFile, locateFile, monitoringReport, playStop, startRecording
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
        addKeyListener(commandManager.getKeyMappings());
        addAndMakeVisible(menuBar);
        addAndMakeVisible(arrangement);
        addAndMakeVisible(inspector);
        addAndMakeVisible(workspace);
        addAndMakeVisible(inspectorDivider);
        addAndMakeVisible(workspaceDivider);
        for (auto* label : { &position, &status, &inspector, &workspace })
        {
            addAndMakeVisible(*label);
            label->setColour(ju::Label::textColourId, ju::Colours::white);
            label->setFont(ju::FontOptions(16.0f, ju::Font::bold));
        }
        position.setJustificationType(ju::Justification::centred);
        position.setColour(ju::Label::backgroundColourId, ju::Colour(0xff122844));
        inspector.setColour(ju::Label::backgroundColourId, ju::Colour(0xff292929));
        inspector.setJustificationType(ju::Justification::centredTop);
        workspace.setColour(ju::Label::backgroundColourId, ju::Colour(0xff252525));
        workspace.setJustificationType(ju::Justification::centred);

        button(first, "RECORD", [this] { record(); }, 0xffa60000);
        button(first, "STOP", [this] { stop(); });
        playButton = &button(first, "PLAY", [this] { play(); }, 0xff006e17);
        pauseButton = &button(first, "PAUSE", [this] { pause(); }, 0xff735100);
        loopButton = &button(first, "LOOP", [this] { toggleLoop(); }, 0xff18395c);
        first.push_back(&position);
        addAndMakeVisible(tempo);
        tempo.setJustification(ju::Justification::centred);
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

        for (auto id : { undo, redo, cut, copy, paste, duplicate, deleteItem })
        {
            ju::ApplicationCommandInfo info(id);
            getCommandInfo(id, info);
            editButtons.push_back(&button(toolbar, info.shortName.toUpperCase(), [this, id] {
                commandManager.invokeDirectly(id, true);
            }));
        }
        button(toolbar, "INSPECTOR", [this] { panels.inspectorOpen = !panels.inspectorOpen; saveLayout(); });
        button(toolbar, "WORKSPACE", [this] { panels.workspaceOpen = !panels.workspaceOpen; saveLayout(); });

        navigationButton(Arrow::screenLeft, "Scroll left one screen", [this] { scrollTime(-visibleSeconds()); });
        navigationButton(Arrow::screenRight, "Scroll right one screen", [this] { scrollTime(visibleSeconds()); });
        navigationButton(Arrow::left, "Scroll left one bar", [this] { scrollBar(-1); });
        navigationButton(Arrow::right, "Scroll right one bar", [this] { scrollBar(1); });
        button(navigation, "-", [this] { zoom(0.8); });
        button(navigation, "+", [this] { zoom(1.25); });
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
        removeKeyListener(commandManager.getKeyMappings());
        setApplicationCommandManagerToWatch(nullptr);
        menuBar.setModel(nullptr);
        stop();
        meters.clear();
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
        layoutRow(first, area.removeFromTop(44), { 11, 9, 10, 10, 9, 26, 12, 12 });
        layoutRow(second, area.removeFromTop(39), { 9, 10, 10, 9, 24, 13, 11, 12 });
        layoutRow(toolbar, area.removeFromTop(37));
        status.setBounds(area.removeFromBottom(26));
        layoutRow(navigation, area.removeFromBottom(38), { 8, 8, 8, 8, 8, 8, 8, 8, 13, 13 });

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

private:
    ju::TextButton& button(std::vector<ju::Component*>& row, const ju::String& label,
                           std::function<void()> action, uint32_t colour = 0xff292929)
    {
        auto& control = *buttons.add(new ju::TextButton(label));
        control.setColour(ju::TextButton::buttonColourId, ju::Colour(colour));
        control.setColour(ju::TextButton::buttonOnColourId, ju::Colour(0xff075ba8));
        control.setColour(ju::TextButton::textColourOffId, ju::Colours::white);
        control.setColour(ju::TextButton::textColourOnId, ju::Colours::white);
        control.onClick = std::move(action);
        control.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(control);
        row.push_back(&control);
        return control;
    }

    void navigationButton(Arrow arrow, const ju::String& title, std::function<void()> action)
    {
        auto& control = *buttons.add(new ArrowButton(arrow));
        control.setTitle(title);
        control.setTooltip(title);
        control.setColour(ju::TextButton::buttonColourId, ju::Colour(0xff292929));
        control.setMouseClickGrabsKeyboardFocus(false);
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
                          std::initializer_list<int> weights = {})
    {
        const auto width = bounds.getWidth();
        const auto total = weights.size() == 0 ? static_cast<int>(row.size())
                                              : std::accumulate(weights.begin(), weights.end(), 0);
        int accumulated = 0;
        auto weight = weights.begin();
        for (auto* control : row)
        {
            const auto left = accumulated * width / total;
            accumulated += weights.size() == 0 ? 1 : *weight++;
            const auto right = accumulated * width / total;
            control->setBounds(ju::Rectangle<int>(bounds.getX() + left, bounds.getY(),
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
            case undo: info.setActive(stopped && edit->getUndoManager().canUndo()); break;
            case redo: info.setActive(stopped && edit->getUndoManager().canRedo()); break;
            case cut: case copy: case paste: case duplicate: info.setActive(false); break;
            case deleteItem: info.setActive(stopped && selectedClip() != nullptr); break;
            case locateFile: info.setActive(stopped && missingWave(selectedClip()) != nullptr); break;
            case audioTrack: case midiTrack: info.setActive(stopped && trackCount() < maxTracks); break;
            case midiItem: case audioFile: case importAudio: case armAudio: case armMidi: case synth: case reverb: case vst3:
                info.setActive(stopped && hasTrack); break;
            case playStop: case startRecording: info.setActive(edit != nullptr); break;
            default: break;
        }
        if (id == saveProject) info.addDefaultKeypress('s', ju::ModifierKeys::ctrlModifier);
        if (id == playStop) info.addDefaultKeypress(ju::KeyPress::spaceKey, {});
        if (id == startRecording) info.addDefaultKeypress('r', {});
    }

    bool perform(const InvocationInfo& invocation) override
    {
        switch (invocation.commandID)
        {
            case newProject: chooseProject(true); break;
            case openProject: chooseProject(false); break;
            case saveProject: save(); break;
            case saveProjectAs: saveAs(); break;
            case audioFile: case importAudio: importWave(); break;
            case exportAudio: exportProject(); break;
            case exitApp: ju::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case undo: if (ready()) edit->getUndoManager().undo(); break;
            case redo: if (ready()) edit->getUndoManager().redo(); break;
            case deleteItem: deleteSelectedItem(); break;
            case locateFile: locateSelectedFile(); break;
            case audioTrack: addTrack(false); break;
            case midiTrack: addTrack(true); break;
            case midiItem: insertMidi(); break;
            case devices: configureDevices(); break;
            case monitoringReport: showMonitoringReport(); break;
            case about: ju::AlertWindow::showMessageBoxAsync(ju::MessageBoxIconType::InfoIcon,
                "About SimpleDAW", "SimpleDAW v0.21\nJUCE and Tracktion Engine"); break;
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
        return edit && selectedItem.isValid() ? te::findClipForID(*edit, selectedItem) : nullptr;
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

    void deleteSelectedItem()
    {
        if (!ready())
            return;
        if (auto* clip = selectedClip())
        {
            if (missingWave(clip) && !ju::AlertWindow::showOkCancelBox(ju::MessageBoxIconType::WarningIcon,
                    "Delete missing audio item?", "Remove " + clip->getName() + " from the project?",
                    "Delete Item", "Cancel", this))
                return;
            edit->getUndoManager().beginNewTransaction("Delete item");
            clip->removeFromParent();
            selectedItem = {};
        }
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
        selectedItem = dragItem = {};
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
            window->setName("SimpleDAW v0.21 - " + file.getFileName());
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
                    window->setName("SimpleDAW v0.21 - " + file.getFileName());
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
            selectTrack(count);
            if (count >= firstRow + rowsOnScreen())
                scrollRows(count - firstRow - rowsOnScreen() + 1);
        }
    }

    void selectTrack(int index)
    {
        selection = index;
        selectedItem = {};
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
                selectedItem = clip->itemID;
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
            return input.removeTarget(id, &undoManager);

        // A physical input can have several independent destinations.
        const auto target = input.setTarget(id, false, &undoManager, 0);
        if (!target)
            return ju::Result::fail(target.error());
        input.setRecordingEnabled(id, true);
        track.edit.restartPlayback();
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
        const auto selected = selection == row;
        auto bounds = ju::Rectangle<int>(2, y + 2, headerWidth - 5, rowHeight() - 4);
        g.setColour(ju::Colour(0xff242424).interpolatedWith(colour, selected ? 0.25f : 0.12f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        if (selected)
        {
            g.setColour(colour);
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

    void paintClip(ju::Graphics& g, te::Clip& clip, int y, ju::Colour colour, int width)
    {
        auto positionInEdit = clip.getPosition();
        if (dragItem == clip.itemID && dragPreview)
            positionInEdit.time = positionInEdit.time + (*dragPreview - positionInEdit.getStart());
        const auto left = timeX(positionInEdit.getStart());
        const auto right = timeX(positionInEdit.getEnd());
        const auto bounds = ju::Rectangle<int>(left, y + 3, ju::jmax(2, right - left), rowHeight() - 6)
            .getIntersection({ headerWidth, y + 3, width - headerWidth, rowHeight() - 6 });
        if (bounds.isEmpty())
            return;
        const auto selected = clip.itemID == selectedItem;
        const auto* missing = missingWave(&clip);
        if (missing)
            colour = ju::Colours::grey;
        g.setColour(colour.withMultipliedBrightness(selected ? 0.56f : 0.27f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        g.setColour(selected ? ju::Colours::white : colour);
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
                paintClip(g, *clip, y, colour, width);
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

    void arrangementDown(const ju::MouseEvent& event)
    {
        if (!edit || (!event.mods.isLeftButtonDown() && !event.mods.isPopupMenu()))
            return;
        const bool context = event.mods.isPopupMenu();
        arrangement.grabKeyboardFocus();
        draggingRange = false;
        rangePreview.reset();
        dragPreview.reset();
        dragItem = {};
        const auto row = firstRow + ju::jmax(0, event.y - rulerHeight) / rowHeight();
        if (event.x < headerWidth)
        {
            if (event.y >= rulerHeight && trackAt(row))
                selectTrack(row);
            return;
        }

        if (event.y < rulerHeight || event.mods.isShiftDown())
        {
            if (context)
                return;
            draggingRange = !edit->getTransport().isPlaying();
            dragAnchor = timeAt(event.x);
            seek(dragAnchor);
            return;
        }
        if (auto* track = trackAt(row))
        {
            selectTrack(row);
            const auto clips = track->getClips();
            for (int index = clips.size(); --index >= 0;)
            {
                auto* clip = clips[index];
                const auto range = clip->getPosition().time;
                if (event.x >= timeX(range.getStart()) && event.x < timeX(range.getEnd()))
                {
                    selectedItem = clip->itemID;
                    if (!context && (dynamic_cast<te::WaveAudioClip*>(clip) || dynamic_cast<te::MidiClip*>(clip)))
                    {
                        dragItem = clip->itemID;
                        dragAnchor = timeAt(event.x);
                    }
                    updateControls();
                    if (context)
                    {
                        ju::PopupMenu menu;
                        if (missingWave(clip))
                            menu.addCommandItem(&commandManager, locateFile);
                        menu.addCommandItem(&commandManager, deleteItem);
                        menu.showMenuAsync(ju::PopupMenu::Options().withTargetScreenArea(
                            ju::Rectangle<int>(event.getScreenX(), event.getScreenY(), 1, 1)));
                    }
                    return;
                }
            }
        }
        if (!context)
            seek(timeAt(event.x));
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
        if (!edit || edit->getTransport().isPlaying() || std::abs(event.getDistanceFromDragStartX()) < 5)
            return;
        updateDragPreview(timeAt(event.x));
        arrangement.repaint();
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
        if (edit && !edit->getTransport().isPlaying() && std::abs(event.getDistanceFromDragStartX()) >= 5)
        {
            updateDragPreview(timeAt(event.x));
            finishDrag();
        }
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
        for (size_t i = 0; i < editButtons.size(); ++i)
        {
            ju::ApplicationCommandInfo info(undo + static_cast<int>(i));
            getCommandInfo(info.commandID, info);
            editButtons[i]->setEnabled((info.flags & ju::ApplicationCommandInfo::isDisabled) == 0);
        }

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
    ju::ApplicationCommandManager commandManager;
    ju::MenuBarComponent menuBar { this };
    Arrangement arrangement { *this };
    Splitter inspectorDivider { *this, true }, workspaceDivider { *this, false };
    ju::Label position, status, inspector, workspace;
    ju::TooltipWindow tooltips { this };
    ju::TextEditor tempo;
    ju::ComboBox signature, recordMode, snap;
    ju::OwnedArray<ju::TextButton> buttons;
    ju::TextButton *playButton = nullptr, *pauseButton = nullptr, *loopButton = nullptr,
        *clickButton = nullptr, *countButton = nullptr, *heightButton = nullptr, *followButton = nullptr;
    std::vector<ju::Component*> first, second, toolbar, navigation, tabs, editButtons;
    std::map<te::EditItemID, std::unique_ptr<Meter>> meters;
    te::EditItemID selectedItem, dragItem;
    te::TimePosition editCursor;
    std::optional<te::TimeRange> timeSelection, rangePreview;
    std::optional<te::TimePosition> dragPreview;
    double dragAnchor = 0.0;
    int selection = -1, firstRow = 0;
    size_t activeTab = 0;
    double viewStart = 0.0, pixelsPerSecond = 100.0;
    bool follow = true, paused = false, draggingRange = false;
