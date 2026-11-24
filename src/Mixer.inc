// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    struct MixerStrip final : ju::Component
    {
        static constexpr int width = 106;
        static constexpr float floorDb = -60.0f, ceilingDb = 6.0f;
        enum Control { mute, solo, arm, monitor };
        enum Kind { track, master, unavailable };

        struct StripSlider final : ju::Slider
        {
            explicit StripSlider(MixerStrip& stripIn) : strip(stripIn) {}

            void paint(ju::Graphics& g) override
            {
                ju::Slider::paint(g);
                if (isVertical() && getHeight() > 18)
                {
                    const auto y = ju::roundToInt(getPositionOfValue(0.0));
                    g.setColour(ju::Colours::lightgrey);
                    g.fillRect(getWidth() - 12, y, 8, 1);
                }
            }

            void mouseDown(const ju::MouseEvent& event) override
            {
                if (event.mods.isPopupMenu())
                    strip.mouseDown(event);
                else
                    ju::Slider::mouseDown(event);
            }

            void mouseDoubleClick(const ju::MouseEvent& event) override
            {
                if (!event.mods.isPopupMenu())
                    ju::Slider::mouseDoubleClick(event);
            }

            MixerStrip& strip;
        };

        struct StripButton final : ju::TextButton
        {
            explicit StripButton(MixerStrip& stripIn) : strip(stripIn) {}

            void mouseDown(const ju::MouseEvent& event) override
            {
                if (event.mods.isPopupMenu())
                    strip.mouseDown(event);
                else
                    ju::TextButton::mouseDown(event);
            }

            void mouseDrag(const ju::MouseEvent& event) override
            {
                if (!event.mods.isPopupMenu())
                    ju::TextButton::mouseDrag(event);
            }

            MixerStrip& strip;
        };

        MixerStrip(MainGui& gui, Kind kindIn, te::EditItemID trackID = {})
            : owner(gui), kind(kindIn), id(trackID)
        {
            setWantsKeyboardFocus(true);
            if (kind == unavailable)
                return;
            for (auto* slider : { &volume, &pan })
            {
                slider->setScrollWheelEnabled(false);
                slider->setPopupMenuEnabled(false);
                slider->setTextBoxStyle(ju::Slider::TextBoxBelow, false, 64, 16);
                slider->setDoubleClickReturnValue(true, 0);
                addAndMakeVisible(*slider);
                owner.configureLiveSlider(*slider);
            }
            volume.setName("Mixer volume");
            volume.setSliderStyle(ju::Slider::LinearVertical);
            volume.setRange(floorDb, ceilingDb, 0.1);
            volume.setSkewFactorFromMidPoint(-12);
            volume.setTextValueSuffix(" dB");
            pan.setName("Mixer pan");
            owner.configureTrackPan(pan);
            pan.setTextBoxStyle(ju::Slider::TextBoxRight, false, 41, 17);

            const auto begin = volume.onDragStart;
            const auto end = volume.onDragEnd;
            volume.onDragStart = [this, begin] {
                begin();
                owner.beginMixerFader(*this);
            };
            volume.onDragEnd = [this, end] {
                owner.mixerFaderStart.clear();
                end();
                owner.updateControls();
            };
            volume.onValueChange = [this] { owner.changeMixerVolume(*this); };
            pan.onValueChange = [this] {
                if (auto plugin = owner.mixerVolume(*this); plugin && owner.mixerLive())
                {
                    owner.beginParameterChange(pan, "Mixer pan");
                    plugin->setPan(static_cast<float>(pan.getValue() / 100));
                    owner.updateControls();
                }
            };

            if (kind == track)
            {
                constexpr const char* names[] { "Mute", "Solo", "Arm", "Monitor" };
                for (size_t i = 0; i < controls.size(); ++i)
                {
                    auto& control = controls[i];
                    control.setButtonText(names[i]);
                    control.setColour(ju::TextButton::buttonOnColourId, ju::Colour(0xff327498));
                    control.onClick = [this, i] {
                        if (auto* target = owner.trackForID(id))
                            owner.changeMixerSwitch(*target, static_cast<Control>(i));
                    };
                    addAndMakeVisible(control);
                }
            }
        }

        void mouseDown(const ju::MouseEvent& event) override
        {
            if (kind == track && (event.mods.isLeftButtonDown() || event.mods.isRightButtonDown()))
            {
                if (isShowing())
                    grabKeyboardFocus();
                owner.selectMixerTrack(id, event.mods.isPopupMenu());
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(5);
            bounds.removeFromTop(34);
            if (kind == track)
            {
                for (size_t i = 0; i < controls.size(); i += 2)
                {
                    auto row = bounds.removeFromBottom(19);
                    controls[i].setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(1));
                    controls[i + 1].setBounds(row.reduced(1));
                }
            }
            pan.setBounds(bounds.removeFromBottom(23));
            meterBounds = bounds.removeFromRight(30).withTrimmedBottom(16).reduced(2, 0);
            volume.setBounds(bounds);
        }

        void paint(ju::Graphics& g) override
        {
            g.fillAll(selected ? ju::Colour(0xff273e50) : ju::Colour(0xff25282c));
            g.setColour(ju::Colour(0xff101113));
            g.fillRect(getWidth() - 1, 0, 1, getHeight());
            g.setColour(colour);
            g.fillRect(0, 0, getWidth(), 4);
            g.setColour(ju::Colours::white);
            g.setFont(14.0f);
            g.drawFittedText(name, 5, 6, getWidth() - 10, 17, ju::Justification::centredLeft, 1);
            g.setFont(11.0f);
            g.setColour(ju::Colours::lightgrey);
            g.drawFittedText(detail, 5, 23, getWidth() - 10, 14, ju::Justification::centredLeft, 1);
            if (kind == unavailable)
                return;
            const auto laneWidth = (meterBounds.getWidth() - 8) / 2;
            for (size_t channel = 0; channel < levels.size(); ++channel)
            {
                auto lane = meterBounds.withWidth(laneWidth).translated(static_cast<int>(channel) * (laneWidth + 2), 0);
                g.setColour(ju::Colour(0xff101113));
                g.fillRect(lane);
                g.setColour(levels[channel] > 0.95f ? ju::Colours::orange : ju::Colour(0xff68c598));
                const auto label = lane.withY(lane.getBottom()).withHeight(14);
                g.fillRect(lane.removeFromBottom(ju::roundToInt(lane.getHeight() * levels[channel])));
                g.setColour(ju::Colours::lightgrey);
                g.drawFittedText(channel == 0 ? "L" : "R", label, ju::Justification::centred, 1);
            }
        }

        MainGui& owner;
        Kind kind;
        te::EditItemID id;
        StripSlider volume { *this }, pan { *this };
        std::array<StripButton, 4> controls { StripButton(*this), StripButton(*this),
                                             StripButton(*this), StripButton(*this) };
        ju::String name, detail;
        ju::Colour colour { 0xff666b73 };
        ju::Rectangle<int> meterBounds;
        std::array<float, 2> levels {};
        bool selected = false;
    };

    static bool isDrumMaster(const te::AudioTrack& track)
    {
        return static_cast<bool>(track.state.getProperty("simpleDawDrumMaster", false));
    }

    te::VolumeAndPanPlugin::Ptr mixerVolume(const MixerStrip& strip) const
    {
        if (!edit)
            return {};
        if (strip.kind == MixerStrip::master)
            return edit->getMasterVolumePlugin();
        if (auto* track = trackForID(strip.id))
            return track->getVolumePlugin();
        return {};
    }

    bool mixerLive() const { return edit && !edit->getTransport().isRecording(); }

    void selectMixerTrack(te::EditItemID id, bool toggle)
    {
        ActionSelection::click(mixerSelection, id, toggle);
        updateControls();
    }

    void beginMixerFader(MixerStrip& strip)
    {
        mixerFaderStart.clear();
        if (strip.kind != MixerStrip::track)
            return;
        if (!mixerSelection.contains(strip.id))
            selectMixerTrack(strip.id, false);
        for (auto id : mixerSelection)
            if (auto* track = trackForID(id))
                if (auto* plugin = track->getVolumePlugin())
                {
                    const auto db = plugin->getVolumeDb();
                    if (db >= MixerStrip::floorDb - 0.001f && db <= MixerStrip::ceilingDb + 0.001f)
                        mixerFaderStart.emplace(id, ju::jlimit(MixerStrip::floorDb, MixerStrip::ceilingDb, db));
                }
    }

    void changeMixerVolume(MixerStrip& strip)
    {
        auto plugin = mixerVolume(strip);
        if (!plugin || !mixerLive())
            return;
        beginParameterChange(strip.volume, "Mixer volume");
        const auto requested = static_cast<float>(strip.volume.getValue());
        const auto leader = mixerFaderStart.find(strip.id);
        if (parameterGesture == &strip.volume && leader != mixerFaderStart.end())
        {
            auto [low, high] = std::minmax_element(mixerFaderStart.begin(), mixerFaderStart.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            const auto delta = ju::jlimit(MixerStrip::floorDb - low->second, MixerStrip::ceilingDb - high->second,
                                          requested - leader->second);
            strip.volume.setValue(leader->second + delta, ju::dontSendNotification);
            for (const auto& [id, start] : mixerFaderStart)
                if (auto* track = trackForID(id))
                    if (auto* volume = track->getVolumePlugin())
                        volume->setVolumeDb(ju::jlimit(MixerStrip::floorDb, MixerStrip::ceilingDb, start + delta));
        }
        else
            plugin->setVolumeDb(requested);
        updateControls();
    }

    void changeMixerSwitch(te::AudioTrack& track, MixerStrip::Control control)
    {
        switch (control)
        {
            case MixerStrip::mute: toggleTrackMute(track); break;
            case MixerStrip::solo: toggleTrackSolo(track); break;
            case MixerStrip::arm: armTrackInput(track); break;
            case MixerStrip::monitor: toggleTrackMonitor(track); break;
        }
    }

    void setMixerOpen(bool open)
    {
        if (panels.mixerOpen == open)
            return;
        panels.mixerOpen = open;
        if (open)
        {
            editorAnimator.cancelAnimation(&effectsEditor, false);
            editorAnimator.cancelAnimation(&pianoRoll, false);
            slideEditorOut({});
        }
        resized();
        if (open && mixerPanel.isShowing())
            mixerPanel.grabKeyboardFocus();
        updateControls();
        panels.save();
    }

    void layoutMixer()
    {
        auto bounds = mixerPanel.getLocalBounds();
        auto header = bounds.removeFromTop(25);
        mixerHeight.setBounds(header.removeFromRight(64).reduced(2));
        for (auto* pinned : { &mixerMaster, &mixerFX2, &mixerFX1 })
            pinned->setBounds(bounds.removeFromRight(MixerStrip::width).withTrimmedBottom(mixerViewport.getScrollBarThickness()));
        mixerViewport.setBounds(bounds);
        mixerTracks.setSize(static_cast<int>(mixerStrips.size()) * MixerStrip::width,
                            ju::jmax(0, bounds.getHeight() - mixerViewport.getScrollBarThickness()));
        int x = 0;
        for (auto& strip : mixerStrips)
        {
            strip->setBounds(x, 0, MixerStrip::width, mixerTracks.getHeight());
            x += MixerStrip::width;
        }
    }

    void refreshMixer()
    {
        if (!panels.mixerOpen || !edit)
            return;
        auto tracks = te::getAudioTracks(*edit);
        // MD0's track list presents the enabled Drum Master before ordinary tracks.
        const auto drum = std::find_if(tracks.begin(), tracks.end(), [](auto* track) { return isDrumMaster(*track); });
        if (drum != tracks.end())
            std::rotate(tracks.begin(), drum, drum + 1);
        size_t index = 0;
        bool changed = false;
        ItemSelection present;
        for (auto* track : tracks)
        {
            // MD0 uses internal audio tracks as lane sources.
            if (te::EditItemID::fromProperty(track->state, "simpleDawDrumSource").isValid()
                || (isDrumMaster(*track) && !track->isProcessing(false)))
                continue;
            present.insert(track->itemID);
            auto found = std::find_if(mixerStrips.begin() + static_cast<std::ptrdiff_t>(index), mixerStrips.end(),
                                      [track](const auto& strip) { return strip->id == track->itemID; });
            if (found == mixerStrips.end())
            {
                auto strip = std::make_unique<MixerStrip>(*this, MixerStrip::track, track->itemID);
                mixerTracks.addAndMakeVisible(*strip);
                mixerStrips.insert(mixerStrips.begin() + static_cast<std::ptrdiff_t>(index), std::move(strip));
                changed = true;
            }
            else if (found != mixerStrips.begin() + static_cast<std::ptrdiff_t>(index))
            {
                std::rotate(mixerStrips.begin() + static_cast<std::ptrdiff_t>(index), found, found + 1);
                changed = true;
            }

            auto& strip = *mixerStrips[index++];
            strip.name = track->getName();
            strip.detail = isDrumMaster(*track) ? "Drum Master  >" : trackTypeName(trackType(*track));
            strip.setTitle(strip.name);
            strip.setDescription(isDrumMaster(*track) ? "Drum lane expansion is unavailable" : strip.detail);
            strip.colour = trackColour(*track);
            strip.selected = mixerSelection.contains(track->itemID);
            if (const auto meter = meters.find(track->itemID); meter != meters.end())
                strip.levels = meter->second->levels;
            else
                strip.levels = {};
            const auto input = trackInputState(*track);
            const bool values[] { track->isMuted(false), track->isSolo(false), input.armed, input.monitoring };
            for (size_t i = 0; i < strip.controls.size(); ++i)
            {
                strip.controls[i].setToggleState(values[i], ju::dontSendNotification);
                strip.controls[i].setEnabled(i < MixerStrip::arm ? mixerLive() : editingStopped() && input.assigned);
            }
            refreshMixerStrip(strip);
        }
        std::erase_if(mixerSelection, [&present](auto id) { return !present.contains(id); });
        if (index != mixerStrips.size())
        {
            if (parameterGesture && std::any_of(mixerStrips.begin() + static_cast<std::ptrdiff_t>(index), mixerStrips.end(),
                [this](const auto& strip) { return parameterGesture == &strip->volume || parameterGesture == &strip->pan; }))
            {
                parameterGesture = nullptr;
                mixerFaderStart.clear();
                edit->getUndoManager().beginNewTransaction();
            }
            mixerStrips.resize(index);
            changed = true;
        }
        if (changed)
            layoutMixer();

        mixerMaster.levels = masterMeter ? masterMeter->levels : std::array<float, 2> {};
        refreshMixerStrip(mixerMaster);
    }

    void refreshMixerStrip(MixerStrip& strip)
    {
        const auto plugin = mixerVolume(strip);
        strip.volume.setEnabled(plugin && mixerLive());
        strip.pan.setEnabled(plugin && mixerLive());
        if (plugin)
        {
            const auto* editor = dynamic_cast<ju::TextEditor*>(ju::Component::getCurrentlyFocusedComponent());
            if (parameterGesture != &strip.volume && !strip.volume.isMouseButtonDown() && (!editor || !strip.volume.isParentOf(editor)))
                strip.volume.setValue(plugin->getVolumeDb(), ju::dontSendNotification);
            if (parameterGesture != &strip.pan && !strip.pan.isMouseButtonDown() && (!editor || !strip.pan.isParentOf(editor)))
                strip.pan.setValue(plugin->getPan() * 100, ju::dontSendNotification);
        }
        strip.repaint();
    }

    struct MixerViewport final : ju::Viewport
    {
        void mouseWheelMove(const ju::MouseEvent&, const ju::MouseWheelDetails& wheel) override
        {
            const auto delta = wheel.deltaX == 0 ? wheel.deltaY : wheel.deltaX;
            setViewPosition(getViewPositionX() - ju::roundToInt(delta * 240), 0);
        }
    };

    ju::Component mixerPanel, mixerTracks;
    MixerViewport mixerViewport;
    ju::TextButton mixerHeight;
    MixerStrip mixerFX1 { *this, MixerStrip::unavailable }, mixerFX2 { *this, MixerStrip::unavailable },
               mixerMaster { *this, MixerStrip::master };
    std::vector<std::unique_ptr<MixerStrip>> mixerStrips;
    ItemSelection mixerSelection;
    std::map<te::EditItemID, float> mixerFaderStart;
    std::unique_ptr<Meter> masterMeter;
