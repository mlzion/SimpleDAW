// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    enum Command
    {
        newProject = 100, openProject, saveProject, saveProjectAs, importAudio, exportAudio, exitApp,
        undo, redo, cut, copy, paste, duplicate, deleteItem,
        audioTrack, midiTrack, midiItem, devices, about, armAudio, armMidi, synth, reverb, vst3,
        locateFile, monitoringReport, playStop, startRecording, importMidiFile
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
        ItemSelection clips;
        te::EditItemID track;

        EditingArea activeArea(bool hasWindow, bool hasInspector) const
        {
            if (requestedArea && (*requestedArea != EditingArea::window || hasWindow)
                && (*requestedArea != EditingArea::inspector || hasInspector))
                return *requestedArea;
            // Only existing selections can supply a fallback. Both panels currently have none.
            return clips.empty() && track.isValid() ? EditingArea::tracks : EditingArea::timeline;
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
        button(inspectorButtons, "COPY SETTINGS", [this] { useInspectorSettings(false); });
        button(inspectorButtons, "APPLY SETTINGS", [this] { useInspectorSettings(true); });
        inspector.setInterceptsMouseClicks(true, true);
        inspector.addAndMakeVisible(inspectorViewport);
        inspectorViewport.setViewedComponent(&inspectorControls, false);
        inspectorViewport.setScrollBarsShown(true, false);
        inspectorControls.addMouseListener(this, true);
        inspectorControls.addAndMakeVisible(inspectorMeter);
        for (auto* control : inspectorButtons)
            inspectorControls.addAndMakeVisible(control);
        button(clipInspectorButtons, "DUPLICATE", [this] { copyInspectedClip(false); });
        button(clipInspectorButtons, "CLONE", [this] { toggleInspectorClone(); });
        for (auto* control : clipInspectorButtons)
            inspectorControls.addChildComponent(control);
        inspectorControls.addChildComponent(inspectorName);
        inspectorName.setEditable(true);
        inspectorName.onEditorShow = [this] {
            actionSelection.requestedArea = EditingArea::inspector;
            updateControls();
        };
        inspectorName.onTextChange = [this] {
            if (!editingStopped())
                return;
            const auto name = inspectorName.getText().trim();
            if (name.isEmpty())
            {
                updateControls();
                return;
            }
            if (inspectorContext == EditingArea::tracks)
            {
                if (auto* track = actionTrack(); track && name != track->getName())
                {
                    edit->getUndoManager().beginNewTransaction("Rename track");
                    track->setName(name);
                }
            }
            else if (inspectorContext == EditingArea::timeline)
            {
                if (auto* clip = selectedClip(); clip && name != clip->getName())
                {
                    edit->getUndoManager().beginNewTransaction("Rename clip");
                    for (auto* member : cloneMembers(*clip))
                        member->setName(name);
                }
            }
            updateControls();
        };
        auto& mute = button(trackInspectorToggles, "MUTE", [this] {
            if (auto* track = actionTrack())
            {
                edit->getUndoManager().beginNewTransaction("Mute track");
                track->setMute(!track->isMuted(false));
                updateControls();
            }
        });
        auto& solo = button(trackInspectorToggles, "SOLO", [this] {
            if (auto* track = actionTrack())
            {
                edit->getUndoManager().beginNewTransaction("Solo track");
                track->setSolo(!track->isSolo(false));
                updateControls();
            }
        });
        inspectorControls.addChildComponent(mute);
        inspectorControls.addChildComponent(solo);
        for (auto* slider : { &inspectorVolume, &inspectorPan, &inspectorGain,
                             &inspectorFadeIn, &inspectorFadeOut })
        {
            slider->setTextBoxStyle(ju::Slider::TextBoxRight, false, 76, 26);
            slider->setChangeNotificationOnlyOnRelease(true);
            inspectorControls.addChildComponent(*slider);
        }
        inspectorVolume.setName("Track volume");
        inspectorPan.setName("Track pan");
        inspectorGain.setName("Clip gain");
        inspectorFadeIn.setName("Fade in");
        inspectorFadeOut.setName("Fade out");
        for (auto* slider : { &inspectorVolume, &inspectorPan, &inspectorGain,
                             &inspectorFadeIn, &inspectorFadeOut })
            slider->setTooltip(slider->getName());
        inspectorVolume.setSliderStyle(ju::Slider::LinearVertical);
        inspectorVolume.setRange(-60, 6, 0.1);
        inspectorVolume.setTextValueSuffix(" dB");
        inspectorPan.setRange(-100, 100, 1);
        inspectorPan.setDoubleClickReturnValue(true, 0);
        inspectorPan.textFromValueFunction = [](double value) {
            return value == 0 ? ju::String("Center")
                : ju::String(value < 0 ? "L" : "R") + ju::String(static_cast<int>(std::abs(value)));
        };
        inspectorPan.valueFromTextFunction = [](const ju::String& text) {
            const auto value = text.trim().toUpperCase();
            if (value.startsWithChar('L'))
                return -value.substring(1).getDoubleValue();
            if (value.startsWithChar('R'))
                return value.substring(1).getDoubleValue();
            return value.getDoubleValue();
        };
        inspectorGain.setRange(-60, 24, 0.1);
        inspectorGain.setTextValueSuffix(" dB");
        inspectorGain.setDoubleClickReturnValue(true, 0);
        for (auto* fade : { &inspectorFadeIn, &inspectorFadeOut })
            fade->setTextValueSuffix(" s");
        inspectorVolume.onValueChange = [this] {
            if (auto* track = actionTrack(); track && inspectorContext == EditingArea::tracks)
                if (auto* plugin = track->getVolumePlugin())
                {
                    edit->getUndoManager().beginNewTransaction("Track volume");
                    plugin->setVolumeDb(static_cast<float>(inspectorVolume.getValue()));
                }
        };
        inspectorPan.onValueChange = [this] {
            if (auto* track = actionTrack(); track && inspectorContext == EditingArea::tracks)
                if (auto* plugin = track->getVolumePlugin())
                {
                    edit->getUndoManager().beginNewTransaction("Track pan");
                    plugin->setPan(static_cast<float>(inspectorPan.getValue() / 100));
                }
        };
        inspectorGain.onValueChange = [this] {
            if (auto* clip = inspectedAudioClip())
            {
                edit->getUndoManager().beginNewTransaction("Clip gain");
                for (auto* member : cloneMembers(*clip))
                    if (auto* audio = dynamic_cast<te::AudioClipBase*>(member))
                        audio->setGainDB(static_cast<float>(inspectorGain.getValue()));
                updateControls();
            }
        };
        for (auto* slider : { &inspectorVelocity, &inspectorTranspose })
        {
            slider->setTextBoxStyle(ju::Slider::TextBoxRight, false, 76, 26);
            slider->setChangeNotificationOnlyOnRelease(true);
            inspectorControls.addChildComponent(*slider);
        }
        inspectorVelocity.setName("Clip velocity");
        inspectorVelocity.setTooltip("Clip velocity");
        inspectorVelocity.setRange(0, 200, 1);
        inspectorVelocity.setTextValueSuffix(" %");
        inspectorVelocity.setDoubleClickReturnValue(true, 100);
        inspectorTranspose.setName("Clip transpose");
        inspectorTranspose.setTooltip("Clip transpose");
        inspectorTranspose.setRange(-24, 24, 1);
        inspectorTranspose.setTextValueSuffix(" st");
        inspectorTranspose.setDoubleClickReturnValue(true, 0);
        inspectorVelocity.onValueChange = [this] {
            setMidiInspectorValue(midiVelocityProperty, static_cast<int>(inspectorVelocity.getValue()), 100, "Clip velocity");
        };
        inspectorTranspose.onValueChange = [this] {
            setMidiInspectorValue(midiTransposeProperty, static_cast<int>(inspectorTranspose.getValue()), 0, "Clip transpose");
        };
        inspectorFadeIn.onValueChange = [this] { setInspectorFade(true); };
        inspectorFadeOut.onValueChange = [this] { setInspectorFade(false); };
        button(loopInspectorButtons, "LOOP", [this] { changeInspectorLoop(true); });
        inspectorControls.addChildComponent(*loopInspectorButtons.front());
        inspectorControls.addChildComponent(inspectorLoopCount);
        inspectorLoopCount.setEditable(true);
        inspectorLoopCount.setName("Loop count");
        inspectorLoopCount.setTooltip("Loop count");
        inspectorLoopCount.onTextChange = [this] { changeInspectorLoop(false); };
        inspectorEffects.setModel(&inspectorEffectsModel);
        inspectorEffects.setRowHeight(32);
        inspectorControls.addChildComponent(inspectorEffects);
        button(effectButtons, "ADD REVERB", [this] { editInspectorEffect(0); });
        button(effectButtons, "DELETE", [this] { editInspectorEffect(1); });
        button(effectButtons, "ENABLE / DISABLE", [this] { editInspectorEffect(2); });
        button(effectButtons, "UP", [this] { editInspectorEffect(3); });
        button(effectButtons, "DOWN", [this] { editInspectorEffect(4); });
        for (auto* button : effectButtons)
            inspectorControls.addChildComponent(button);
        inspectorControls.addChildComponent(inspectorOutput);
        inspectorOutput.setName("Track output");
        inspectorOutput.setTooltip("Track output");
        inspectorOutput.addItem("Master", 1);
        inspectorOutput.onChange = [this] {
            if (!editingStopped() || inspectorContext != EditingArea::tracks
                || inspectorOutput.getSelectedId() != 1)
                return;
            if (auto* track = actionTrack())
            {
                edit->getUndoManager().beginNewTransaction("Route track to Master");
                track->getOutput().setOutputToDefaultDevice(false);
                updateControls();
            }
        };
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
            inspectorViewport.setBounds(inspector.getLocalBounds().withTrimmedTop(105));
            inspectorControls.setSize(std::max(0, inspectorViewport.getWidth() - inspectorViewport.getScrollBarThickness()), 580);
            auto controls = inspectorControls.getLocalBounds().reduced(8);
            inspectorName.setBounds(controls.removeFromTop(32));
            controls.removeFromTop(4);
            auto trackControls = controls;
            auto toggles = trackControls.removeFromTop(38);
            trackInspectorToggles[0]->setBounds(toggles.removeFromLeft(toggles.getWidth() / 2).reduced(2));
            trackInspectorToggles[1]->setBounds(toggles.reduced(2));
            auto fader = trackControls.removeFromTop(120);
            inspectorMeter.setBounds(fader.removeFromLeft(14).reduced(2));
            inspectorVolume.setBounds(fader);
            inspectorPan.setBounds(trackControls.removeFromTop(36));
            inspectorEffects.setBounds(trackControls.removeFromTop(150).reduced(2));
            layoutRow({ effectButtons[0], effectButtons[1], effectButtons[2] }, trackControls.removeFromTop(36));
            layoutRow({ effectButtons[3], effectButtons[4] }, trackControls.removeFromTop(36));
            for (auto* control : inspectorButtons)
                control->setBounds(trackControls.removeFromTop(36).reduced(2));
            inspectorOutput.setBounds(trackControls.removeFromTop(36).reduced(2));
            auto midiControls = controls;
            inspectorVelocity.setBounds(midiControls.removeFromTop(36));
            inspectorTranspose.setBounds(midiControls.removeFromTop(36));
            inspectorGain.setBounds(controls.removeFromTop(36));
            inspectorFadeIn.setBounds(controls.removeFromTop(36));
            inspectorFadeOut.setBounds(controls.removeFromTop(36));
            auto loopRow = controls.removeFromTop(36);
            loopInspectorButtons.front()->setBounds(loopRow.removeFromLeft(loopRow.getWidth() / 2).reduced(2));
            inspectorLoopCount.setBounds(loopRow.reduced(2));
            auto copies = controls.removeFromTop(36);
            clipInspectorButtons[0]->setBounds(copies.removeFromLeft(copies.getWidth() / 2).reduced(2));
            clipInspectorButtons[1]->setBounds(copies.reduced(2));
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
        first->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        second->state.setProperty(trackTypeProperty, "Audio", nullptr);
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
        if (!check(gui.activeEditingArea() == EditingArea::tracks && gui.actionSelection.track == second->itemID
                && gui.actionSelection.clips.size() == 2 && gui.canDispatch(Action::remove)
                && gui.canDispatch(Action::copy), "separate track focus and selection"))
            return false;
        if (!check(!gui.trackIsHighlighted(*first) && gui.trackIsHighlighted(*second),
                   "Track List focus highlights only its selected track"))
            return false;
        click({ gui.timeX(seconds(1.0)), 10 });
        if (!check(gui.actionSelection.track == second->itemID && gui.trackIsHighlighted(*first)
                && gui.trackIsHighlighted(*second) && gui.inspector.getText().contains("2 clips selected")
                && gui.canDispatch(Action::copy),
                   "Timeline focus highlights both clip tracks without retargeting Track List actions"))
            return false;
        click(midiPoint);
        if (!check(gui.trackIsHighlighted(*first) && !gui.trackIsHighlighted(*second)
                && gui.actionSelection.track == second->itemID,
                   "single Timeline selection clears stale track highlight"))
            return false;
        click(audioPoint, true);
        click({ 30, rowY(1) });
        if (!check(!gui.trackIsHighlighted(*first) && gui.trackIsHighlighted(*second)
                && gui.actionSelection.clips.size() == 2,
                   "returning to Tracks restores the stored highlight and retains both clips"))
            return false;
        const auto trackFrom = ju::Point<int>(20, rulerHeight + 1);
        const auto trackTo = ju::Point<int>(70, rowY(1));
        gui.arrangement.mouseDown(mouse(gui.arrangement, trackFrom, ju::ModifierKeys::leftButtonModifier, trackFrom));
        gui.arrangement.mouseUp(mouse(gui.arrangement, trackTo, 0, trackFrom, true));
        if (!check(!gui.actionSelection.track.isValid() && gui.actionSelection.clips.size() == 2,
                   "dragging in the header does not select tracks or change the Timeline selection"))
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

        click({ gui.timeX(seconds(4.13)), rowY(0) });
        if (!check(gui.actionSelection.clips.empty() && gui.canDispatch(Action::paste)
                && !gui.trackIsHighlighted(*first) && !gui.trackIsHighlighted(*second),
                   "empty Timeline selection sets paste destination without a stale track highlight"))
            return false;
        const auto destination = gui.editCursor;
        gui.dispatchAction(Action::paste);
        auto* pasted = gui.selectedClip();
        if (!check(dynamic_cast<te::MidiClip*>(pasted) && pasted->itemID != midiID
                && first->getClips().size() == 2 && std::abs((pasted->getPosition().getStart() - destination).inSeconds()) < 0.000001,
                   "MIDI Paste creates and selects an independent clip at the unsnapped cursor"))
            return false;
        const auto pasteID = pasted->itemID;
        gui.commandManager.invokeDirectly(undo, false);
        if (!check(first->getClips().size() == 1 && gui.actionSelection.clips.empty(), "one undo removes paste"))
            return false;
        gui.commandManager.invokeDirectly(redo, false);
        if (!check(te::findClipForID(*gui.edit, pasteID) && first->getClips().size() == 2, "redo restores paste"))
            return false;

        click(audioPoint);
        gui.dispatchAction(Action::copy);
        gui.dispatchAction(Action::remove);
        if (!check(!te::findClipForID(*gui.edit, audioID) && gui.actionSelection.clips.empty()
                && te::findClipForID(*gui.edit, pasteID)->getPosition().getStart() == destination, "Delete leaves the gap"))
            return false;
        gui.commandManager.invokeDirectly(undo, false);
        if (!check(te::findClipForID(*gui.edit, audioID) && second->getClips().size() == 1, "undo restores audio clip"))
            return false;

        gui.snap.setSelectedId(4, ju::dontSendNotification);
        click({ gui.timeX(seconds(2.71)), rowY(1) });
        gui.seek(2.71, false);
        const auto snapped = gui.pasteStart();
        gui.dispatchAction(Action::paste);
        auto* pastedWave = dynamic_cast<te::WaveAudioClip*>(gui.selectedClip());
        if (!check(pastedWave && pastedWave->itemID != audioID && pastedWave->getOriginalFile() == audio
                && pastedWave->getPosition().getStart() == snapped && second->getClips().size() == 2,
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
        gui.trackAt(2)->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        gui.trackAt(3)->state.setProperty(trackTypeProperty, "Audio", nullptr);
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
        gui.trackAt(3)->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        gui.dispatchAction(Action::paste);
        if (!check(gui.trackAt(2)->getClips().isEmpty() && gui.trackAt(3)->getClips().isEmpty()
                && gui.incompatiblePlacement(Action::paste).contains("Cannot place Audio clips"),
                   "one incompatible group paste destination leaves both lanes empty"))
            return false;
        gui.trackAt(3)->state.setProperty(trackTypeProperty, "Audio", nullptr);
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
        if (!check(gui.canDispatch(Action::left) && !gui.canDispatch(Action::endMinus)
                && !gui.canDispatch(Action::up) && gui.incompatiblePlacement(Action::up).isNotEmpty(),
                   "Snap Off retains horizontal MOVE and blocks an incompatible vertical group move"))
            return false;
        gui.snap.setSelectedId(4, ju::dontSendNotification);
        for (const auto action : { Action::startMinus, Action::startPlus, Action::endMinus, Action::endPlus,
                                   Action::split, Action::trim, Action::reserved })
            if (!check(!gui.canDispatch(action), "all group SIZE and MODIFY actions are disabled"))
                return false;

        gui.dispatchAction(Action::left);
        const auto movedStart = gui.trackAt(2)->getClips().getFirst()->getPosition().getStart();
        if (!check(movedStart < groupAnchor, "horizontal MOVE steps left"))
            return false;
        gui.dispatchAction(Action::up);
        if (!check(gui.trackAt(2)->getClips().size() == 1 && gui.trackAt(3)->getClips().size() == 1,
                   "incompatible vertical MOVE leaves both selected clips in place"))
            return false;

        auto* movingMidi = gui.trackAt(2)->getClips().getFirst();
        gui.dragItem = movingMidi->itemID;
        gui.dragAnchor = movedStart.inSeconds();
        gui.updateDragPreview(gui.dragAnchor, 1);
        gui.finishDrag();
        if (!check(!gui.dragPreview && gui.dragError.isNotEmpty()
                && gui.trackAt(2)->getClips().size() == 1 && gui.trackAt(3)->getClips().size() == 1,
                   "incompatible group drop keeps both clips at their original positions"))
            return false;
        gui.updateDragPreview(gui.dragAnchor, 0);
        gui.finishDrag();
        gui.dragItem = {};
        gui.dragPreview.reset();
        const auto firstClips = gui.trackAt(0)->getClips();
        if (!check(gui.trackAt(3)->getClips().isEmpty()
                && std::any_of(firstClips.begin(), firstClips.end(),
                               [movedStart](auto* clip) { return clip->getPosition().getStart() == movedStart; }),
                   "compatible group drag preserves timing and row offsets"))
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
        const auto firstTrackID = gui.trackAt(0)->itemID;
        gui.selectTrack(1);
        gui.dispatchAction(Action::up);
        if (!check(gui.trackAt(0)->itemID == secondTrackID && gui.trackAt(1)->itemID == firstTrackID
                && gui.actionSelection.track == secondTrackID && gui.actionSelection.clips.size() == 1,
                   "track MOVE reorders one track and preserves the Timeline selection"))
            return false;
        for (const auto action : { Action::startMinus, Action::startPlus, Action::endMinus, Action::endPlus })
            if (!check(!gui.canDispatch(action), "all SIZE actions are disabled with Tracks focused"))
                return false;
        gui.useHistory(false);
        if (!check(gui.trackAt(1)->itemID == secondTrackID && gui.trackAt(0)->itemID == firstTrackID,
                   "one global undo restores track order"))
            return false;
        gui.actionSelection.requestedArea = EditingArea::timeline;
        const auto pastedMidiID = gui.trackAt(2)->getClips().getFirst()->itemID;
        gui.actionSelection.clips = { pastedMidiID };
        const auto near = [](double actual, double expected) { return std::abs(actual - expected) < 1.0e-9; };
        for (const auto& [bpm, grid] : { std::pair{ 80.0, 2 }, std::pair{ 150.0, 4 } })
        {
            gui.edit->tempoSequence.getTempoAt(te::TimePosition{}).setBpm(bpm);
            const auto clipPosition = [&] { return te::findClipForID(*gui.edit, pastedMidiID)->getPosition(); };
            gui.snap.setSelectedId(grid, ju::dontSendNotification);
            const auto gridStart = clipPosition().getStart();
            gui.dispatchAction(Action::right);
            if (!check(near((clipPosition().getStart() - gridStart).inSeconds(),
                            60.0 / bpm / (1 << (grid - 2))), "Snap On uses the selected grid division"))
                return false;
            gui.useHistory(false);
            gui.snap.setSelectedId(1, ju::dontSendNotification);
            for (const auto action : { Action::right, Action::left, Action::startPlus, Action::startMinus,
                                       Action::endMinus, Action::endPlus })
            {
                const auto before = clipPosition();
                gui.dispatchAction(action);
                const auto after = clipPosition();
                const bool earlier = action == Action::left || action == Action::startMinus || action == Action::endMinus;
                const bool changesStart = action != Action::endMinus && action != Action::endPlus;
                const bool changesEnd = action != Action::startMinus && action != Action::startPlus;
                const double delta = earlier ? -0.010 : 0.010;
                if (!check(near((after.getStart() - before.getStart()).inSeconds(), changesStart ? delta : 0.0)
                        && near((after.getEnd() - before.getEnd()).inSeconds(), changesEnd ? delta : 0.0),
                           "Snap Off moves each affected boundary by exactly 10 ms"))
                    return false;
                gui.useHistory(false);
                if (!check(clipPosition() == before, "one undo restores the Snap Off edit"))
                    return false;
                gui.useHistory(true);
                if (!check(clipPosition() == after, "one redo restores the Snap Off edit"))
                    return false;
            }
        }
        auto* third = gui.trackAt(2);
        auto* fourth = gui.trackAt(3);
        first->setName("Source");
        first->getVolumePlugin()->setVolumeDb(-6.0f);
        first->getVolumePlugin()->setPan(0.25f);
        first->getOutput().setOutputToTrack(second);
        if (!check(insertBuiltIn<te::ReverbPlugin>(*first), "create source effect"))
            return false;
        auto* sourceEffect = first->pluginList.findFirstPluginOfType<te::ReverbPlugin>();
        auto* sourceParameter = sourceEffect->getAutomatableParameters().getFirst();
        sourceParameter->getCurve().addPoint(seconds(1), 0.5f, 0.0f);
        gui.selectTrack(0);
        gui.dispatchAction(Action::copy);
        gui.selectTrack(2);
        if (!check(gui.canDispatch(Action::paste) && !gui.canDispatch(Action::cut),
                   "Track Paste uses a separate anchor and Track Cut is disabled"))
            return false;
        gui.dispatchAction(Action::paste);
        auto* clone = gui.trackAt(3);
        if (!check(clone != nullptr, "track paste inserts below the anchor"))
            return false;
        auto* clonedEffect = clone->pluginList.findFirstPluginOfType<te::ReverbPlugin>();
        if (!check(gui.trackCount() == 5 && gui.actionSelection.track == clone->itemID && clone->getName() == "Source Copy" && gui.trackAt(2) == third
                && gui.trackAt(4) == fourth && clone->getColour() == first->getColour()
                && clone->getClips().size() == first->getClips().size()
                && clone->getClips().getFirst()->itemID != first->getClips().getFirst()->itemID
                && clone->getOutput().getDestinationTrack() == second && clonedEffect
                && clonedEffect->getAutomatableParameters().getFirst()->hasAutomationPoints(),
                   "Track Paste creates and selects a complete independent copy below the anchor"))
            return false;
        const auto cloneID = clone->itemID;
        gui.useHistory(false);
        if (!check(gui.trackCount() == 4 && !gui.trackForID(cloneID), "one undo removes the duplicate"))
            return false;
        gui.useHistory(true);
        if (!check(gui.trackCount() == 5 && gui.trackForID(cloneID), "one redo restores the duplicate"))
            return false;

        gui.selectTrack(2);
        gui.dispatchAction(Action::paste);
        if (!check(gui.trackCount() == 6 && gui.trackAt(3)->getName() == "Source Copy 2",
                   "repeated Track Paste uses a unique suffix"))
            return false;
        gui.useHistory(false);

        gui.selectTrack(0);
        gui.useInspectorSettings(false);
        gui.selectTrack(4);
        const auto fourthName = fourth->getName();
        const auto fourthClips = fourth->getClips().size();
        const auto fourthColor = fourth->getColour();
        gui.useInspectorSettings(true);
        auto* replacement = fourth->pluginList.findFirstPluginOfType<te::ReverbPlugin>();
        if (!check(replacement && !replacement->getAutomatableParameters().getFirst()->hasAutomationPoints()
                && std::abs(fourth->getVolumePlugin()->getVolumeDb() + 6.0f) < 0.0001f
                && fourth->getVolumePlugin()->getPan() == 0.25f && fourth->getOutput().getDestinationTrack() == second
                && fourth->getName() == fourthName && fourth->getColour() == fourthColor
                && fourth->getClips().size() == fourthClips,
                   "Inspector applies mixer, effects and output settings while leaving other track state intact"))
            return false;
        replacement->getAutomatableParameters().getFirst()->getCurve().addPoint(seconds(2), 0.4f, 0.0f);
        gui.selectTrack(2);
        gui.useInspectorSettings(false);
        gui.selectTrack(4);
        gui.useInspectorSettings(true);
        if (!check(fourth->pluginList.findFirstPluginOfType<te::ReverbPlugin>() == replacement,
                   "Inspector rejects an apply that would remove destination effect automation"))
            return false;
        gui.selectTrack(0);
        gui.useInspectorSettings(false);
        gui.selectTrack(4);
        gui.useInspectorSettings(true);
        auto* retained = fourth->pluginList.findFirstPluginOfType<te::ReverbPlugin>();
        if (!check(retained && retained->getAutomatableParameters().getFirst()->hasAutomationPoints(),
                   "Inspector preserves compatible destination automation"))
            return false;

        gui.edit->ensureNumberOfAudioTracks(20);
        gui.selectTrack(19);
        for (int count = 21; count <= maxTracks; ++count)
        {
            if (!check(gui.canDispatch(Action::paste), "Track Paste is available for tracks 21–24"))
                return false;
            gui.dispatchAction(Action::paste);
            if (!check(gui.trackCount() == count && gui.actionTrack() == gui.trackAt(count - 1)
                    && gui.selection < gui.firstRow + gui.rowsOnScreen(),
                       "Track Paste inserts and reveals the new bottom row"))
                return false;
        }
        const auto lastID = gui.actionTrack()->itemID;
        if (!check(!gui.canDispatch(Action::paste) && !gui.canAddTrack(), "track creation is disabled at 24 tracks"))
            return false;
        gui.dispatchAction(Action::paste);
        gui.addTrack(false);
        if (!check(gui.trackCount() == maxTracks, "neither Paste nor Insert creates a 25th track"))
            return false;
        gui.useHistory(false);
        if (!check(gui.trackCount() == maxTracks - 1 && !gui.trackForID(lastID), "undo removes track 24"))
            return false;
        gui.useHistory(true);
        if (!check(gui.trackCount() == maxTracks && gui.trackForID(lastID) && gui.save(), "redo restores and saves track 24"))
            return false;
        auto reopened = te::loadEditFromFile(engine, project);
        if (!check(reopened && te::getAudioTracks(*reopened).size() == maxTracks
                && te::findTrackForID(*reopened, lastID), "all 24 tracks survive reopening"))
            return false;
        reopened.reset();
        gui.setProject(te::createEmptyEdit(engine, project), project);
        return check(!gui.clipClipboard && !gui.trackClipboard && !gui.inspectorClipboard && gui.actionSelection.clips.empty(),
                     "opening a project clears clipboard and selection")
            && testClipPlacementAndMinimum(gui, engine, project, audio, check);
    }

    static bool testMidiImport(te::Engine& engine)
    {
        auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-midi-import", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        const auto project = dir.getChildFile("import.tracktionedit");
        const auto file = dir.getChildFile("conductor.mid");
        const auto writeMidi = [](const ju::File& target, bool multipleNotes) {
            ju::MidiFile midi;
            midi.setTicksPerQuarterNote(480);
            ju::MidiMessageSequence conductor;
            conductor.addEvent(ju::MidiMessage::tempoMetaEvent(1000000));
            midi.addTrack(conductor);
            ju::MidiMessageSequence notes;
            notes.addEvent(ju::MidiMessage::noteOn(1, 64, static_cast<ju::uint8>(91)), 240);
            notes.addEvent(ju::MidiMessage::noteOff(1, 64), 600);
            midi.addTrack(notes);
            if (multipleNotes)
                midi.addTrack(notes);
            ju::FileOutputStream stream(target);
            return stream.openedOk() && midi.writeTo(stream, 1);
        };
        if (!writeMidi(file, false))
            return false;

        MainGui gui(engine, dir.getChildFile("gui.settings"));
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.edit->ensureNumberOfAudioTracks(2);
        auto* midiTrack = gui.trackAt(0);
        auto* audioTrack = gui.trackAt(1);
        midiTrack->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        audioTrack->state.setProperty(trackTypeProperty, "Audio", nullptr);
        const auto start = te::TimePosition::fromSeconds(2.0);
        const auto multiple = dir.getChildFile("multiple.mid");
        if (!writeMidi(multiple, true)
            || gui.importMidi(*audioTrack, file, start).wasOk()
            || gui.importMidi(*midiTrack, multiple, start).wasOk()
            || gui.importMidi(*midiTrack, dir.getChildFile("missing.mid"), start).wasOk()
            || !midiTrack->getClips().isEmpty() || !audioTrack->getClips().isEmpty())
            return false;

        const auto bpm = gui.edit->tempoSequence.getTempoAt(start).getBpm();
        if (gui.importMidi(*midiTrack, file, start).failed())
            return false;
        const auto id = gui.selectedClip()->itemID;
        const auto verify = [&](te::Edit& editToCheck) {
            auto* clip = dynamic_cast<te::MidiClip*>(te::findClipForID(editToCheck, id));
            if (!clip || clip->getPosition().getStart() != start
                || editToCheck.tempoSequence.getTempoAt(start).getBpm() != bpm)
                return false;
            const auto& notes = clip->getSequence().getNotes();
            return notes.size() == 1 && notes[0]->getNoteNumber() == 64 && notes[0]->getVelocity() == 91
                && notes[0]->getStartBeat().inBeats() == 0.5
                && notes[0]->getLengthBeats().inBeats() == 0.75;
        };
        if (!verify(*gui.edit))
            return false;
        gui.useHistory(false);
        if (!midiTrack->getClips().isEmpty())
            return false;
        gui.useHistory(true);
        if (!verify(*gui.edit) || !gui.save())
            return false;
        auto reopened = te::loadEditFromFile(engine, project);
        return reopened && verify(*reopened);
    }

    static bool testMidiInspector(te::Engine& engine)
    {
        const auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-midi-inspector", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        const auto project = dir.getChildFile("inspector.tracktionedit");
        MainGui gui(engine, dir.getChildFile("gui.settings"));
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.addTrack(true);
        auto* track = gui.actionTrack();
        if (!track)
            return false;
        auto clip = te::insertMIDIClip(*track, "MIDI fixture",
            { te::TimePosition{}, te::TimePosition::fromSeconds(2) });
        if (!clip)
            return false;
        clip->getSequence().addNote(120, te::BeatPosition{}, te::BeatDuration::fromBeats(1), 90, 0, nullptr);
        gui.actionSelection.clips = { clip->itemID };
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.updateControls();

        const auto verify = [](te::MidiClip& item, int expectedPitch, int expectedVelocity) {
            const auto events = item.getSequence().exportToPlaybackMidiSequence(item, te::MidiList::TimeBase::beatsRaw, false);
            int ons = 0;
            int offs = 0;
            for (const auto* event : events)
            {
                const auto& message = event->message;
                if (!message.isNoteOn() && !message.isNoteOff())
                    continue;
                if (message.getNoteNumber() != expectedPitch)
                    return false;
                if (message.isNoteOn())
                {
                    if (message.getVelocity() != expectedVelocity)
                        return false;
                    ++ons;
                }
                else
                    ++offs;
            }
            const auto count = expectedVelocity == 0 ? 0 : 1;
            const auto& notes = item.getSequence().getNotes();
            return ons == count && offs == count && notes.size() == 1
                && notes[0]->getNoteNumber() == 120 && notes[0]->getVelocity() == 90
                && notes[0]->getLengthBeats().inBeats() == 1;
        };

        if (!verify(*clip, 120, 90))
            return false;
        gui.setMidiInspectorValue(midiTransposeProperty, 24, 0, "Transpose");
        gui.setMidiInspectorValue(midiVelocityProperty, 200, 100, "Velocity");
        if (!verify(*clip, 127, 127))
            return false;
        gui.setMidiInspectorValue(midiVelocityProperty, 0, 100, "Velocity");
        if (!verify(*clip, 127, 0))
            return false;
        gui.useHistory(false);
        if (!verify(*clip, 127, 127))
            return false;
        gui.setMidiInspectorValue(midiTransposeProperty, -24, 0, "Transpose");
        if (!verify(*clip, 96, 127) || !gui.save())
            return false;
        auto reopened = te::loadEditFromFile(engine, project);
        auto* restored = reopened ? dynamic_cast<te::MidiClip*>(te::findClipForID(*reopened, clip->itemID)) : nullptr;
        return restored && verify(*restored, 96, 127);
    }

    static bool testInspectorLoop(te::Engine& engine)
    {
        const auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-inspector-loop", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        const auto project = dir.getChildFile("loop.tracktionedit");
        MainGui gui(engine, dir.getChildFile("gui.settings"));
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.addTrack(true);
        auto* track = gui.actionTrack();
        const auto start = te::TimePosition::fromSeconds(1);
        const auto pattern = te::TimeDuration::fromSeconds(1);
        auto midi = te::insertMIDIClip(*track, "Pattern", { start, pattern });
        if (!midi || !gui.setClipLoop(*midi, true, 1) || !midi->isLooping()
            || !gui.setClipLoop(*midi, true, 3) || midi->getPosition().getLength() != pattern * 3)
            return false;
        gui.useHistory(false);
        if (midi->getPosition().getLength() != pattern)
            return false;
        gui.useHistory(true);
        if (!gui.setClipLoop(*midi, false, 3) || midi->isLooping()
            || midi->getPosition().getLength() != pattern || !gui.setClipLoop(*midi, true, 3))
            return false;

        gui.addTrack(false);
        track = gui.actionTrack();
        const auto wave = dir.getChildFile("pattern.wav");
        if (!writeTestWave(wave) || !insertAudio(*track, wave, start))
            return false;
        auto* audio = dynamic_cast<te::AudioClipBase*>(track->getClips().getFirst());
        if (!audio)
            return false;
        const auto length = audio->getPosition().getLength();
        const auto fade = length / 4.0;
        audio->setFadeIn(fade);
        if (!gui.setClipLoop(*audio, true, 2) || !audio->isLooping()
            || audio->getFadeIn() != te::TimeDuration{} || !gui.setClipLoop(*audio, false, 2)
            || audio->isLooping() || audio->getFadeIn() != fade || audio->getPosition().getLength() != length
            || !gui.save())
            return false;
        auto reopened = te::loadEditFromFile(engine, project);
        auto* restored = reopened ? te::findClipForID(*reopened, midi->itemID) : nullptr;
        return restored && restored->isLooping() && restored->getPosition().getLength() == pattern * 3;
    }

    static bool testInspectorClone(te::Engine& engine)
    {
        const auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-inspector-clone", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        const auto project = dir.getChildFile("clone.tracktionedit");
        MainGui gui(engine, dir.getChildFile("gui.settings"));
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.addTrack(true);
        const auto start = te::TimePosition::fromSeconds(1);
        const auto duration = te::TimeDuration::fromSeconds(1);
        auto source = te::insertMIDIClip(*gui.actionTrack(), "Parent", { start, duration });
        if (!source)
            return false;
        const auto sourceID = source->itemID;
        gui.actionSelection.clips = { sourceID };
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.editCursor = start + duration * 2;
        gui.updateControls();
        gui.copyInspectedClip(true);
        auto* clone = gui.selectedClip();
        if (!clone || clone == source.get() || clone->getPosition().getStart() != gui.editCursor
            || gui.cloneMembers(*clone).size() != 2 || gui.cloneMembers(*clone).front() != source.get())
            return false;
        const auto cloneID = clone->itemID;
        gui.setMidiInspectorValue(midiVelocityProperty, 50, 100, "Velocity");
        if (static_cast<int>(source->state.getProperty(midiVelocityProperty)) != 50)
            return false;
        gui.toggleInspectorClone();
        if (gui.cloneMembers(*source).size() != 1)
            return false;
        gui.useHistory(false);
        if (gui.cloneMembers(*source).size() != 2)
            return false;
        gui.edit->getUndoManager().beginNewTransaction("Remove parent");
        source->removeFromParent();
        if (gui.cloneMembers(*clone).size() != 1 || gui.cloneMembers(*clone).front() != clone)
            return false;
        gui.useHistory(false);
        if (!gui.save())
            return false;
        auto reopened = te::loadEditFromFile(engine, project);
        auto* restoredSource = reopened ? te::findClipForID(*reopened, sourceID) : nullptr;
        auto* restoredClone = reopened ? te::findClipForID(*reopened, cloneID) : nullptr;
        return restoredSource && restoredClone
            && restoredSource->state[cloneFamilyProperty] == restoredClone->state[cloneFamilyProperty]
            && restoredClone->state.hasProperty(cloneFamilyProperty)
            && static_cast<int>(restoredClone->state[midiVelocityProperty]) == 50;
    }

    static bool testInspectorEffects(te::Engine& engine)
    {
        const auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-inspector-effects", {});
        if (dir.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
        const auto project = dir.getChildFile("effects.tracktionedit");
        MainGui gui(engine, dir.getChildFile("gui.settings"));
        gui.setProject(te::createEmptyEdit(engine, project), project);
        gui.addTrack(false);
        auto* track = gui.actionTrack();
        if (!track)
            return false;
        for (int i = 0; i < 9; ++i)
            gui.editInspectorEffect(0);
        auto effects = audioInserts(track);
        if (effects.size() != 8)
            return false;
        const auto id = effects.front()->itemID;
        gui.inspectorEffectID = id;
        gui.editInspectorEffect(4);
        effects = audioInserts(track);
        if (effects.size() != 8 || effects[1]->itemID != id)
            return false;
        gui.editInspectorEffect(2);
        if (effects[1]->isEnabled() || !gui.save())
            return false;
        auto reopened = te::loadEditFromFile(engine, project);
        const auto restored = reopened ? audioInserts(te::findAudioTrackForID(*reopened, track->itemID))
                                       : std::vector<te::Plugin*>{};
        if (restored.size() != 8 || restored[1]->itemID != id || restored[1]->isEnabled())
            return false;
        gui.editInspectorEffect(1);
        return audioInserts(track).size() == 7;
    }

    static bool testClipPlacementAndMinimum(MainGui& gui, te::Engine& engine, const ju::File& project,
                                            const ju::File& audio, bool (*check)(bool, const char*))
    {
        using Action = UniversalAction;
        const auto seconds = te::TimePosition::fromSeconds<double>;
        gui.edit->ensureNumberOfAudioTracks(3);
        auto* first = gui.trackAt(0);
        auto* second = gui.trackAt(1);
        auto* third = gui.trackAt(2);
        first->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        second->state.setProperty(trackTypeProperty, "MIDI", nullptr);
        third->state.setProperty(trackTypeProperty, "Audio", nullptr);
        if (!check(insertAudio(*third, audio, seconds(3)), "create short WAV fixture"))
            return false;

        gui.selectTrack(2);
        gui.insertMidi();
        if (!check(third->getClips().size() == 1 && gui.status.getText().contains("Cannot place MIDI clips"),
                   "MIDI Item refuses an Audio track"))
            return false;
        gui.selectTrack(0);
        gui.importWave();
        if (!check(first->getClips().isEmpty() && gui.status.getText().contains("Cannot place Audio clips"),
                   "Audio File refuses a MIDI track before choosing a file"))
            return false;

        auto midi = te::insertMIDIClip(*first, "Threshold", { seconds(1), seconds(1.135) });
        if (!check(midi != nullptr, "create MIDI clip near the size threshold"))
            return false;
        const auto id = midi->itemID;
        midi = nullptr;
        const auto find = [&] { return te::findClipForID(*gui.edit, id); };
        gui.actionSelection.requestedArea = EditingArea::timeline;
        gui.actionSelection.clips = { id };
        gui.snap.setSelectedId(1, ju::dontSendNotification);
        const auto original = find()->getPosition();
        gui.dispatchAction(Action::endMinus);
        const auto atMinimum = find()->getPosition();
        if (!check(std::abs(atMinimum.getLength().inSeconds() - 0.125) < 1.0e-9
                && !gui.canDispatch(Action::endMinus) && !gui.canDispatch(Action::startPlus),
                   "Snap Off reaches the exact 4/4 threshold and blocks both further shrink actions"))
            return false;
        gui.useHistory(false);
        if (!check(find()->getPosition() == original, "Undo restores pre-trim duration"))
            return false;
        gui.useHistory(true);
        if (!check(find()->getPosition() == atMinimum, "Redo restores exact threshold"))
            return false;
        gui.snap.setSelectedId(4, ju::dontSendNotification);
        if (!check(!gui.canDispatch(Action::endMinus) && !gui.canDispatch(Action::startPlus),
                   "Snap On does not override the minimum"))
            return false;

        gui.snap.setSelectedId(1, ju::dontSendNotification);
        find()->setPosition({ { seconds(1), seconds(1.04) }, {} });
        if (!check(!gui.canDispatch(Action::startPlus) && !gui.canDispatch(Action::endMinus)
                && !gui.canDispatch(Action::startMinus) && gui.canDispatch(Action::endPlus),
                   "legacy short clip can grow without revealing media before its source"))
            return false;
        gui.dispatchAction(Action::endPlus);
        if (!check(std::abs(find()->getPosition().getLength().inSeconds() - 0.05) < 1.0e-9
                && !gui.canDispatch(Action::endMinus), "a short clip grows by one full Snap Off step"))
            return false;

        if (!check(gui.canDispatch(Action::down), "compatible single vertical move is available"))
            return false;
        gui.dispatchAction(Action::down);
        if (!check(find()->getTrack() == second && gui.incompatiblePlacement(Action::down).isNotEmpty(),
                   "single vertical move rejects the next incompatible lane"))
            return false;
        gui.dispatchAction(Action::down);
        if (!check(find()->getTrack() == second && third->getClips().size() == 1, "rejected move starts no transaction"))
            return false;
        gui.useHistory(false);
        if (!check(find()->getTrack() == first, "Undo returns the compatible move to its source"))
            return false;
        gui.useHistory(true);
        if (!check(find()->getTrack() == second, "Redo restores the compatible move"))
            return false;

        auto& sequence = gui.edit->tempoSequence;
        sequence.getTimeSigAt(seconds(0)).setStringTimeSig("7/8");
        sequence.insertTempo(te::BeatPosition::fromBeats(1.0), 70.0, 1.0f);
        te::tempo::Sequence::Position position(sequence.getInternalSequence());
        const auto ppq = [&position](double beats) {
            position.setPPQTime(beats);
            return position.getTime();
        };
        const te::TimeRange acrossTempo { ppq(0.4), ppq(0.8) };
        if (!check(gui.meetsMinimumDuration(acrossTempo, { ppq(0.4), ppq(0.61875) })
                && !gui.meetsMinimumDuration(acrossTempo, { ppq(0.4), ppq(0.61) }),
                   "7/8 minimum uses the start meter and musical duration across a tempo change"))
            return false;

        gui.selectTrack(0);
        gui.dispatchAction(Action::copy);
        gui.selectTrack(2);
        gui.dispatchAction(Action::paste);
        if (!check(trackType(*gui.trackAt(3)) == TrackType::midi && gui.trackAt(3)->getClips().isEmpty(),
                   "duplicate preserves the type of an empty MIDI track"))
            return false;

        third->state.removeProperty(trackTypeProperty, nullptr);
        second->state.removeProperty(trackTypeProperty, nullptr);
        if (!check(gui.save(), "save typed and untyped track fixtures"))
            return false;
        gui.setProject(te::loadEditFromFile(engine, project), project);
        if (!check(gui.trackAt(3)->state[trackTypeProperty].toString() == "MIDI"
                && gui.trackAt(1)->state[trackTypeProperty].toString() == "MIDI"
                && gui.trackAt(2)->state[trackTypeProperty].toString() == "Audio",
                   "reopen preserves explicit typing and infers homogeneous legacy track types"))
            return false;

        auto* mixed = gui.trackAt(2);
        if (!check(te::insertMIDIClip(*mixed, "Legacy MIDI", { seconds(1), seconds(2) }) != nullptr,
                   "create unexpected mixed legacy fixture"))
            return false;
        mixed->state.removeProperty(trackTypeProperty, nullptr);
        if (!check(gui.save(), "save mixed legacy fixture"))
            return false;
        gui.setProject(te::loadEditFromFile(engine, project), project);
        return check(gui.trackAt(2)->getClips().size() == 2 && trackType(*gui.trackAt(2)) == TrackType::mixed
                && gui.status.getText().contains("Mixed legacy tracks")
                && placementError(TrackType::audio, *gui.trackAt(2)).isNotEmpty(),
                     "unexpected mixed legacy track is reported and its clips remain intact");
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
        addTrack(true);
        const auto midiRow = selection;
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
        const auto clipCount = trackAt(midiRow)->getClips().size();
        invokeShortcut(deleteItem, key);
        return trackAt(midiRow)->getClips().size() == clipCount;
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
        using Action = UniversalAction;
        gui.selectTrack(0);
        gui.dispatchAction(Action::copy);
        gui.selectTrack(1);
        gui.dispatchAction(Action::paste);
        auto* clone = gui.trackAt(2);
        if (!clone || !isArmed(input, clone->itemID) || !isArmed(input, first.itemID)
            || !isArmed(input, second.itemID) || !clone->getOutput().usesDefaultAudioOut())
            return false;
        const auto cloneID = clone->itemID;
        gui.useHistory(false);
        if (gui.trackCount() != 2 || isArmed(input, cloneID))
            return false;
        gui.useHistory(true);
        if (gui.trackCount() != 3 || !isArmed(input, cloneID))
            return false;
        gui.useHistory(false);

        // Applying Inspector settings must not copy the source's input assignment.
        if (toggleInput(input, second).failed())
            return false;
        gui.selectTrack(0);
        gui.useInspectorSettings(false);
        gui.selectTrack(1);
        gui.useInspectorSettings(true);
        if (!isArmed(input, first.itemID) || isArmed(input, second.itemID)
            || toggleInput(input, second).failed())
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
            case 0: add({ newProject, openProject, saveProject, saveProjectAs, importAudio, importMidiFile, exportAudio, exitApp }); break;
            case 1: add({ undo, redo, cut, copy, paste, duplicate, deleteItem, locateFile }); break;
            case 2: add({ audioTrack, midiTrack, midiItem, synth, reverb, vst3 }); break;
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
        for (int id = newProject; id <= importMidiFile; ++id)
            list.add(id);
    }

    void getCommandInfo(ju::CommandID id, ju::ApplicationCommandInfo& info) override
    {
        static const char* names[] = {
            "New", "Open", "Save", "Save As", "Import Audio", "Export Audio", "Exit",
            "Undo", "Redo", "Cut", "Copy", "Paste", "Duplicate", "Delete",
            "Audio Track", "MIDI Track", "MIDI Item", "Audio and MIDI Devices", "About SimpleDAW",
            "Arm Audio Input", "Arm MIDI Input", "Add MIDI Synth", "Add Reverb", "Load VST3",
            "Locate File", "Audio Monitoring Report", "Play / Stop", "Record", "Import MIDI"
        };
        if (id < newProject || id > importMidiFile)
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
            case paste: info.setActive(canDispatch(UniversalAction::paste)
                || incompatiblePlacement(UniversalAction::paste).isNotEmpty()); break;
            case deleteItem: info.setActive(canDispatch(UniversalAction::remove) || canClearTimeSelection()); break;
            case locateFile:
                info.setActive(stopped && activeEditingArea() == EditingArea::timeline && missingWave(selectedClip()));
                break;
            case audioTrack: case midiTrack: info.setActive(stopped && canAddTrack()); break;
            case armAudio: info.setActive(edit && !edit->getTransport().isRecording() && hasTrack); break;
            case importMidiFile: info.setActive(stopped && actionTrack()); break;
            case midiItem: case importAudio: case armMidi: case synth: case reverb: case vst3:
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
            case importAudio: importWave(); break;
            case importMidiFile: chooseMidiImport(); break;
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
                {
                    if (audioInserts(track).size() >= 8)
                        report("The track has eight or more audio inserts");
                    else
                    {
                        edit->getUndoManager().beginNewTransaction("Add Reverb");
                        report(insertBuiltIn<te::ReverbPlugin>(*track) ? "Reverb inserted" : "Could not insert reverb");
                    }
                }
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
        return edit && id.isValid() ? dynamic_cast<te::AudioTrack*>(te::findTrackForID(*edit, id)) : nullptr;
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

    enum class TrackType { audio, midi, mixed };
    static inline const ju::Identifier trackTypeProperty { "simpleDawTrackType" };

    static ju::String trackTypeName(TrackType type)
    {
        switch (type)
        {
            case TrackType::audio: return "Audio";
            case TrackType::midi: return "MIDI";
            case TrackType::mixed: return "Mixed legacy";
        }
        return {};
    }

    static TrackType clipType(const te::Clip& clip)
    {
        return dynamic_cast<const te::MidiClip*>(&clip) ? TrackType::midi : TrackType::audio;
    }

    static TrackType trackType(const te::AudioTrack& track)
    {
        const auto saved = track.state[trackTypeProperty].toString();
        if (saved == "Audio") return TrackType::audio;
        if (saved == "MIDI") return TrackType::midi;
        if (saved == "Mixed legacy") return TrackType::mixed;

        bool audio = false, midi = false;
        for (auto* clip : track.getClips())
        {
            audio |= clipType(*clip) == TrackType::audio;
            midi |= clipType(*clip) == TrackType::midi;
        }
        if (audio && midi) return TrackType::mixed;
        if (midi || (!audio && track.getName().startsWithIgnoreCase("MIDI "))) return TrackType::midi;
        return TrackType::audio;
    }

    static ju::String placementError(TrackType type, const te::AudioTrack& destination)
    {
        const auto targetType = trackType(destination);
        if (targetType == type)
            return {};
        if (targetType == TrackType::mixed)
            return "Cannot place clips on a mixed legacy track. Its existing clips are unchanged.";
        return "Cannot place " + trackTypeName(type) + " clips on "
            + (targetType == TrackType::audio ? "an Audio" : "a MIDI") + " track.";
    }

    void warnPlacement(const ju::String& message)
    {
        report(message);
        if (isShowing())
            ju::AlertWindow::showMessageBoxAsync(ju::MessageBoxIconType::WarningIcon, "Incompatible track", message, "OK");
    }

    bool meetsMinimumDuration(te::TimeRange before, te::TimeRange after) const
    {
        const auto& sequence = edit->tempoSequence;
        const auto& meter = sequence.getTimeSigAt(before.getStart());
        const double minimum = meter.numerator.get() * 4.0 / (16.0 * meter.denominator.get());
        te::tempo::Sequence::Position position(sequence.getInternalSequence());
        const auto duration = [&position](te::TimeRange range) {
            position.set(range.getStart());
            const auto start = position.getPPQTime();
            position.set(range.getEnd());
            return position.getPPQTime() - start;
        };
        const auto previous = duration(before);
        const auto next = duration(after);
        constexpr auto tolerance = 1.0e-9;
        return next >= previous - tolerance || next >= minimum - tolerance;
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

    std::vector<Placement> pastePlan(ju::String* reason = nullptr) const
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
            const auto type = item.state.hasType(te::IDs::MIDICLIP) ? TrackType::midi : TrackType::audio;
            const auto error = placementError(type, *track);
            if (error.isNotEmpty())
            {
                if (reason) *reason = error;
                return {};
            }
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
            clearCloneIdentity(state, nullptr);
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

    inline static const ju::Identifier loopCountProperty { "simpleDawLoopCount" };
    inline static const ju::Identifier loopFadeInProperty { "simpleDawLoopFadeIn" };
    inline static const ju::Identifier loopFadeOutProperty { "simpleDawLoopFadeOut" };

    bool canControlLoop(const te::Clip& clip) const
    {
        if (clip.isLooping() && !clip.state.hasProperty(loopCountProperty))
            return false;
        if (dynamic_cast<const te::MidiClip*>(&clip))
            return true;
        auto* wave = dynamic_cast<const te::WaveAudioClip*>(&clip);
        return wave && !wave->isUsingMelodyne() && !wave->getAutoTempo() && !wave->getWarpTime()
            && wave->getOriginalFile().existsAsFile();
    }

    bool setClipLoop(te::Clip& clip, bool enable, int count, bool beginTransaction = true)
    {
        if (count < 1 || !canControlLoop(clip))
            return false;
        const auto position = clip.getPosition();
        const bool wasLooping = clip.isLooping();
        auto* midi = dynamic_cast<te::MidiClip*>(&clip);
        auto* audio = dynamic_cast<te::AudioClipBase*>(&clip);
        auto& tempo = edit->tempoSequence;
        const auto start = position.getStart();
        const auto firstBeat = tempo.toBeats(start);
        const auto patternBeats = midi ? (wasLooping ? midi->getLoopLengthBeats()
            : tempo.toBeats(position.getEnd()) - firstBeat) : te::BeatDuration{};
        const auto patternTime = wasLooping ? clip.getLoopLength() : position.getLength();
        const auto repeats = enable ? count : 1;
        const auto end = midi ? tempo.toTime(firstBeat + patternBeats * repeats)
            : start + patternTime * repeats;
        if (end <= start || end > te::Edit::getMaximumEditEnd())
            return false;

        auto& undo = edit->getUndoManager();
        if (beginTransaction)
            undo.beginNewTransaction("Clip loop");
        clip.state.setProperty(loopCountProperty, count, &undo);
        if (enable && !wasLooping)
        {
            if (midi)
            {
                const auto offset = tempo.toBeats(start) - tempo.toBeats(position.getStartOfSource());
                const auto sourceStart = te::BeatPosition{} + offset;
                midi->setLoopRangeBeats({ sourceStart, sourceStart + patternBeats });
            }
            else
            {
                const auto sourceStart = te::TimePosition{} + position.getOffset();
                audio->setLoopRange({ sourceStart, sourceStart + patternTime });
                if (audio->getLoopLength() != patternTime)
                {
                    if (beginTransaction)
                        undo.undoCurrentTransactionOnly();
                    return false;
                }
                audio->state.setProperty(loopFadeInProperty, audio->getFadeIn().inSeconds(), &undo);
                audio->state.setProperty(loopFadeOutProperty, audio->getFadeOut().inSeconds(), &undo);
                audio->setFadeIn({});
                audio->setFadeOut({});
            }
            clip.setPosition({ { start, end }, {} });
        }
        else if (!enable && wasLooping)
        {
            const auto offset = midi ? start - tempo.toTime(firstBeat - (midi->getLoopStartBeats() - te::BeatPosition{}))
                : audio->getLoopStart() - te::TimePosition{};
            if (midi)
                midi->setLoopRangeBeats({});
            else
                audio->setLoopRange({});
            clip.setPosition({ { start, end }, offset });
            if (audio)
            {
                audio->setFadeIn(te::TimeDuration::fromSeconds(static_cast<double>(audio->state.getProperty(loopFadeInProperty, 0.0))));
                audio->setFadeOut(te::TimeDuration::fromSeconds(static_cast<double>(audio->state.getProperty(loopFadeOutProperty, 0.0))));
                audio->state.removeProperty(loopFadeInProperty, &undo);
                audio->state.removeProperty(loopFadeOutProperty, &undo);
            }
        }
        else if (enable)
        {
            clip.setPosition({ { start, end }, position.getOffset() });
        }
        edit->markAsChanged();
        edit->restartPlayback();
        return true;
    }

    void changeInspectorLoop(bool toggle)
    {
        auto* clip = inspectorContext == EditingArea::timeline ? selectedClip() : nullptr;
        if (!editingStopped() || !clip)
            return;
        const auto text = inspectorLoopCount.getText().trim();
        const auto count = toggle ? static_cast<int>(clip->state.getProperty(loopCountProperty, 1))
            : text.containsOnly("0123456789") && text.length() <= 10 ? text.getLargeIntValue() : 0;
        const bool enable = toggle ? !clip->isLooping() : clip->isLooping();
        if (count < 1 || count > std::numeric_limits<int>::max())
            report("Loop count or pattern exceeds the available source or timeline");
        else
        {
            auto& undo = edit->getUndoManager();
            undo.beginNewTransaction("Clip loop");
            for (auto* member : cloneMembers(*clip))
                if (!setClipLoop(*member, enable, static_cast<int>(count), false))
                {
                    undo.undoCurrentTransactionOnly();
                    edit->restartPlayback();
                    report("Loop count or pattern exceeds the available source or timeline");
                    break;
                }
        }
        updateControls();
    }

    void setMidiInspectorValue(const ju::Identifier& property, int value, int defaultValue, const char* transaction)
    {
        if (!editingStopped() || inspectorContext != EditingArea::timeline)
            return;
        auto* clip = dynamic_cast<te::MidiClip*>(selectedClip());
        if (!clip || static_cast<int>(clip->state.getProperty(property, defaultValue)) == value)
            return;
        auto& undo = edit->getUndoManager();
        undo.beginNewTransaction(transaction);
        for (auto* member : cloneMembers(*clip))
            member->state.setProperty(property, value, &undo);
        edit->markAsChanged();
        edit->restartPlayback();
        updateControls();
    }

    te::AudioClipBase* inspectedAudioClip() const
    {
        return inspectorContext == EditingArea::timeline
            ? dynamic_cast<te::AudioClipBase*>(selectedClip()) : nullptr;
    }

    void setInspectorFade(bool fadeIn)
    {
        auto* clip = inspectedAudioClip();
        if (!editingStopped() || !clip || clip->isLooping())
            return;
        const auto opposite = fadeIn ? clip->getFadeOut() : clip->getFadeIn();
        const auto remaining = std::max(0.0, (clip->getPosition().getLength() - opposite).inSeconds());
        const auto requested = fadeIn ? inspectorFadeIn.getValue() : inspectorFadeOut.getValue();
        const auto length = te::TimeDuration::fromSeconds(std::min(requested, remaining));
        edit->getUndoManager().beginNewTransaction(fadeIn ? "Clip fade in" : "Clip fade out");
        for (auto* member : cloneMembers(*clip))
        {
            auto* audio = dynamic_cast<te::AudioClipBase*>(member);
            if (!audio)
                continue;
            if (fadeIn)
            {
                audio->setFadeInType(te::AudioFadeCurve::linear);
                audio->setFadeIn(length);
            }
            else
            {
                audio->setFadeOutType(te::AudioFadeCurve::linear);
                audio->setFadeOut(length);
            }
        }
        updateControls();
    }

    inline static const ju::Identifier cloneFamilyProperty { "simpleDawCloneFamily" };
    inline static const ju::Identifier cloneOrderProperty { "simpleDawCloneOrder" };

    static void clearCloneIdentity(ju::ValueTree state, ju::UndoManager* undo)
    {
        state.removeProperty(cloneFamilyProperty, undo);
        state.removeProperty(cloneOrderProperty, undo);
    }

    std::vector<te::Clip*> cloneMembers(te::Clip& clip) const
    {
        const auto family = static_cast<int>(clip.state.getProperty(cloneFamilyProperty, 0));
        if (family == 0)
            return { &clip };

        std::vector<te::Clip*> members;
        for (auto* track : te::getAudioTracks(*edit))
            for (auto* item : track->getClips())
                if (static_cast<int>(item->state.getProperty(cloneFamilyProperty, 0)) == family)
                    members.push_back(item);
        std::sort(members.begin(), members.end(), [](const auto* a, const auto* b) {
            return static_cast<int>(a->state.getProperty(cloneOrderProperty, 0))
                < static_cast<int>(b->state.getProperty(cloneOrderProperty, 0));
        });
        return members;
    }

    void toggleInspectorClone()
    {
        if (!editingStopped() || inspectorContext != EditingArea::timeline)
            return;
        auto* clip = selectedClip();
        if (!clip)
            return;
        const auto members = cloneMembers(*clip);
        if (members.size() > 1 && members.front() != clip)
        {
            auto& undo = edit->getUndoManager();
            undo.beginNewTransaction("Unclone clip");
            clearCloneIdentity(clip->state, &undo);
            edit->markAsChanged();
            updateControls();
        }
        else
            copyInspectedClip(true);
    }

    void copyInspectedClip(bool linked)
    {
        if (!editingStopped() || inspectorContext != EditingArea::timeline)
            return;
        auto* source = selectedClip();
        if (!source)
            return;
        auto* track = dynamic_cast<te::AudioTrack*>(source->getTrack());
        if (!track)
            return;
        const auto type = dynamic_cast<te::MidiClip*>(source) ? TrackType::midi : TrackType::audio;
        if (const auto error = placementError(type, *track); error.isNotEmpty())
            return warnPlacement(error);

        const auto position = source->getPosition();
        auto length = position.getLength();
        if (type == TrackType::midi)
        {
            auto& tempo = edit->tempoSequence;
            const auto beats = tempo.toBeats(position.getEnd()) - tempo.toBeats(position.getStart());
            length = tempo.toTime(tempo.toBeats(editCursor) + beats) - editCursor;
        }
        if (length <= te::TimeDuration{} || editCursor + length > te::Edit::getMaximumEditEnd())
            return report("The duplicate would exceed the available timeline");

        auto state = source->state.createCopy();
        te::EditItemID::remapIDs(state, nullptr, *edit);
        state.removeProperty(te::IDs::linkID, nullptr);
        clearCloneIdentity(state, nullptr);
        state.setProperty(te::IDs::start, editCursor.inSeconds(), nullptr);
        state.setProperty(te::IDs::length, length.inSeconds(), nullptr);
        auto& undo = edit->getUndoManager();
        undo.beginNewTransaction(linked ? "Clone clip" : "Duplicate clip");
        if (auto* duplicateClip = track->insertClipWithState(state))
        {
            if (linked)
            {
                auto family = static_cast<int>(source->state.getProperty(cloneFamilyProperty, 0));
                int order = 1;
                if (family == 0)
                {
                    for (auto* row : te::getAudioTracks(*edit))
                        for (auto* item : row->getClips())
                            family = std::max(family, static_cast<int>(item->state.getProperty(cloneFamilyProperty, 0)));
                    ++family;
                    source->state.setProperty(cloneFamilyProperty, family, &undo);
                    source->state.setProperty(cloneOrderProperty, 0, &undo);
                }
                else
                    for (auto* member : cloneMembers(*source))
                        order = std::max(order, static_cast<int>(member->state.getProperty(cloneOrderProperty, 0)) + 1);
                duplicateClip->state.setProperty(cloneFamilyProperty, family, &undo);
                duplicateClip->state.setProperty(cloneOrderProperty, order, &undo);
                duplicateClip->setColour(source->getColour());
            }
            actionSelection.clips = { duplicateClip->itemID };
            actionSelection.requestedArea = EditingArea::inspector;
            updateControls();
        }
        else
        {
            undo.undoCurrentTransactionOnly();
            report("Could not duplicate the clip");
        }
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

    std::vector<ClipChange> movePlan(const std::vector<te::Clip*>& clips, int rowDelta,
                                     te::TimeDuration delta, ju::String* reason = nullptr) const
    {
        const auto tracks = te::getAudioTracks(*edit);
        std::vector<ClipChange> changes;
        std::map<te::AudioTrack*, int> outgoing, incoming;
        if (rowDelta != 0)
            for (auto* clip : clips)
                ++outgoing[dynamic_cast<te::AudioTrack*>(clip->getTrack())];

        for (auto* clip : clips)
        {
            auto* source = dynamic_cast<te::AudioTrack*>(clip->getTrack());
            if (!ordinaryClip(*clip) || !source || source->isFrozen(te::Track::anyFreeze))
                return {};
            auto position = clip->getPosition();
            position.time = position.time + delta;
            if (position.getStart() < te::TimePosition{} || position.getEnd() > te::Edit::getMaximumEditEnd())
                return {};

            te::AudioTrack* destination = nullptr;
            if (rowDelta != 0)
            {
                destination = tracks[tracks.indexOf(source) + rowDelta];
                if (!destinationAvailable(destination, ++incoming[destination] - outgoing[destination])
                    || !clip->canBeAddedTo(*destination))
                    return {};
                const auto error = placementError(clipType(*clip), *destination);
                if (error.isNotEmpty())
                {
                    if (reason) *reason = error;
                    return {};
                }
            }
            changes.push_back({ clip, destination, position });
        }

        // Move outer rows first, so outgoing clips vacate a lane before incoming clips arrive.
        if (rowDelta != 0)
            std::stable_sort(changes.begin(), changes.end(), [&](const auto& a, const auto& b) {
                return rowDelta < 0 ? tracks.indexOf(a.destination) < tracks.indexOf(b.destination)
                                    : tracks.indexOf(a.destination) > tracks.indexOf(b.destination);
            });
        return changes;
    }

    bool applyClipChanges(const std::vector<ClipChange>& changes, const ju::String& transaction)
    {
        if (changes.empty())
            return false;
        auto& history = edit->getUndoManager();
        history.beginNewTransaction(transaction);
        for (const auto& change : changes)
        {
            if (change.destination && (!change.clip->moveTo(*change.destination) || change.clip->getTrack() != change.destination))
            {
                history.undoCurrentTransactionOnly();
                report("Could not move clips");
                return false;
            }
            if (change.clip->getPosition() != change.position)
                change.clip->setPosition(change.position);
        }
        return true;
    }

    std::vector<ClipChange> clipChanges(UniversalAction action, const std::vector<te::Clip*>& clips,
                                        ju::String* reason = nullptr) const
    {
        using Action = UniversalAction;
        if (clips.empty())
            return {};
        if (action == Action::up || action == Action::down)
            return movePlan(clips, action == Action::up ? -1 : 1, {}, reason);
        const bool move = action == Action::left || action == Action::right;
        const bool start = action == Action::startMinus || action == Action::startPlus;
        const bool earlier = action == Action::left || action == Action::startMinus || action == Action::endMinus;
        const auto grid = snap.getSelectedId();
        auto delta = te::TimeDuration::fromSeconds(earlier ? -0.010 : 0.010);
        if (grid > 1)
        {
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

        if (move)
            return movePlan(clips, 0, delta);

        auto* clip = clips.front();
        auto* source = dynamic_cast<te::AudioTrack*>(clip->getTrack());
        if (clips.size() != 1 || !ordinaryClip(*clip) || !source || source->isFrozen(te::Track::anyFreeze)
            || clip->isLooping() || clip->getNumTakes(true) > 1)
            return {};

        auto position = clip->getPosition();
        const auto original = position.time;
        if (start)
        {
            const auto next = position.getStart() + delta;
            if (next < te::TimePosition{} || next >= position.getEnd())
                return {};
            position.time = position.time.withStart(next);
            position.offset = position.offset + delta;
        }
        else
        {
            const auto next = position.getEnd() + delta;
            if (next <= position.getStart() || next > te::Edit::getMaximumEditEnd())
                return {};
            position.time = position.time.withEnd(next);
        }

        if (position.offset < te::TimeDuration{} || !meetsMinimumDuration(original, position.time))
            return {};
        if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
        {
            const te::AudioFile sourceFile(engine, wave->getOriginalFile());
            if (!sourceFile.isValid() || wave->getWarpTime() || wave->getAutoTempo()
                || (position.offset + position.time.getLength()).inSeconds() * wave->getSpeedRatio()
                    > sourceFile.getLength() + 1.0e-9)
                return {};
        }
        std::vector<ClipChange> changes { { clip, nullptr, position } };
        for (auto* member : cloneMembers(*clip))
        {
            if (member == clip)
                continue;
            const auto memberStart = member->getPosition().getStart();
            auto size = position.getLength();
            auto offset = position.getOffset();
            if (dynamic_cast<te::MidiClip*>(clip))
            {
                auto& tempo = edit->tempoSequence;
                const auto beat = tempo.toBeats(position.getStart());
                const auto duration = tempo.toBeats(position.getEnd()) - beat;
                const auto offsetBeats = beat - tempo.toBeats(position.getStartOfSource());
                const auto memberBeat = tempo.toBeats(memberStart);
                size = tempo.toTime(memberBeat + duration) - memberStart;
                offset = memberStart - tempo.toTime(memberBeat - offsetBeats);
            }
            const auto end = memberStart + size;
            if (end <= memberStart || end > te::Edit::getMaximumEditEnd())
                return {};
            changes.push_back({ member, nullptr, { { memberStart, end }, offset } });
        }
        return changes;
    }

    te::AudioTrack* actionTrack() const { return trackForID(actionSelection.track); }

    te::AudioTrack* trackMoveNeighbor(UniversalAction action) const
    {
        auto* track = actionTrack();
        if (!track)
            return nullptr;
        const auto tracks = te::getAudioTracks(*edit);
        auto* neighbor = tracks[tracks.indexOf(track) + (action == UniversalAction::up ? -1 : 1)];
        return neighbor && neighbor->state.getParent() == track->state.getParent() ? neighbor : nullptr;
    }

    struct TrackInputSetting
    {
        ju::ValueTree device, assignment;
        std::optional<te::InputDevice::MonitorMode> monitoring;
    };

    struct TrackOutputSetting
    {
        te::EditItemID track;
        ju::String device;
    };

    struct TrackSnapshot
    {
        ju::ValueTree state;
        TrackOutputSetting output;
        std::vector<TrackInputSetting> inputs;
    };

    struct TrackSettings
    {
        std::vector<ju::ValueTree> plugins;
        TrackOutputSetting output;
    };

    TrackOutputSetting trackOutput(te::AudioTrack& track) const
    {
        auto& output = track.getOutput();
        if (auto* destination = output.getDestinationTrack())
            return { destination->itemID, {} };
        return { {}, output.state.getChildWithName(te::IDs::DEVICE)[te::IDs::name].toString() };
    }

    bool outputAvailable(const TrackOutputSetting& output) const
    {
        if (output.track.isValid())
            return trackForID(output.track) != nullptr;
        auto& manager = engine.getDeviceManager();
        return output.device.isEmpty() || output.device == te::DeviceManager::getDefaultAudioOutDeviceName(false)
            || output.device == te::DeviceManager::getDefaultMidiOutDeviceName(false)
            || manager.findOutputDeviceForID(output.device) || manager.findOutputDeviceWithName(output.device);
    }

    void setTrackOutput(te::AudioTrack& track, const TrackOutputSetting& output)
    {
        if (auto* destination = trackForID(output.track))
            track.getOutput().setOutputToTrack(destination);
        else
            track.getOutput().setOutputToDeviceID(output.device);
    }

    void copyTrack(te::AudioTrack& track)
    {
        track.flushStateToValueTree();
        TrackSnapshot snapshot { track.state.createCopy(), trackOutput(track), {} };
        snapshot.state.setProperty(trackTypeProperty, trackTypeName(trackType(track)), nullptr);
        for (int i = 0; i < snapshot.state.getNumChildren(); ++i)
            clearCloneIdentity(snapshot.state.getChild(i), nullptr);
        for (auto device : edit->state.getChildWithName(te::IDs::INPUTDEVICES))
            for (auto assignment : device)
                if (assignment.hasType(te::IDs::INPUTDEVICEDESTINATION)
                    && te::EditItemID::fromProperty(assignment, te::IDs::targetID) == track.itemID)
                {
                    auto* input = engine.getDeviceManager().findInputDeviceForID(device[te::IDs::deviceID].toString());
                    snapshot.inputs.push_back({ device, assignment.createCopy(),
                        input ? std::optional{ input->getMonitorMode() } : std::nullopt });
                }
        trackClipboard = std::move(snapshot);
    }

    bool canAddTrack() const
    {
        return edit && trackCount() < maxTracks
            && te::getAllTracks(*edit).size() < engine.getEngineBehaviour().getEditLimits().maxNumTracks;
    }

    bool canPasteTrack() const
    {
        return actionTrack() && trackClipboard && canAddTrack() && outputAvailable(trackClipboard->output)
            && trackClipboard->state[trackTypeProperty].toString() != "Mixed legacy";
    }

    void pasteTrack(te::AudioTrack& anchor)
    {
        if (!canPasteTrack())
            return;
        const auto& snapshot = *trackClipboard;
        const auto devices = edit->state.getChildWithName(te::IDs::INPUTDEVICES);
        if (!snapshot.inputs.empty())
            edit->getTransport().ensureContextAllocated();
        const auto instances = edit->getAllInputDevices();
        for (const auto& input : snapshot.inputs)
        {
            if (input.device.getParent() != devices || std::none_of(instances.begin(), instances.end(), [&](auto* instance) {
                    // Track-to-track input devices allow only one target.
                    return instance->state == input.device && !instance->owner.isTrackDevice() && !instance->isRecording()
                        && input.monitoring && instance->owner.getMonitorMode() == *input.monitoring;
                }))
            {
                report("Cannot duplicate the copied track's input assignment");
                return;
            }
        }

        auto parent = anchor.state.getParent();
        const auto insertIndex = parent.indexOf(anchor.state) + 1;
        if (!parent.isValid() || insertIndex <= 0)
            return;

        auto state = snapshot.state.createCopy();
        te::EditItemID::IDMap remapped;
        te::EditItemID::remapIDs(state, nullptr, *edit, &remapped);
        const auto remapPluginLinks = [&](auto&& visit, ju::ValueTree node) -> void {
            const auto master = te::EditItemID::fromProperty(node, te::IDs::masterPluginID);
            if (const auto replacement = remapped.find(master); replacement != remapped.end())
                node.setProperty(te::IDs::masterPluginID, replacement->second, nullptr);
            for (auto child : node)
                visit(visit, child);
        };
        remapPluginLinks(remapPluginLinks, state);
        const auto id = te::EditItemID::fromID(state);
        const auto baseName = state[te::IDs::name].toString() + " Copy";
        auto name = baseName;
        const auto tracks = te::getAllTracks(*edit);
        for (int suffix = 2; std::any_of(tracks.begin(), tracks.end(), [&](auto* track) {
                 return track->getName().equalsIgnoreCase(name);
             }); ++suffix)
            name = baseName + " " + ju::String(suffix);
        state.setProperty(te::IDs::name, name, nullptr);
        // Track output numbers are positional. Resolve the saved target identity after insertion.
        auto device = state.getChildWithName(te::IDs::OUTPUTDEVICES).getChildWithName(te::IDs::DEVICE);
        if (device.isValid())
            device.setProperty(te::IDs::name, "", nullptr);

        auto& history = edit->getUndoManager();
        history.beginNewTransaction("Duplicate track");
        parent.addChild(state, insertIndex, &history);
        auto* duplicate = trackForID(id);
        if (!duplicate)
        {
            history.undoCurrentTransactionOnly();
            report("Could not duplicate the track");
            return;
        }
        setTrackOutput(*duplicate, snapshot.output);
        for (auto input : snapshot.inputs)
        {
            auto assignment = input.assignment.createCopy();
            assignment.setProperty(te::IDs::targetID, id, nullptr);
            input.device.addChild(assignment, -1, &history);
        }
        selectTrack(te::getAudioTracks(*edit).indexOf(duplicate));
        revealTrack(selection);
        edit->restartPlayback();
    }

    void copyInspectorSettings(te::AudioTrack& track)
    {
        TrackSettings settings { {}, trackOutput(track) };
        for (auto* plugin : track.pluginList)
        {
            plugin->flushPluginStateToValueTree();
            auto state = plugin->state.createCopy();
            te::AutomationCurve::removeAllAutomationCurvesRecursively(state);
            state.removeChild(state.getChildWithName(te::IDs::MODIFIERASSIGNMENTS), nullptr);
            state.removeProperty(te::IDs::masterPluginID, nullptr);
            settings.plugins.push_back(state);
        }
        inspectorClipboard = std::move(settings);
    }

    struct TrackSettingsApply
    {
        te::AudioTrack* track;
        const TrackSettings* settings;
        std::vector<te::Plugin::Ptr> plugins;
        std::map<te::EditItemID, te::EditItemID> replacedPlugins;
    };

    bool canApplyInspectorSettings(te::AudioTrack& destination) const
    {
        if (!inspectorClipboard || !outputAvailable(inspectorClipboard->output)
            || destination.isFrozen(te::Track::anyFreeze))
            return false;
        auto* output = trackForID(inspectorClipboard->output.track);
        return !output || (output != &destination && !output->getOutput().feedsInto(&destination));
    }

    bool prepareTrackPlugins(TrackSettingsApply& item)
    {
        const auto& oldPlugins = item.track->pluginList;
        if (item.track->isFrozen(te::Track::anyFreeze)
            || item.settings->plugins.size() > static_cast<size_t>(engine.getEngineBehaviour().getEditLimits().maxPluginsOnTrack))
            return false;

        std::map<te::EditItemID, te::EditItemID> ids;
        for (const auto& snapshot : item.settings->plugins)
        {
            auto state = snapshot.createCopy();
            te::EditItemID::remapIDs(state, nullptr, *edit, &ids);
            auto plugin = edit->getPluginCache().getOrCreatePluginFor(state);
            if (!plugin || plugin->isMissing() || !item.track->canContainPlugin(plugin.get()))
                return false;
            const auto sidechain = plugin->getSidechainSourceID();
            if (sidechain.isValid() && (!trackForID(sidechain) || sidechain == item.track->itemID))
                return false;
            item.plugins.push_back(plugin);
        }

        for (int index = 0; index < oldPlugins.size(); ++index)
        {
            auto* old = oldPlugins[index];
            const auto matches = [old](te::Plugin* candidate) {
                return old->getPluginType() == candidate->getPluginType()
                    && old->getIdentifierString() == candidate->getIdentifierString();
            };
            const auto before = std::count_if(oldPlugins.begin(), oldPlugins.begin() + index, matches);
            const auto count = std::count_if(oldPlugins.begin(), oldPlugins.end(), matches);
            std::vector<te::Plugin*> candidates;
            for (const auto& plugin : item.plugins)
                if (matches(plugin.get()))
                    candidates.push_back(plugin.get());
            // Repeated effects are paired by occurrence only when the pairing is unambiguous.
            auto* replacement = candidates.size() == static_cast<size_t>(count) ? candidates[size_t(before)] : nullptr;
            auto state = replacement ? replacement->state : ju::ValueTree{};

            // Parameter IDs and ranges must agree before carrying automation into a new effect.
            for (auto child : old->state)
            {
                if (child.hasType(te::IDs::AUTOMATIONCURVE))
                {
                    const auto parameters = old->getAutomatableParameters();
                    const auto previous = std::find_if(parameters.begin(), parameters.end(),
                        [&child](const auto* parameter) { return parameter->getCurve().state == child; });
                    if (previous == parameters.end() || !replacement)
                        return false;
                    auto next = replacement->getAutomatableParameterByID((*previous)->paramID);
                    if (!next || (*previous)->getValueRange() != next->getValueRange())
                        return false;
                    auto curve = child.createCopy();
                    // Tracktion's child-added listener uses the name to bind a replacement curve.
                    curve.setProperty(te::IDs::name, next->paramID, nullptr);
                    state.addChild(curve, -1, nullptr);
                    next->curveHasChanged();
                }
                if (child.hasType(te::IDs::MODIFIERASSIGNMENTS) && child.getNumChildren() > 0)
                {
                    for (auto assignment : child)
                        if (!replacement || !replacement->getAutomatableParameterByID(assignment[te::IDs::paramID].toString()))
                            return false;
                    state.getOrCreateChildWithName(te::IDs::MODIFIERASSIGNMENTS, nullptr)
                        .copyPropertiesAndChildrenFrom(child, nullptr);
                }
            }
            for (auto* track : te::getAllTracks(*edit))
            {
                if (te::EditItemID::fromProperty(track->state, te::IDs::currentAutoParamPluginID) != old->itemID)
                    continue;
                if (!replacement || !replacement->getAutomatableParameterByID(track->state[te::IDs::currentAutoParamTag].toString()))
                    return false;
                item.replacedPlugins.emplace(old->itemID, replacement->itemID);
            }
        }
        return true;
    }

    bool inspectorOutputWouldFeedBack(te::AudioTrack& track) const
    {
        const auto target = inspectorClipboard->output.track;
        if (!target.isValid())
            return false;

        std::map<te::EditItemID, std::vector<te::EditItemID>> inputEdges;
        for (auto device : edit->state.getChildWithName(te::IDs::INPUTDEVICES))
        {
            const auto source = te::EditItemID::fromProperty(device, te::IDs::sourceTrack);
            if (source.isValid())
                for (auto assignment : device)
                    if (assignment.hasType(te::IDs::INPUTDEVICEDESTINATION))
                        inputEdges[source].push_back(te::EditItemID::fromProperty(assignment, te::IDs::targetID));
        }

        std::vector<te::EditItemID> remaining { target };
        std::set<te::EditItemID> seen;
        while (!remaining.empty())
        {
            const auto id = remaining.back();
            remaining.pop_back();
            if (id == track.itemID)
                return true;
            if (!seen.insert(id).second)
                continue;
            if (auto* downstream = trackForID(id))
                if (auto* output = downstream->getOutput().getDestinationTrack())
                    remaining.push_back(output->itemID);
            if (const auto edges = inputEdges.find(id); edges != inputEdges.end())
                remaining.insert(remaining.end(), edges->second.begin(), edges->second.end());
        }
        return false;
    }

    void applyInspectorSettings(te::AudioTrack& track)
    {
        TrackSettingsApply item { &track, &*inspectorClipboard, {}, {} };
        if (inspectorOutputWouldFeedBack(track) || !prepareTrackPlugins(item))
        {
            report("Track settings are incompatible with the selected track");
            return;
        }
        auto& history = edit->getUndoManager();
        history.beginNewTransaction("Apply track settings");
        auto& list = track.pluginList;
        for (auto plugin : list.getPlugins())
            plugin->removeFromParent();
        for (const auto& plugin : item.plugins)
            if (!list.insertPlugin(plugin->state, -1))
            {
                history.undoCurrentTransactionOnly();
                edit->restartPlayback();
                report("Could not apply track settings");
                return;
            }
        for (auto* lane : te::getAllTracks(*edit))
        {
            auto state = lane->state;
            const auto id = te::EditItemID::fromProperty(state, te::IDs::currentAutoParamPluginID);
            if (const auto replacement = item.replacedPlugins.find(id); replacement != item.replacedPlugins.end())
                state.setProperty(te::IDs::currentAutoParamPluginID, replacement->second, &history);
        }
        setTrackOutput(track, item.settings->output);
        edit->restartPlayback();
    }

    void useInspectorSettings(bool apply)
    {
        auto* track = actionTrack();
        if (!editingStopped() || !panels.inspectorOpen || inspectorContext != EditingArea::tracks || !track)
            return;
        actionSelection.requestedArea = EditingArea::inspector;
        if (!apply)
            copyInspectorSettings(*track);
        else if (canApplyInspectorSettings(*track))
            applyInspectorSettings(*track);
        updateControls();
    }

    bool canDispatch(UniversalAction action) const
    {
        using Action = UniversalAction;
        if (!editingStopped())
            return false;
        if (activeEditingArea() == EditingArea::tracks)
        {
            if (action == Action::copy || action == Action::remove)
                return actionTrack() != nullptr;
            if (action == Action::paste)
                return canPasteTrack();
            return (action == Action::up || action == Action::down) && trackMoveNeighbor(action);
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
        if (action == Action::startMinus || action == Action::startPlus
            || action == Action::endMinus || action == Action::endPlus)
            return clips.size() == 1 && !clipChanges(action, clips).empty();
        return !clipChanges(action, clips).empty();
    }

    void deleteSelection(bool tracks, bool cutting)
    {
        auto* project = edit.get();
        const auto area = tracks ? EditingArea::tracks : EditingArea::timeline;
        const auto id = actionSelection.track;
        const auto ids = actionSelection.clips;
        const auto clips = tracks ? std::vector<te::Clip*>{} : selectedClips();
        const bool missing = std::any_of(clips.begin(), clips.end(), [](auto* clip) { return missingWave(clip); });
        if ((tracks || missing) && !ju::AlertWindow::showOkCancelBox(ju::MessageBoxIconType::WarningIcon,
                tracks ? "Delete track?" : "Remove missing audio?",
                tracks ? "Remove the selected track and its contents from the project?"
                       : "The selection includes missing audio. Remove the selected clips from the project?",
                cutting ? "Cut Clips" : "Delete", "Cancel", this))
            return;
        const auto action = cutting ? UniversalAction::cut : UniversalAction::remove;
        if (edit.get() != project || activeEditingArea() != area
            || (tracks ? actionSelection.track != id : actionSelection.clips != ids) || !canDispatch(action))
            return;
        if (cutting && !snapshotClips(selectedClips()))
            return;

        edit->getUndoManager().beginNewTransaction(tracks ? "Delete track" : cutting ? "Cut clips" : "Delete clips");
        if (tracks)
        {
            const auto* primary = trackAt(selection);
            const auto primaryID = primary ? primary->itemID : te::EditItemID{};
            edit->deleteTrack(actionTrack());
            selection = te::getAudioTracks(*edit).indexOf(trackForID(primaryID));
            actionSelection.track = {};
        }
        else
        {
            for (auto clipID : ids)
                te::findClipForID(*edit, clipID)->removeFromParent();
            actionSelection.clips.clear();
        }
        edit->restartPlayback();
    }

    ju::String incompatiblePlacement(UniversalAction action) const
    {
        using Action = UniversalAction;
        ju::String reason;
        if (canEditTimeline())
        {
            if (action == Action::paste)
                pastePlan(&reason);
            if (action == Action::up || action == Action::down)
                clipChanges(action, selectedClips(), &reason);
        }
        return reason;
    }

    void dispatchAction(UniversalAction action)
    {
        using Action = UniversalAction;
        if (!canDispatch(action))
        {
            if (auto reason = incompatiblePlacement(action); reason.isNotEmpty())
                warnPlacement(reason);
            return;
        }
        const bool tracks = activeEditingArea() == EditingArea::tracks;
        auto& history = edit->getUndoManager();
        if (action == Action::remove || action == Action::cut)
            deleteSelection(tracks, action == Action::cut);
        else if (action == Action::copy)
        {
            if (tracks)
                copyTrack(*actionTrack());
            else
                snapshotClips(selectedClips());
        }
        else if (action == Action::paste)
        {
            if (tracks)
                pasteTrack(*actionTrack());
            else
                pasteClips(pastePlan());
        }
        else if (tracks)
        {
            auto* track = actionTrack();
            auto* neighbor = trackMoveNeighbor(action);
            auto parent = track->state.getParent();
            history.beginNewTransaction("Move track");
            parent.moveChild(parent.indexOf(track->state), parent.indexOf(neighbor->state), &history);
            selection = te::getAudioTracks(*edit).indexOf(track);
        }
        else
        {
            const auto changes = clipChanges(action, selectedClips());
            const bool moving = action == Action::up || action == Action::down || action == Action::left || action == Action::right;
            applyClipChanges(changes, moving ? "Move clips" : "Resize clips");
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

    static std::vector<te::Plugin*> audioInserts(te::AudioTrack* track)
    {
        std::vector<te::Plugin*> result;
        if (track)
            for (auto* plugin : track->pluginList)
                if (!plugin->isSynth() && !dynamic_cast<te::VolumeAndPanPlugin*>(plugin)
                    && !dynamic_cast<te::LevelMeterPlugin*>(plugin))
                    result.push_back(plugin);
        return result;
    }

    void editInspectorEffect(int action)
    {
        if (!editingStopped() || inspectorContext != EditingArea::tracks)
            return;
        auto* track = actionTrack();
        if (!track)
            return;
        const auto effects = audioInserts(track);
        auto& undo = edit->getUndoManager();
        if (action == 0)
        {
            if (effects.size() >= 8)
                return report("The track has eight or more audio inserts");
            undo.beginNewTransaction("Add Reverb");
            if (!insertBuiltIn<te::ReverbPlugin>(*track))
                return report("Could not insert Reverb");
        }
        else
        {
            const auto item = std::find_if(effects.begin(), effects.end(), [&](auto* plugin) {
                return plugin->itemID == inspectorEffectID;
            });
            if (item == effects.end())
                return;
            auto* plugin = *item;
            if (action == 1)
            {
                undo.beginNewTransaction("Remove effect");
                plugin->deleteFromParent();
                inspectorEffectID = {};
            }
            else if (action == 2)
            {
                undo.beginNewTransaction("Enable or disable effect");
                plugin->setEnabled(!plugin->isEnabled());
            }
            else
            {
                const auto index = std::distance(effects.begin(), item);
                const auto destination = index + (action == 3 ? -1 : 1);
                if (destination < 0 || destination >= static_cast<decltype(index)>(effects.size()))
                    return;
                auto parent = plugin->state.getParent();
                if (parent != effects[static_cast<size_t>(destination)]->state.getParent())
                    return;
                undo.beginNewTransaction("Reorder effect");
                parent.moveChild(parent.indexOf(plugin->state),
                    parent.indexOf(effects[static_cast<size_t>(destination)]->state), &undo);
            }
        }
        edit->markAsChanged();
        edit->restartPlayback();
        updateControls();
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
        ju::StringArray mixedTracks;
        for (auto* track : te::getAudioTracks(*edit))
        {
            const auto type = trackType(*track);
            track->state.setProperty(trackTypeProperty, trackTypeName(type), nullptr);
            if (type == TrackType::mixed)
                mixedTracks.add(track->getName());
        }
        projectFile = file;
        selection = trackCount() == 0 ? -1 : 0;
        actionSelection = {};
        if (auto* track = trackAt(selection))
            actionSelection.track = track->itemID;
        clipClipboard.reset();
        trackClipboard.reset();
        inspectorClipboard.reset();
        boxAnchor.reset();
        selectionBox = {};
        dragItem = {};
        dragPreview.reset();
        dragError.clear();
        dragRowDelta = 0;
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
        if (!mixedTracks.isEmpty())
            warnPlacement("Mixed legacy tracks contain both Audio and MIDI clips. Existing clips are unchanged. "
                          "New placement is disabled on: " + mixedTracks.joinIntoString(", "));
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
        if (!canAddTrack())
            return report("Track limit reached");
        edit->getUndoManager().beginNewTransaction(midi ? "Add MIDI track" : "Add audio track");
        edit->ensureNumberOfAudioTracks(count + 1);
        if (auto* track = trackAt(count))
        {
            track->state.setProperty(trackTypeProperty, trackTypeName(midi ? TrackType::midi : TrackType::audio),
                                     &edit->getUndoManager());
            track->setName(ju::String(midi ? "MIDI " : "Audio ") + ju::String(count + 1));
            track->setColour(palette(count));
            edit->markAsChanged();
            selectTrack(count);
            revealTrack(count);
        }
    }

    void selectTrack(int index)
    {
        selection = index;
        auto* track = trackAt(index);
        actionSelection.track = track ? track->itemID : te::EditItemID{};
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
            if (const auto error = placementError(TrackType::midi, *track); error.isNotEmpty())
                return warnPlacement(error);
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

    void revealTrack(int row)
    {
        if (row < firstRow)
            scrollRows(row - firstRow);
        else if (row >= firstRow + rowsOnScreen())
            scrollRows(row - firstRow - rowsOnScreen() + 1);
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

    bool trackIsHighlighted(const te::AudioTrack& track) const
    {
        const auto active = activeEditingArea();
        if (active == EditingArea::tracks)
            return actionSelection.track == track.itemID;
        if (active == EditingArea::timeline)
            for (auto* clip : track.getClips())
                if (actionSelection.clips.contains(clip->itemID))
                    return true;
        return false;
    }

    void paintTrack(ju::Graphics& g, te::AudioTrack& track, int row, int y, ju::Colour colour)
    {
        const auto selected = trackIsHighlighted(track);
        auto bounds = trackBounds(y);
        g.setColour(ju::Colour(0xff242424).interpolatedWith(colour, selected ? 0.25f : 0.12f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        if (selected)
        {
            g.setColour(ju::Colours::white);
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
        g.drawFittedText(ju::String(row + 1) + "  [" + trackTypeName(trackType(track)) + "] " + track.getName(),
                         body.reduced(5, 0),
                         ju::Justification::centredLeft, 1);
    }

    void paintAudioEnvelope(ju::Graphics& g, const te::AudioClipBase& audio,
                            te::ClipPosition position, ju::Rectangle<int> content)
    {
        const auto duration = position.getLength().inSeconds();
        const auto gain = ju::Decibels::decibelsToGain(audio.getGainDB());
        const auto fadeIn = audio.getFadeIn().inSeconds();
        const auto fadeOut = audio.getFadeOut().inSeconds();
        const auto start = position.getStart().inSeconds();
        ju::Path envelope;
        for (int x = content.getX(); x < content.getRight(); ++x)
        {
            const auto offset = ju::jlimit(0.0, duration, timeAt(x) - start);
            const auto attack = fadeIn > 0.0 ? std::min(1.0, offset / fadeIn) : 1.0;
            const auto release = fadeOut > 0.0 ? std::min(1.0, (duration - offset) / fadeOut) : 1.0;
            const auto level = std::min(1.0, gain * attack * release * 0.5);
            const auto y = static_cast<float>(content.getBottom() - 1 - level * (content.getHeight() - 2));
            if (x == content.getX())
                envelope.startNewSubPath(static_cast<float>(x), y);
            else
                envelope.lineTo(static_cast<float>(x), y);
        }
        g.setColour(ju::Colours::cyan);
        g.strokePath(envelope, ju::PathStrokeType(1.5f));
    }

    void paintLoopBoundaries(ju::Graphics& g, const te::Clip& clip, te::TimePosition start,
                             ju::Rectangle<int> bounds)
    {
        const auto count = static_cast<int>(clip.state.getProperty(loopCountProperty, 1));
        if (!clip.isLooping() || count <= 1)
            return;

        auto& tempo = edit->tempoSequence;
        const bool beats = clip.beatBasedLooping();
        const auto beatStart = tempo.toBeats(start);
        const auto timeLength = clip.getLoopLength();
        const auto beatLength = clip.getLoopLengthBeats();
        const auto boundaryAt = [&](int index) {
            return beats ? tempo.toTime(beatStart + beatLength * index) : start + timeLength * index;
        };

        // One glyph per visible position at close zoom. Probe pixel columns so very
        // dense loops never require work proportional to the total repeat count.
        g.setColour(ju::Colours::orange);
        g.setFont(ju::FontOptions(13.0f, ju::Font::bold));
        const auto span = [&](int x) {
            const auto time = te::TimePosition::fromSeconds(timeAt(x));
            return beats ? (tempo.toBeats(time) - beatStart).inBeats() / beatLength.inBeats()
                         : (time - start).inSeconds() / timeLength.inSeconds();
        };
        for (int x = bounds.getX(); x < bounds.getRight(); ++x)
        {
            const auto index = std::ceil(span(x));
            if (index >= 1 && index < count && index < span(x + 1))
            {
                const auto at = timeX(boundaryAt(static_cast<int>(index)));
                g.drawText("=", at - 6, bounds.getCentreY() - 8, 12, 16, ju::Justification::centred);
                x += 8;
            }
        }
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
                    if (panels.tallTracks)
                        paintAudioEnvelope(g, *audio, positionInEdit, content);
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
        if (panels.tallTracks)
        {
            paintLoopBoundaries(g, clip, positionInEdit.getStart(), bounds);
            if (clip.state.hasProperty(cloneFamilyProperty) && cloneMembers(clip).size() > 1)
            {
                g.setColour(ju::Colours::orange);
                g.setFont(ju::FontOptions(12.0f, ju::Font::bold));
                g.drawText(ju::String::fromUTF8("↔ ") + clip.state[cloneFamilyProperty].toString(),
                           bounds.getRight() - 43, bounds.getY() + 2, 40, 14, ju::Justification::centredRight);
            }
            if (auto* audio = dynamic_cast<te::AudioClipBase*>(&clip); audio && !missing && !audio->isLooping())
            {
                const auto offset = positionInEdit.getOffset();
                const auto end = (offset + positionInEdit.getLength()) * audio->getSpeedRatio();
                g.setColour(ju::Colours::orange);
                if (offset > te::TimeDuration{} && left >= bounds.getX())
                    g.fillRect(left, bounds.getCentreY() - 3, 6, 6);
                const auto right = timeX(positionInEdit.getEnd());
                if (end.inSeconds() + 1.0e-6 < audio->getSourceLength().inSeconds() && right <= bounds.getRight())
                    g.fillRect(right - 6, bounds.getCentreY() - 3, 6, 6);
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
                paintClip(g, *clip, y + (dragPreview && actionSelection.clips.contains(clip->itemID)
                    ? dragRowDelta * rowHeight() : 0), colour);
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
        if (event.eventComponent == &inspector || event.eventComponent == &inspectorControls
            || inspectorControls.isParentOf(event.eventComponent))
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
        if (dragPreview && actionSelection.clips.contains(clip.itemID))
            if (auto* anchor = te::findClipForID(*edit, dragItem))
                return range + (*dragPreview - anchor->getPosition().getStart());
        return range;
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
        dragError.clear();
        dragRowDelta = 0;
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
                actionSelection.track = track->itemID;
                selection = row;
            }
            else if (!toggle)
            {
                actionSelection.track = {};
                selection = -1;
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
                    // Keep an existing group until a click resolves or a drag starts.
                    const bool draggable = dynamic_cast<te::WaveAudioClip*>(clip) || dynamic_cast<te::MidiClip*>(clip);
                    if (toggle || !draggable || !actionSelection.clips.contains(clip->itemID))
                        ActionSelection::click(actionSelection.clips, clip->itemID, toggle);
                    if (!toggle && draggable)
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

    void updateSelectionBox(ju::Point<int> position)
    {
        const auto bounds = arrangementAreaBounds(EditingArea::timeline).withTrimmedTop(rulerHeight);
        selectionBox = ju::Rectangle<int>(std::min(boxAnchor->x, position.x), std::min(boxAnchor->y, position.y),
            std::abs(boxAnchor->x - position.x) + 1, std::abs(boxAnchor->y - position.y) + 1).getIntersection(bounds);
        auto& items = actionSelection.clips;
        items.clear();
        const auto tracks = te::getAudioTracks(*edit);
        for (int row = firstRow, y = rulerHeight; row < tracks.size() && y < bounds.getBottom(); ++row, y += rowHeight())
            for (auto* clip : tracks[row]->getClips())
                if (clipBounds(clip->getPosition().time, y).intersects(selectionBox))
                    items.insert(clip->itemID);
    }

    void updateDragPreview(double pointerTime, std::optional<int> pointerRow = std::nullopt)
    {
        if (draggingRange)
        {
            const auto a = te::TimePosition::fromSeconds(snapSeconds(dragAnchor));
            const auto b = te::TimePosition::fromSeconds(snapSeconds(pointerTime));
            rangePreview = te::TimeRange(std::min(a, b), std::max(a, b));
        }
        else if (dragItem.isValid())
        {
            dragPreview.reset();
            dragError.clear();
            const auto tracks = te::getAudioTracks(*edit);
            auto* anchor = te::findClipForID(*edit, dragItem);
            if (!anchor || (pointerRow && !tracks[*pointerRow]))
                return;
            auto* source = dynamic_cast<te::AudioTrack*>(anchor->getTrack());
            if (!source)
                return;
            dragRowDelta = pointerRow ? *pointerRow - tracks.indexOf(source) : 0;
            const auto start = anchor->getPosition().getStart();
            const auto target = te::TimePosition::fromSeconds(snapSeconds(start.inSeconds() + pointerTime - dragAnchor));
            if (!movePlan(selectedClips(), dragRowDelta, target - start, &dragError).empty())
                dragPreview = target;
        }
    }

    int dragTargetRow(const ju::MouseEvent& event) const
    {
        if (event.x < headerWidth || event.y < rulerHeight || !arrangement.getLocalBounds().contains(event.getPosition()))
            return -1;
        return firstRow + (event.y - rulerHeight) / rowHeight();
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
        else if (editingStopped() && (dragItem.isValid() ? event.getDistanceFromDragStart() >= 5
                                                       : std::abs(event.getDistanceFromDragStartX()) >= 5))
        {
            updateDragPreview(timeAt(event.x), dragItem.isValid() ? std::optional{ dragTargetRow(event) } : std::nullopt);
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
        else if (dragError.isNotEmpty())
            warnPlacement(dragError);
        else if (dragItem.isValid() && dragPreview)
        {
            if (auto* anchor = te::findClipForID(*edit, dragItem))
            {
                const auto delta = *dragPreview - anchor->getPosition().getStart();
                if (dragRowDelta != 0 || delta != te::TimeDuration{})
                    applyClipChanges(movePlan(selectedClips(), dragRowDelta, delta), "Move clips");
            }
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
        else if (editingStopped() && (dragItem.isValid() ? event.getDistanceFromDragStart() >= 5
                                                        : std::abs(event.getDistanceFromDragStartX()) >= 5))
        {
            updateDragPreview(timeAt(event.x), dragItem.isValid() ? std::optional{ dragTargetRow(event) } : std::nullopt);
            finishDrag();
        }
        else if (dragItem.isValid() && event.getDistanceFromDragStart() < 5)
            actionSelection.clips = { dragItem };
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
        if (edit)
        {
            std::erase_if(actionSelection.clips, [this](auto id) { return !te::findClipForID(*edit, id); });
            if (!actionTrack())
                actionSelection.track = {};
        }
        const auto active = activeEditingArea();
        if (actionSelection.requestedArea && actionSelection.requestedArea != active)
            actionSelection.requestedArea = active;
        if (active != EditingArea::inspector)
            inspectorContext = active;

        ju::String description;
        ju::String inspectorDetails;
        auto* inspectorTrack = inspectorContext == EditingArea::tracks ? actionTrack() : nullptr;
        auto* inspectorClip = inspectorContext == EditingArea::timeline ? selectedClip() : nullptr;
        if (inspectorTrack)
        {
            description = inspectorTrack->getName();
            const auto row = te::getAudioTracks(*edit).indexOf(inspectorTrack) + 1;
            const auto type = trackType(*inspectorTrack);
            inspectorDetails = ju::String(row) + (type == TrackType::audio ? "A" : type == TrackType::midi ? "M" : "")
                + "  " + trackTypeName(type);
        }
        else if (inspectorClip)
        {
            description = inspectorClip->getName();
            const auto position = inspectorClip->getPosition();
            inspectorDetails = dynamic_cast<te::MidiClip*>(inspectorClip) ? "MIDI" : "Audio";
            if (auto* track = inspectorClip->getTrack())
                inspectorDetails += "  " + track->getName();
            if (inspectorClip->state.hasProperty(cloneFamilyProperty) && cloneMembers(*inspectorClip).size() > 1)
                inspectorDetails += ju::String::fromUTF8("  ↔ ") + inspectorClip->state[cloneFamilyProperty].toString()
                    + "." + inspectorClip->state[cloneOrderProperty].toString();
            inspectorDetails += "\nStart " + ju::String(position.getStart().inSeconds(), 3)
                + " s\nLength " + ju::String(position.getLength().inSeconds(), 3) + " s";
        }
        else if (inspectorContext == EditingArea::timeline && !actionSelection.clips.empty())
            inspectorDetails = ju::String(static_cast<int>(actionSelection.clips.size())) + " clips selected";
        inspector.setText(inspectorDetails.isEmpty() ? "INSPECTOR" : "INSPECTOR\n\n" + inspectorDetails,
                          ju::dontSendNotification);
        const bool canEditInspector = editingStopped() && (inspectorTrack || inspectorClip);
        inspectorName.setVisible(inspectorTrack || inspectorClip);
        inspectorName.setEnabled(canEditInspector);
        if (!inspectorName.isBeingEdited())
            inspectorName.setText(description, ju::dontSendNotification);
        for (auto* control : inspectorButtons)
            control->setVisible(inspectorTrack != nullptr);
        for (auto* button : clipInspectorButtons)
        {
            button->setVisible(inspectorClip != nullptr);
            button->setEnabled(canEditInspector);
        }
        if (inspectorClip)
        {
            const auto members = cloneMembers(*inspectorClip);
            static_cast<ju::Button*>(clipInspectorButtons[1])->setButtonText(
                members.size() > 1 && members.front() != inspectorClip ? "UNCLONE" : "CLONE");
        }
        for (auto* control : trackInspectorToggles)
            control->setVisible(inspectorTrack != nullptr);
        inspectorMeter.setVisible(inspectorTrack != nullptr);
        if (inspectorTrack)
        {
            const auto found = meters.find(inspectorTrack->itemID);
            inspectorMeter.level = found != meters.end() ? found->second->level : 0.0f;
            inspectorMeter.repaint();
        }
        auto* volume = inspectorTrack ? inspectorTrack->getVolumePlugin() : nullptr;
        for (auto* control : { &inspectorVolume, &inspectorPan })
            control->setVisible(volume != nullptr);
        if (volume)
        {
            if (!inspectorVolume.isMouseButtonDown())
                inspectorVolume.setValue(volume->getVolumeDb(), ju::dontSendNotification);
            if (!inspectorPan.isMouseButtonDown())
                inspectorPan.setValue(volume->getPan() * 100, ju::dontSendNotification);
        }
        if (inspectorTrack)
        {
            static_cast<ju::Button*>(trackInspectorToggles[0])->setToggleState(inspectorTrack->isMuted(false), ju::dontSendNotification);
            static_cast<ju::Button*>(trackInspectorToggles[1])->setToggleState(inspectorTrack->isSolo(false), ju::dontSendNotification);
        }
        inspectorEffects.setVisible(inspectorTrack != nullptr);
        for (auto* button : effectButtons)
        {
            button->setVisible(inspectorTrack != nullptr);
            button->setEnabled(canEditInspector);
        }
        if (inspectorTrack)
        {
            const auto effects = audioInserts(inspectorTrack);
            const auto previousEffect = inspectorEffectID;
            inspectorEffects.updateContent();
            const auto selected = std::find_if(effects.begin(), effects.end(), [&](auto* plugin) {
                return plugin->itemID == previousEffect;
            });
            if (selected == effects.end())
            {
                inspectorEffects.deselectAllRows();
                inspectorEffectID = {};
            }
            else
                inspectorEffects.selectRow(static_cast<int>(std::distance(effects.begin(), selected)), true);
            effectButtons.front()->setEnabled(canEditInspector && effects.size() < 8);
        }
        inspectorOutput.setVisible(inspectorTrack != nullptr);
        inspectorOutput.setEnabled(canEditInspector);
        if (inspectorTrack)
        {
            const auto route = trackOutput(*inspectorTrack);
            if (!route.track.isValid() && (route.device.isEmpty()
                || route.device == te::DeviceManager::getDefaultAudioOutDeviceName(false)))
                inspectorOutput.setSelectedId(1, ju::dontSendNotification);
            else if (auto* destination = trackForID(route.track))
                inspectorOutput.setText(destination->getName(), ju::dontSendNotification);
            else
                inspectorOutput.setText(route.device, ju::dontSendNotification);
        }
        const auto canLoop = inspectorClip && canControlLoop(*inspectorClip);
        auto* loop = static_cast<ju::Button*>(loopInspectorButtons.front());
        loop->setVisible(canLoop);
        loop->setEnabled(canEditInspector);
        loop->setToggleState(canLoop && inspectorClip->isLooping(), ju::dontSendNotification);
        inspectorLoopCount.setVisible(canLoop);
        inspectorLoopCount.setEnabled(canEditInspector);
        if (canLoop && !inspectorLoopCount.isBeingEdited())
            inspectorLoopCount.setText(ju::String(static_cast<int>(inspectorClip->state.getProperty(loopCountProperty, 1))),
                                       ju::dontSendNotification);
        auto* midi = dynamic_cast<te::MidiClip*>(inspectorClip);
        const auto syncMidi = [&](ju::Slider& slider, const ju::Identifier& property, int defaultValue) {
            slider.setVisible(midi != nullptr);
            slider.setEnabled(canEditInspector);
            if (midi && !slider.isMouseButtonDown())
                slider.setValue(static_cast<int>(midi->state.getProperty(property, defaultValue)), ju::dontSendNotification);
        };
        syncMidi(inspectorVelocity, midiVelocityProperty, 100);
        syncMidi(inspectorTranspose, midiTransposeProperty, 0);
        auto* audio = inspectedAudioClip();
        for (auto* slider : { &inspectorGain, &inspectorFadeIn, &inspectorFadeOut })
            slider->setVisible(audio != nullptr);
        if (audio)
        {
            inspectorGain.setEnabled(canEditInspector);
            if (!inspectorGain.isMouseButtonDown())
                inspectorGain.setValue(audio->getGainDB(), ju::dontSendNotification);
            const auto duration = audio->getPosition().getLength().inSeconds();
            const auto syncFade = [&](ju::Slider& slider, te::TimeDuration fade, te::TimeDuration opposite) {
                slider.setEnabled(canEditInspector && !audio->isLooping());
                if (!slider.isMouseButtonDown())
                {
                    slider.setRange(0, std::max(0.001, duration - opposite.inSeconds()), 0.001);
                    slider.setValue(fade.inSeconds(), ju::dontSendNotification);
                }
            };
            syncFade(inspectorFadeIn, audio->getFadeIn(), audio->getFadeOut());
            syncFade(inspectorFadeOut, audio->getFadeOut(), audio->getFadeIn());
        }
        inspectorButtons[0]->setEnabled(editingStopped() && inspectorTrack);
        inspectorButtons[1]->setEnabled(editingStopped() && inspectorTrack && canApplyInspectorSettings(*inspectorTrack));
        inspector.setActive(edit && active == EditingArea::inspector);
        workspace.setActive(edit && active == EditingArea::window);
        for (const auto& slot : actionButtons)
        {
            const bool enabled = canDispatch(slot.action) || incompatiblePlacement(slot.action).isNotEmpty();
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
    struct InspectorEffectsModel final : ju::ListBoxModel
    {
        explicit InspectorEffectsModel(MainGui& ownerIn) : owner(ownerIn) {}
        int getNumRows() override
        {
            return static_cast<int>(audioInserts(owner.actionTrack()).size());
        }
        void paintListBoxItem(int row, ju::Graphics& g, int width, int height, bool selected) override
        {
            const auto effects = audioInserts(owner.actionTrack());
            if (row < 0 || static_cast<size_t>(row) >= effects.size())
                return;
            auto* plugin = effects[static_cast<size_t>(row)];
            g.fillAll(ju::Colour(0xff245d77));
            g.setColour(plugin->isEnabled() ? ju::Colours::limegreen : ju::Colours::orange);
            g.fillRect(6, height / 2 - 3, 6, 6);
            g.setColour(ju::Colours::white);
            g.drawFittedText(plugin->getName(), 18, 0, width - 24, height, ju::Justification::centredLeft, 1);
            if (selected)
                g.drawRect(0, 0, width, height, 2);
        }
        void selectedRowsChanged(int row) override
        {
            const auto effects = audioInserts(owner.actionTrack());
            if (row >= 0 && static_cast<size_t>(row) < effects.size())
                owner.inspectorEffectID = effects[static_cast<size_t>(row)]->itemID;
        }
        void listBoxItemDoubleClicked(int row, const ju::MouseEvent&) override
        {
            selectedRowsChanged(row);
            owner.report("Effects Editor is pending E1 integration");
        }
        MainGui& owner;
    };
    te::EditItemID inspectorEffectID;
    InspectorEffectsModel inspectorEffectsModel { *this };
    ju::ListBox inspectorEffects;
    std::vector<ju::Component*> effectButtons;
    struct InspectorMeter final : ju::Component
    {
        float level = 0;
        void paint(ju::Graphics& g) override
        {
            g.fillAll(ju::Colour(0xff171717));
            g.setColour(ju::Colours::limegreen);
            g.fillRect(getLocalBounds().withTrimmedTop(ju::roundToInt(getHeight() * (1.0f - level))));
        }
    };
    ju::Component inspectorControls;
    ju::Viewport inspectorViewport;
    InspectorMeter inspectorMeter;
    ju::Label inspectorName, inspectorLoopCount;
    ju::ComboBox inspectorOutput;
    std::vector<ju::Component*> clipInspectorButtons, trackInspectorToggles, loopInspectorButtons;
    ju::Slider inspectorVolume, inspectorPan, inspectorGain, inspectorFadeIn, inspectorFadeOut;
    ju::Slider inspectorVelocity, inspectorTranspose;
    ju::TooltipWindow tooltips { this };
    ju::TextEditor tempo;
    ju::ComboBox signature, recordMode, snap;
    ju::OwnedArray<ju::TextButton> buttons;
    ju::TextButton *playButton = nullptr, *pauseButton = nullptr, *loopButton = nullptr,
        *clickButton = nullptr, *countButton = nullptr, *heightButton = nullptr, *followButton = nullptr;
    ju::TextButton *undoButton = nullptr, *redoButton = nullptr;
    std::vector<ju::Component*> first, second, panelButtons, historyButtons, inspectorButtons, navigation, tabs;
    std::array<ActionGroup, 4> actionGroups;
    std::vector<ActionButton> actionButtons;
    ActionSelection actionSelection;
    EditingArea inspectorContext = EditingArea::timeline;
    std::unique_ptr<te::Clipboard::Clips> clipClipboard;
    std::optional<TrackSnapshot> trackClipboard;
    std::optional<TrackSettings> inspectorClipboard;
    std::map<te::EditItemID, std::unique_ptr<Meter>> meters;
    te::EditItemID dragItem;
    std::optional<ju::Point<int>> boxAnchor;
    ju::Rectangle<int> selectionBox;
    te::TimePosition editCursor;
    std::optional<te::TimeRange> timeSelection, rangePreview;
    std::optional<te::TimePosition> dragPreview;
    double dragAnchor = 0.0;
    int dragRowDelta = 0;
    ju::String dragError;
    int selection = -1, firstRow = 0;
    size_t activeTab = 0;
    double viewStart = 0.0, pixelsPerSecond = 100.0;
    bool follow = true, paused = false, draggingRange = false;
