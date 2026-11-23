// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

bool MainGui::testEffectsEditor(te::Engine& engine)
{
    auto dir = ju::File::getSpecialLocation(ju::File::tempDirectory)
        .getNonexistentChildFile("SimpleDAW-effects-editor", {});
    if (dir.createDirectory().failed())
        return false;
    const ju::ScopeGuard cleanup { [&] { dir.deleteRecursively(); } };
    const auto check = [](bool passed, const char* step) {
        if (!passed)
            std::fprintf(stderr, "Effects Editor: %s\n", step);
        return passed;
    };
    const auto near = [](double a, double b) { return std::abs(a - b) < 0.0001; };
    const auto file = dir.getChildFile("test.tracktionedit");
    MainGui gui(engine, dir.getChildFile("gui.settings"));
    gui.setProject(te::createEmptyEdit(engine, file), file);
    gui.edit->ensureNumberOfAudioTracks(2);
    auto* first = gui.trackAt(0);
    auto* second = gui.trackAt(1);
    const auto firstID = first->itemID;
    const auto secondID = second->itemID;
    if (!check(insertBuiltIn<te::ReverbPlugin>(*first) && insertBuiltIn<te::ReverbPlugin>(*first)
            && insertBuiltIn<te::CompressorPlugin>(*first) && insertBuiltIn<te::ReverbPlugin>(*second),
               "create native fixtures through the project's insertion path"))
        return false;

    auto* compressor = dynamic_cast<te::CompressorPlugin*>(first->pluginList[0]);
    auto* reverb = dynamic_cast<te::ReverbPlugin*>(first->pluginList[1]);
    auto* sibling = dynamic_cast<te::ReverbPlugin*>(first->pluginList[2]);
    auto* other = dynamic_cast<te::ReverbPlugin*>(second->pluginList[0]);
    if (!check(compressor && reverb && sibling && other, "fixture order"))
        return false;

    const auto compressorID = compressor->itemID;
    const auto reverbID = reverb->itemID;
    const auto siblingID = sibling->itemID;
    const auto otherID = other->itemID;
    auto& editor = gui.effectsEditor;
    const auto control = [&editor](const ju::String& id) -> EffectsEditor::Control& {
        return **std::find_if(editor.controls.begin(), editor.controls.end(),
            [id](const auto& candidate) { return id == candidate->definition.id; });
    };
    const auto adjust = [&](const ju::String& id, double value) {
        control(id).slider.setValue(value, ju::sendNotificationSync);
    };

    reverb->setRoomSize(0.25f);
    reverb->setWet(0.2f);
    reverb->setDry(0.75f);
    gui.actionSelection.track = firstID;
    if (!check(!editor.isOpen() && !gui.openEffectsEditor(secondID, reverbID), "initial state and mismatched IDs")
        || !check(gui.openEffectsEditor(firstID, reverbID) && editor.controls.size() == 6,
                  "open native Reverb by identity")
        || !check(near(control("room size").slider.getValue(), 0.25)
            && near(control("wet level").slider.getValue(), 0.2)
            && near(control("dry level").slider.getValue(), 0.75)
            && editor.title.getText().contains(first->getName()), "show saved values and track name")
        || !check(gui.activeEditingArea() == EditingArea::window && !gui.canDispatch(UniversalAction::remove)
            && gui.actionSelection.track == firstID && editor.isVisible()
            && gui.effectsEditorArea.getX() == gui.arrangement.getX() + headerWidth
            && gui.effectsEditorArea.getRight() == gui.inspectorDivider.getX(), "overlay bounds and Universal Actions ownership"))
        return false;

    gui.actionSelection.track = secondID;
    gui.actionSelection.requestedArea = EditingArea::inspector;
    gui.updateControls();
    adjust("room size", 0.8);
    gui.useHistory(false);
    if (!check(near(control("room size").slider.getValue(), 0.25), "Undo refreshes the native slider"))
        return false;
    gui.useHistory(true);
    adjust("wet level", 0.0);
    adjust("dry level", 0.5);
    if (!check(near(reverb->getRoomSize(), 0.8) && near(reverb->getWet(), 0.0) && near(reverb->getDry(), 0.5)
            && near(sibling->getRoomSize(), 0.3) && near(other->getRoomSize(), 0.3),
               "Inspector selection leaves editor bound to the original instance"))
        return false;

    gui.selectTrack(te::getAudioTracks(*gui.edit).indexOf(second));
    if (!check(!editor.isOpen(), "selecting another track closes the editor")
        || !check(gui.openEffectsEditor(firstID, reverbID), "open the previous effect again"))
        return false;
    const auto now = ju::Time::getCurrentTime();
    const auto click = [&](ju::Component& component) {
        return ju::MouseEvent(ju::Desktop::getInstance().getMainMouseSource(), {},
            ju::ModifierKeys::leftButtonModifier, 0, 0, 0, 0, 0, &component, &component,
            now, {}, now, 1, false);
    };
    gui.mouseDown(click(editor.bypass));
    if (!check(editor.isOpen(), "interacting inside the editor keeps it open"))
        return false;
    gui.mouseDown(click(gui.inspectorName));
    if (!check(!editor.isOpen(), "unrelated Inspector control closes the editor")
        || !check(gui.openEffectsEditor(firstID, reverbID), "reopen after an unrelated control"))
        return false;

    auto& chain = first->pluginList;
    chain.state.moveChild(chain.state.indexOf(reverb->state), chain.state.indexOf(sibling->state),
                          &gui.edit->getUndoManager());
    reverb->setEnabled(false);
    gui.updateControls();
    adjust("damping", 0.35);
    adjust("wet level", 0.4);
    adjust("dry level", 0.0);
    if (!check(editor.bypass.getButtonText() == "BYPASSED" && editor.title.getText().contains("Reverb 2")
            && near(reverb->getDamp(), 0.35) && near(reverb->getWet(), 0.4) && near(reverb->getDry(), 0.0)
            && near(sibling->getDamp(), 0.5), "reorder, bypass readout, and native Wet/Dry endpoints"))
        return false;

    reverb->setEnabled(true);
    gui.updateControls();
    if (!check(editor.bypass.getButtonText() == "ACTIVE", "bypass restoration"))
        return false;
    reverb->setEnabled(false);
    if (!check(gui.openEffectsEditor(firstID, compressorID) && editor.controls.size() == 5,
               "rebind to native Compressor using the same panel"))
        return false;

    adjust("threshold", 0.1);
    adjust("ratio", control("ratio").slider.getMaximum());
    adjust("attack", 4);
    adjust("release", 180);
    adjust("output gain", 6);
    if (!check(near(compressor->getThreshold(), 0.1) && near(compressor->getRatio(), 0.0)
            && near(compressor->attackMs.getCurrentValue(), 4)
            && near(compressor->releaseMs.getCurrentValue(), 180)
            && near(compressor->outputDb.getCurrentValue(), 6)
            && control("attack").value.getText() == "4.0 ms", "Compressor adjustment and readout"))
        return false;
    editor.closeButton.onClick();
    if (!check(!editor.isOpen() && !editor.isVisible() && compressor->isEnabled()
            && near(compressor->getThreshold(), 0.1), "close control leaves effect state intact"))
        return false;
    gui.useHistory(false);
    if (!check(near(compressor->outputDb.getCurrentValue(), 0), "Undo updates a closed native effect"))
        return false;
    gui.useHistory(true);
    if (!check(near(compressor->outputDb.getCurrentValue(), 6) && gui.save(),
               "Redo restores the value before normal project save"))
        return false;

    gui.setProject(te::loadEditFromFile(engine, file), file);
    first = gui.trackForID(firstID);
    if (!check(first && first->pluginList[0]->itemID == compressorID
            && first->pluginList[1]->itemID == siblingID && first->pluginList[2]->itemID == reverbID,
               "saved processing order and IDs")
        || !check(gui.openEffectsEditor(firstID, compressorID) && near(control("attack").slider.getValue(), 4)
            && near(control("release").slider.getValue(), 180) && near(control("output gain").slider.getValue(), 6),
                  "reopen saved Compressor values")
        || !check(gui.openEffectsEditor(firstID, reverbID) && editor.bypass.getButtonText() == "BYPASSED"
            && near(control("room size").slider.getValue(), 0.8) && near(control("damping").slider.getValue(), 0.35)
            && near(control("wet level").slider.getValue(), 0.4) && near(control("dry level").slider.getValue(), 0.0),
                  "reopen saved Reverb values and bypass"))
        return false;

    first->pluginList[2]->deleteFromParent();
    adjust("room size", 0.1); // A late slider event cannot write after deletion.
    gui.updateControls();
    if (!check(!editor.isOpen() && !editor.isVisible(), "deleted effect invalidates editor")
        || !check(gui.openEffectsEditor(firstID, siblingID) && near(control("room size").slider.getValue(), 0.3)
            && gui.openEffectsEditor(secondID, otherID) && near(control("room size").slider.getValue(), 0.3),
                  "other instances retain independent saved values"))
        return false;

    gui.edit->deleteTrack(gui.trackForID(secondID));
    gui.updateControls();
    if (!check(!editor.isOpen(), "deleted track invalidates editor")
        || !check(gui.openEffectsEditor(firstID, siblingID), "open surviving plugin")
        || !check(gui.save(), "save with editor open"))
        return false;
    gui.setProject(te::loadEditFromFile(engine, file), file);
    if (!check(!editor.isOpen() && !editor.isVisible(), "project replacement clears binding even with the same IDs"))
        return false;

    auto& manager = engine.getDeviceManager();
    auto& host = manager.getHostedAudioDeviceInterface();
    const te::HostedAudioDeviceInterface::Parameters device;
    host.initialise(device);
    host.prepareToPlay(device.sampleRate, device.blockSize);
    manager.dispatchPendingUpdates();
    ju::MessageManager::getInstance()->runDispatchLoopUntil(100);

    first = gui.trackForID(firstID);
    const auto audio = dir.getChildFile("playback.wav");
    if (!check(first && writeTestWave(audio) && insertAudio(*first, audio), "create hosted playback input")
        || !check(gui.openEffectsEditor(firstID, compressorID), "open Compressor for live adjustments"))
        return false;

    adjust("threshold", 1);
    adjust("output gain", 0);
    auto& transport = gui.edit->getTransport();
    transport.setLoopRange({ {}, te::TimePosition::fromSeconds(0.1) });
    transport.looping = true;
    transport.ensureContextAllocated();
    std::atomic<float> peak { 0.0f }, rightPeak { 0.0f };
    std::jthread processing([&](std::stop_token stop) {
        ju::AudioBuffer<float> buffer(2, device.blockSize);
        ju::MidiBuffer midi;
        while (!stop.stop_requested())
        {
            buffer.clear();
            midi.clear();
            host.processBlock(buffer, midi);
            peak.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
            rightPeak.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
            ju::Thread::sleep(5);
        }
    });
    gui.play();
    const auto waitForPeak = [&](const auto& condition) {
        for (int attempt = 0; attempt < 75; ++attempt)
        {
            ju::MessageManager::getInstance()->runDispatchLoopUntil(20);
            if (condition(peak.load()))
                return true;
        }
        return false;
    };
    float baseline = 0.0f;
    if (!check(transport.isPlaying() && waitForPeak([&baseline](float level) {
            baseline = level;
            return level > 0.05f;
        }), "hosted playback produces audio"))
        return false;
    adjust("output gain", 12.0);
    if (!check(waitForPeak([baseline](float level) { return level > baseline * 2; }),
               "Compressor slider changes hosted playback output"))
        return false;
    editor.bypass.onClick();
    if (!check(editor.bypass.getButtonText() == "BYPASSED"
            && waitForPeak([baseline](float level) { return level > 0.02f && level < baseline * 1.6f; }),
               "live Compressor bypass removes its output gain"))
        return false;
    editor.bypass.onClick();
    if (!check(waitForPeak([baseline](float level) { return level > baseline * 2; }),
               "live Compressor re-enable restores processing")
        || !check(gui.openEffectsEditor(firstID, siblingID), "switch effect while playback continues"))
        return false;
    adjust("wet level", 0.0);
    adjust("dry level", 0.0);
    if (!check(waitForPeak([](float level) { return level < 0.001f; }),
               "Reverb Wet/Dry sliders attenuate hosted playback output"))
        return false;
    editor.bypass.onClick();
    if (!check(editor.bypass.getButtonText() == "BYPASSED"
            && waitForPeak([](float level) { return level > 0.05f; }), "live E1 bypass restores the signal"))
        return false;
    editor.bypass.onClick();
    if (!check(waitForPeak([](float level) { return level < 0.001f; }), "live E1 re-enable restores processing"))
        return false;
    adjust("wet level", 1.0 / 3.0);
    if (!check(waitForPeak([](float level) { return level > 0.01f; }),
               "Reverb produces wet-only output while playing"))
        return false;
    gui.stop();
    if (!check(editor.isOpen() && !transport.isPlaying(), "STOP preserves the effects editor target"))
        return false;
    gui.play();
    gui.closeEffectsEditor();
    if (!check(!editor.isOpen() && transport.isPlaying(), "closing the editor leaves playback running"))
        return false;
    gui.selectTrack(te::getAudioTracks(*gui.edit).indexOf(first));
    auto* fader = first->getVolumePlugin();
    const auto originalVolume = fader->getVolumeDb();
    float trackPeak = 0;
    if (!check(waitForPeak([&trackPeak](float level) { trackPeak = level; return level > 0.05f; }),
               "baseline after closing E1"))
        return false;
    {
        ju::Slider::ScopedDragNotification drag(gui.inspectorVolume);
        gui.inspectorVolume.setValue(-8, ju::sendNotificationSync);
        gui.inspectorVolume.setValue(-20, ju::sendNotificationSync);
        if (!check(gui.inspectorVolume.isEnabled()
                && waitForPeak([trackPeak](float level) { return level > 0.005f && level < trackPeak * 0.45f; }),
                   "track fader changes sound before release"))
            return false;
    }
    gui.stop();
    gui.useHistory(false);
    if (!check(near(fader->getVolumeDb(), originalVolume), "one Undo restores an entire fader gesture"))
        return false;
    gui.play();
    gui.inspectorPan.setValue(-100, ju::sendNotificationSync);
    if (!check(gui.inspectorPan.isEnabled() && waitForPeak([&](float level) {
            return level > 0.05f && rightPeak.load() < 0.001f;
        }), "live track pan changes the stereo output"))
    {
        std::fprintf(stderr, "Pan %.3f left %.6f right %.6f position %.6f playing %d\n",
            fader->getPan(), peak.load(), rightPeak.load(), transport.getPosition().inSeconds(),
            static_cast<int>(transport.isPlaying()));
        return false;
    }
    gui.inspectorPan.setValue(0, ju::sendNotificationSync);
    gui.inspectorEQ[1].setValue(-12, ju::sendNotificationSync);
    float eqPeak = 0;
    if (!check(gui.inspectorEQ[1].isEnabled() && waitForPeak([&](float level) {
            eqPeak = level;
            return level > 0.01f && level < trackPeak * 0.75f;
        }), "live mid EQ cuts the hosted test tone"))
        return false;
    gui.eqBypass.onClick();
    if (!check(waitForPeak([eqPeak](float level) { return level > eqPeak * 1.3f; }),
               "live EQ bypass restores the hosted signal"))
        return false;
    gui.actionSelection.clips = { first->getClips().getFirst()->itemID };
    gui.actionSelection.requestedArea = EditingArea::timeline;
    gui.updateControls();
    gui.inspectorGain.setValue(-24, ju::sendNotificationSync);
    if (!check(gui.inspectorGain.isEnabled() && waitForPeak([trackPeak](float level) {
            return level > 0.002f && level < trackPeak * 0.35f;
        }), "live clip gain attenuates playback"))
        return false;
    gui.clipMute.onClick();
    if (!check(waitForPeak([](float level) { return level < 0.001f; }) && !first->isMuted(false),
               "live audio clip mute leaves the track unmuted"))
        return false;
    gui.stop();
    processing.request_stop();
    processing.join();

    ju::DocumentWindow window("Effects Editor Test", ju::Colours::black, 0);
    window.setContentNonOwned(&gui, false);
    window.centreWithSize(1060, 580);
    window.setVisible(true);
    if (!check(gui.openEffectsEditor(firstID, siblingID), "open the editor over the timeline"))
        return false;
    auto* messages = ju::MessageManager::getInstance();
    messages->runDispatchLoopUntil(300);
    if (!check(editor.getBounds() == gui.effectsEditorArea.getLocalBounds()
            && control("wet level").slider.getHeight() >= 28 && gui.inspector.isVisible(),
               "fixed slide-up layout at minimum window size"))
        return false;
    gui.closeEffectsEditor();
    messages->runDispatchLoopUntil(300);
    return check(!editor.isVisible() && gui.outgoingEffects.getAlpha() == 0.0f
                     && gui.arrangement.getBottom() + 2 == gui.navigation.front()->getY(),
                 "close animation leaves the full-height timeline");
}
