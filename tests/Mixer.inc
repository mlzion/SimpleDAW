// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    static bool testMixer(te::Engine& engine)
    {
        const auto directory = ju::File::getSpecialLocation(ju::File::tempDirectory)
            .getNonexistentChildFile("SimpleDAW-mixer", {});
        if (directory.createDirectory().failed())
            return false;
        const ju::ScopeGuard cleanup { [&] { directory.deleteRecursively(); } };
        const auto check = [](bool result, const char* step) {
            if (!result)
                std::fprintf(stderr, "Mixer: %s\n", step);
            return result;
        };
        const auto closeTo = [](double a, double b) { return std::abs(a - b) < 0.02; };

        auto& manager = engine.getDeviceManager();
        auto& host = manager.getHostedAudioDeviceInterface();
        const te::HostedAudioDeviceInterface::Parameters parameters;
        host.initialise(parameters);
        host.prepareToPlay(parameters.sampleRate, parameters.blockSize);
        manager.dispatchPendingUpdates();
        for (int i = 0; i < manager.getNumWaveInDevices(); ++i)
            if (auto* wave = manager.getWaveInDevice(i))
                wave->setEnabled(true);
        manager.deviceManager.dispatchPendingMessages();
        manager.dispatchPendingUpdates();
        ju::MessageManager::getInstance()->runDispatchLoopUntil(100);

        const auto path = directory.getChildFile("mixer.tracktionedit");
        const auto preferences = directory.getChildFile("mixer.settings");
        MainGui gui(engine, preferences);
        gui.setProject(te::createEmptyEdit(engine, path), path);
        gui.edit->ensureNumberOfAudioTracks(maxTracks);
        const auto tracks = te::getAudioTracks(*gui.edit);
        auto& first = *tracks[0];
        auto& second = *tracks[1];
        auto& drum = *tracks[2];
        const auto firstID = first.itemID;
        const auto secondID = second.itemID;
        const auto drumID = drum.itemID;
        drum.state.setProperty("simpleDawDrumMaster", true, nullptr);
        tracks[3]->state.setProperty("simpleDawDrumSource", drumID.toVar(), nullptr);
        drum.setName("Drum Master");
        gui.selectTrack(0);

        auto clip = te::insertMIDIClip(first, "Workspace fixture",
            { te::TimePosition::fromSeconds(1.0), te::TimePosition::fromSeconds(2.0) });
        if (!clip)
            return false;
        gui.pianoRollClip = clip->itemID;
        gui.pianoRoll.setVisible(true);
        gui.focusPianoRoll();
        const auto requestedArea = gui.actionSelection.requestedArea;
        gui.setMixerOpen(true);
        if (!check(gui.mixerPanel.isVisible() && !gui.inspector.isVisible() && !gui.editingPanelArea.isVisible()
                   && gui.pianoRoll.isVisible() && gui.pianoRoll.clip.get() == clip.get()
                   && gui.mixerPanel.getWidth() == gui.arrangement.getWidth()
                   && gui.mixerPanel.getY() == gui.arrangement.getBottom(), "full-width dock suspends the Piano Roll"))
            return false;
        if (!check(gui.mixerStrips.size() == maxTracks - 1 && gui.mixerStrips[0]->id == drumID
                   && gui.mixerStrips[0]->detail.startsWith("Drum Master")
                   && gui.mixerFX1.kind == MixerStrip::unavailable && gui.mixerFX2.kind == MixerStrip::unavailable,
                   "track order, hidden internal drum lane and unavailable effects returns"))
            return false;

        auto& leader = *gui.mixerStrips[1];
        auto& peer = *gui.mixerStrips[2];
        const auto compactMeterHeight = leader.meterBounds.getHeight();
        const auto fixedMaster = gui.mixerMaster.getBounds();
        const auto fixedFX1 = gui.mixerFX1.getBounds();
        ju::MouseWheelDetails wheel {};
        wheel.deltaY = -1;
        const auto now = ju::Time::getCurrentTime();
        const auto mouse = [&](int buttons) {
            return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), {}, ju::ModifierKeys(buttons),
                0, 0, 0, 0, 0, &leader, &leader, now, {}, now, 1, false);
        };
        const auto initialVolume = leader.volume.getValue();
        leader.volume.mouseWheelMove(mouse(0), wheel);
        if (!check(gui.mixerViewport.getViewPositionX() > 0 && gui.mixerViewport.getHorizontalScrollBar().isVisible()
                   && gui.mixerMaster.getBounds() == fixedMaster && gui.mixerFX1.getBounds() == fixedFX1
                   && leader.volume.getValue() == initialVolume, "wheel over fader scrolls while pinned strips stay put"))
            return false;
        gui.mixerHeight.onClick();
        if (!check(leader.meterBounds.getHeight() > compactMeterHeight && leader.getWidth() == MixerStrip::width,
                   "MAX extends meters at the same width"))
            return false;

        first.getVolumePlugin()->setVolumeDb(-12);
        second.getVolumePlugin()->setVolumeDb(-3);
        gui.updateControls();
        leader.mouseDown(mouse(ju::ModifierKeys::leftButtonModifier));
        peer.volume.mouseDown(mouse(ju::ModifierKeys::rightButtonModifier));
        if (!check(gui.mixerSelection == ItemSelection { firstID, secondID } && gui.actionSelection.track == firstID,
                   "right-click toggles mixer selection independently"))
            return false;
        leader.volume.onDragStart();
        leader.volume.setValue(-3, ju::sendNotificationSync);
        leader.volume.setValue(1, ju::sendNotificationSync);
        leader.volume.onDragEnd();
        const auto db = [](te::AudioTrack& track) { return track.getVolumePlugin()->getVolumeDb(); };
        if (!check(closeTo(db(first), -3) && closeTo(db(second), 6), "relative movement preserves offsets at the shared limit"))
            return false;
        gui.useHistory(false);
        const bool undone = closeTo(db(first), -12) && closeTo(db(second), -3);
        gui.useHistory(true);
        if (!check(undone && closeTo(db(first), -3) && closeTo(db(second), 6),
                   "one Undo and Redo restore the entire fader gesture"))
            return false;

        gui.setMixerOpen(false);
        if (!check(gui.inspector.isVisible() && gui.editingPanelArea.isVisible()
                   && gui.actionSelection.requestedArea == requestedArea && gui.pianoRoll.clip.get() == clip.get(),
                   "closing the mixer restores editor focus, binding and Inspector visibility"))
            return false;
        gui.selectTrack(0);
        gui.inspectorVolume.setValue(-8, ju::sendNotificationSync);
        gui.inspectorPan.setValue(-40, ju::sendNotificationSync);
        gui.setMixerOpen(true);
        if (!check(closeTo(leader.volume.getValue(), -8) && closeTo(leader.pan.getValue(), -40),
                   "mixer reads Inspector volume and pan"))
            return false;
        leader.pan.setValue(25, ju::sendNotificationSync);
        leader.controls[MixerStrip::mute].onClick();
        leader.controls[MixerStrip::solo].onClick();
        gui.setMixerOpen(false);
        if (!check(closeTo(gui.inspectorPan.getValue(), 25) && first.isMuted(false) && first.isSolo(false),
                   "Inspector reads mixer pan and switches"))
            return false;

        gui.editInspectorEffect(0);
        const auto inserts = audioInserts(&first);
        if (!check(inserts.size() == 1 && gui.openEffectsEditor(firstID, inserts.front()->itemID),
                   "E1 editor opens before workspace switch"))
            return false;
        const auto target = gui.effectsEditor.target;
        gui.setMixerOpen(true);
        if (!check(!gui.editingPanelArea.isVisible() && gui.effectsEditor.isOpen(), "mixer suspends E1"))
            return false;
        gui.setMixerOpen(false);
        if (!check(gui.editingPanelArea.isVisible() && gui.effectsEditor.target && target
                   && gui.effectsEditor.target->plugin == target->plugin && gui.activeEditingArea() == EditingArea::window,
                   "closing mixer restores the E1 target and workspace"))
            return false;
        gui.closeEffectsEditor();
        gui.selectTrack(0);

        auto& history = gui.edit->getUndoManager();
        auto& transport = gui.edit->getTransport();
        transport.ensureContextAllocated();
        auto inputs = gui.edit->getAllInputDevices();
        inputs.removeIf([](auto* input) { return input->getInputDevice().getDeviceType() != te::InputDevice::waveDevice; });
        if (!check(!inputs.isEmpty(), "hosted wave input available"))
            return false;
        auto& input = *inputs.getFirst();
        input.getInputDevice().setMonitorMode(te::InputDevice::MonitorMode::automatic);
        if (!check(toggleInput(input, first).wasOk() && toggleInput(input, second).wasOk(), "shared input assignment"))
            return false;
        gui.setMixerOpen(true);
        gui.updateControls();
        leader.controls[MixerStrip::monitor].onClick();
        if (!check(!input.isLivePlayEnabled(first) && input.isLivePlayEnabled(second)
                   && input.isRecordingEnabled(firstID), "explicit monitor isolates one destination and preserves arm"))
            return false;
        if (!check(history.undo() && input.isLivePlayEnabled(first) && input.isLivePlayEnabled(second)
                   && history.redo() && !input.isLivePlayEnabled(first), "monitor override supports Undo and Redo"))
            return false;
        leader.controls[MixerStrip::arm].onClick();
        if (!check(!input.isRecordingEnabled(firstID) && input.isRecordingEnabled(secondID)
                   && history.undo() && input.isRecordingEnabled(firstID), "arm changes only one destination and supports Undo"))
            return false;
        gui.setMixerOpen(false);
        if (!check(!static_cast<ju::Button*>(gui.trackInspectorToggles[2])->getToggleState(),
                   "Inspector reflects the mixer monitor setting"))
            return false;

        te::LevelMeasurer measurer;
        Meter meter(measurer);
        ju::AudioBuffer<float> block(2, 128);
        block.clear();
        std::fill_n(block.getWritePointer(0), block.getNumSamples(), 0.5f);
        measurer.processBuffer(block, 0, block.getNumSamples());
        meter.poll();
        if (!check(meter.levels[0] > 0.5f && meter.levels[1] == 0 && meter.level == meter.levels[0],
                   "stereo meter keeps left and right peaks independent"))
            return false;

        gui.setMixerOpen(true);
        const auto originalMasterDb = gui.edit->getMasterVolumePlugin()->getVolumeDb();
        gui.mixerMaster.volume.setValue(-5, ju::sendNotificationSync);
        gui.useHistory(false);
        const bool masterUndone = closeTo(gui.edit->getMasterVolumePlugin()->getVolumeDb(), originalMasterDb);
        gui.useHistory(true);
        if (!check(masterUndone && closeTo(gui.edit->getMasterVolumePlugin()->getVolumeDb(), -5) && gui.save(),
                   "Master fader writes the native edit and saves"))
            return false;
        auto reopened = te::loadEditFromFile(engine, path);
        if (!check(reopened && closeTo(reopened->getMasterVolumePlugin()->getVolumeDb(), -5), "Master gain survives reopen"))
            return false;
        clip = nullptr;
        gui.setProject(std::move(reopened), path);
        if (!check(gui.mixerSelection.empty() && gui.mixerStrips[1]->id == firstID
                   && closeTo(gui.mixerStrips[1]->pan.getValue(), 25)
                   && gui.panels.tallMixer && gui.panels.mixerOpen,
                   "project replacement clears transient selection and binds restored parameters"))
            return false;
        gui.edit->getTransport().ensureContextAllocated();
        gui.updateControls();
        if (!check(gui.mixerStrips[1]->controls[MixerStrip::arm].getToggleState()
                   && !gui.mixerStrips[1]->controls[MixerStrip::monitor].getToggleState()
                   && gui.mixerStrips[2]->controls[MixerStrip::monitor].getToggleState(),
                   "independent arm and monitor survive reopening"))
            return false;
        gui.selectMixerTrack(secondID, false);
        gui.edit->deleteTrack(gui.trackForID(secondID));
        gui.updateControls();
        if (!check(gui.mixerSelection.empty() && gui.mixerStrips.size() == maxTracks - 2,
                   "track deletion removes the strip and its selection"))
            return false;
        return true;
    }
