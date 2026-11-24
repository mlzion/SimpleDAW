// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    int drumFxBus(const ju::String& name) const
    {
        for (auto* plugin : te::getAllPlugins(*edit, false))
            if (auto* receiver = dynamic_cast<te::AuxReturnPlugin*>(plugin);
                receiver && edit->getAuxBusName(receiver->busNumber.get()).equalsIgnoreCase(name))
                return receiver->busNumber.get();
        return -1;
    }

    void setDrumSend(te::AudioTrack& master, int bus, const ju::String& name)
    {
        if (bus < 0)
            return;
        te::AuxSendPlugin* send = nullptr;
        for (auto* plugin : master.pluginList)
            if (auto* candidate = dynamic_cast<te::AuxSendPlugin*>(plugin); candidate && candidate->getBusNumber() == bus)
                send = candidate;
        ju::AlertWindow dialog(name + " send", "Enter a level from -100 dB (off) to +6 dB.",
                               ju::MessageBoxIconType::NoIcon);
        dialog.addTextEditor("gain", ju::String(send ? send->getGainDb() : -100.0f, 1));
        dialog.addButton("Apply", 1);
        dialog.addButton("Cancel", 0, ju::KeyPress(ju::KeyPress::escapeKey));
        if (dialog.runModalLoop() != 1)
            return;
        const auto text = dialog.getTextEditorContents("gain").trim();
        if (text.isEmpty() || !text.containsOnly("-+.0123456789")
            || !std::isfinite(text.getFloatValue()) || text.getFloatValue() < -100.0f || text.getFloatValue() > 6.0f)
            return report("Enter a send level from -100 to +6 dB");
        edit->getUndoManager().beginNewTransaction("Set " + name + " send");
        if (!send)
        {
            auto state = te::AuxSendPlugin::create();
            auto plugin = master.pluginList.insertPlugin(state, master.pluginList.size());
            send = dynamic_cast<te::AuxSendPlugin*>(plugin.get());
            if (!send)
                return;
            send->busNumber = bus;
        }
        send->setGainDb(text.getFloatValue());
        edit->restartPlayback();
    }

    void showDrumMasterControls(te::AudioTrack& master)
    {
        const auto collapsed = static_cast<bool>(master.state[drumMaster::collapsedProperty]);
        ju::PopupMenu menu;
        menu.addItem(1, "Add lane", drumMaster::lanes(master).size() < drumMaster::maxLanes);
        menu.addItem(2, collapsed ? "Show lanes" : "Collapse lanes");
        const auto fx1 = drumFxBus("FX1");
        const auto fx2 = drumFxBus("FX2");
        menu.addItem(3, fx1 >= 0 ? "FX1 send" : "FX1 send (no return)", fx1 >= 0);
        menu.addItem(4, fx2 >= 0 ? "FX2 send" : "FX2 send (no return)", fx2 >= 0);
        const auto choice = menu.show();
        if (!choice || !editingStopped())
            return;
        auto& undo = edit->getUndoManager();
        if (choice == 1)
        {
            undo.beginNewTransaction("Add drum lane");
            if (auto* lane = drumMaster::addLane(master, "Drum " + ju::String(drumMaster::lanes(master).size() + 1), -1))
            {
                master.state.setProperty(drumMaster::collapsedProperty, false, &undo);
                selectTrack(visibleTracks().indexOf(lane));
                revealTrack(selection);
                edit->restartPlayback();
            }
        }
        else if (choice == 2)
        {
            undo.beginNewTransaction(collapsed ? "Show drum lanes" : "Collapse drum lanes");
            master.state.setProperty(drumMaster::collapsedProperty, !collapsed, &undo);
            edit->markAsChanged();
            selection = visibleTracks().indexOf(&master);
            revealTrack(selection);
        }
        else if (choice == 3 || choice == 4)
            setDrumSend(master, choice == 3 ? fx1 : fx2, choice == 3 ? "FX1" : "FX2");
        updateControls();
    }

    bool canDuplicateDrumLane(const te::AudioTrack& track) const
    {
        if (!edit || !drumMaster::isLane(track))
            return false;
        auto* master = drumMaster::find(*edit);
        return master && drumMaster::lanes(*master).size() < drumMaster::maxLanes;
    }

    void duplicateDrumLane(te::AudioTrack& track)
    {
        using namespace drumMaster;
        if (!editingStopped() || !canDuplicateDrumLane(track))
            return;
        auto* master = drumMaster::find(*edit);
        auto& history = edit->getUndoManager();
        track.flushStateToValueTree();
        auto copy = track.state.createCopy();
        te::EditItemID::remapIDs(copy, nullptr, *edit);
        copy.setProperty(sourceProperty, master->itemID, nullptr);
        const auto base = track.getName() + " Copy";
        auto name = base;
        const auto children = lanes(*master);
        for (int suffix = 2; std::any_of(children.begin(), children.end(), [&](auto* child) {
                 return child->getName().equalsIgnoreCase(name);
             }); ++suffix)
            name = base + " " + ju::String(suffix);
        copy.setProperty(te::IDs::name, name, nullptr);
        const auto id = te::EditItemID::fromID(copy);
        auto parent = track.state.getParent();
        history.beginNewTransaction("Duplicate drum lane");
        parent.addChild(copy, parent.indexOf(track.state) + 1, &history);
        selectTrack(visibleTracks().indexOf(trackForID(id)));
        revealTrack(selection);
        edit->restartPlayback();
    }

    void showDrumLaneControls(te::AudioTrack& track, drumMaster::Sampler& sampler)
    {
        using namespace drumMaster;
        auto lane = sampler.state.getChildWithName(laneType);
        ju::PopupMenu menu;
        const auto file = sampler.resolveSample(lane);
        const auto reference = lane[sampleProperty].toString();
        menu.addSectionHeader(reference.isEmpty() ? ju::String("No sample")
            : file.existsAsFile() ? file.getFileName() : "Missing sample: " + reference);
        menu.addItem(1, "GM note: " + (static_cast<int>(lane[noteProperty]) < 0 ? ju::String("Unassigned")
            : lane[noteProperty].toString()));
        menu.addItem(2, "Overlap", true, static_cast<bool>(lane[overlapProperty]));
        menu.addItem(3, "Audition", file.existsAsFile());
        menu.addItem(4, "Locate WAV");
        menu.addItem(5, "Duplicate lane", canDuplicateDrumLane(track));

        drumLibrary = scanLibrary();
        const auto& files = drumLibrary;
        ju::StringArray categories;
        for (const auto& sample : files)
            categories.addIfNotAlreadyThere(sampleCategory(sample.getFileName()));
        categories.sort(true);
        for (const auto& category : categories)
        {
            ju::PopupMenu choices;
            for (int i = 0; i < files.size(); ++i)
                if (sampleCategory(files[i].getFileName()) == category)
                    choices.addItem(100 + i, files[i].getFileName(), true, file == files[i]);
            menu.addSubMenu(category, choices);
        }

        const int choice = menu.show();
        if (!choice || !editingStopped())
            return;
        auto& history = edit->getUndoManager();
        if (choice == 1)
        {
            ju::AlertWindow dialog("GM drum note", "Enter a note from 35 to 81, or -1 to leave the lane unassigned.",
                                   ju::MessageBoxIconType::NoIcon);
            dialog.addTextEditor("note", lane[noteProperty].toString());
            dialog.addButton("Apply", 1);
            dialog.addButton("Cancel", 0, ju::KeyPress(ju::KeyPress::escapeKey));
            if (dialog.runModalLoop() != 1)
                return;
            const auto text = dialog.getTextEditorContents("note").trim();
            const auto note = text.getIntValue();
            if (text != "-1" && (!text.containsOnly("0123456789") || note < 35 || note > 81))
                return report("Enter a GM drum note from 35 to 81, or -1");
            history.beginNewTransaction("Map drum note");
            lane.setProperty(noteProperty, note, &history);
        }
        else if (choice == 2)
        {
            history.beginNewTransaction("Set drum overlap");
            lane.setProperty(overlapProperty, !static_cast<bool>(lane[overlapProperty]), &history);
        }
        else if (choice == 3)
        {
            edit->getTransport().ensureContextAllocated();
            sampler.audition();
        }
        else if (choice == 4)
        {
            const auto trackID = track.itemID;
            chooseFile("Locate drum sample", "*.wav", ju::FileBrowserComponent::openMode
                    | ju::FileBrowserComponent::canSelectFiles, [this, trackID, lane](const ju::File& selected) mutable {
                auto* current = trackForID(trackID);
                auto* instrument = current ? drumMaster::sampler(*current) : nullptr;
                if (editingStopped() && instrument && lane.getParent() == instrument->state)
                {
                    auto& undo = edit->getUndoManager();
                    undo.beginNewTransaction("Locate drum sample");
                    lane.setProperty(drumMaster::sampleProperty,
                        te::SourceFileReference::findPathFromFile(*edit, selected, true), &undo);
                }
            });
        }
        else if (choice == 5)
            duplicateDrumLane(track);
        else if (ju::isPositiveAndBelow(choice - 100, files.size()))
        {
            history.beginNewTransaction("Select drum sample");
            lane.setProperty(sampleProperty, "library:" + files[choice - 100].getFileName(), &history);
        }
        updateControls();
    }

    void showDrumLanes()
    {
        auto* track = actionTrack();
        if (!track || !editingStopped())
            return;
        if (drumMaster::isMaster(*track))
            showDrumMasterControls(*track);
        else if (auto* sampler = drumMaster::sampler(*track))
            showDrumLaneControls(*track, *sampler);
    }
