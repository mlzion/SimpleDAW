// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    struct DrumCell
    {
        DrumStep step;
        ju::Rectangle<int> bounds;
    };

    struct DrumEditorSurface final : ju::Component
    {
        explicit DrumEditorSurface(MainGui& gui) : owner(gui)
        {
            setWantsKeyboardFocus(true);
            addAndMakeVisible(close);
            addAndMakeVisible(velocity);
            close.setTooltip("Close Drum Pattern Editor");
            close.onClick = [this] { owner.closeDrumEditor(); };
            velocity.setJustificationType(ju::Justification::centredRight);
            velocity.setInterceptsMouseClicks(false, false);
        }

        void resized() override
        {
            close.setBounds(getWidth() - 36, 3, 32, 26);
            velocity.setBounds(getWidth() / 2, 2, getWidth() / 2 - 42, 28);
        }

        void paint(ju::Graphics& g) override { owner.paintDrumEditor(g); }
        void mouseDown(const ju::MouseEvent& event) override
        {
            owner.focusDrumEditor();
            if (event.mods.isLeftButtonDown() || event.mods.isRightButtonDown())
                owner.drumMouseDown(event);
        }
        void mouseDrag(const ju::MouseEvent& event) override
        {
            if (event.getDistanceFromDragStart() >= 5)
                owner.drumDrag(event.getPosition());
        }
        void mouseUp(const ju::MouseEvent& event) override { owner.drumMouseUp(event); }
        void mouseWheelMove(const ju::MouseEvent&, const ju::MouseWheelDetails& wheel) override
        {
            owner.focusDrumEditor();
            // Precision wheels can report both axes for an ordinary vertical
            // gesture. Give the dominant axis to the intended operation.
            if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
                owner.scrollDrumTime(-wheel.deltaX);
            else if (wheel.deltaY != 0.0f && !owner.adjustDrumVelocity(wheel.deltaY))
                owner.scrollDrumRows(wheel.deltaY > 0 ? -1 : 1);
        }
        bool keyPressed(const ju::KeyPress& key) override
        {
            if (key != ju::KeyPress::escapeKey)
                return false;
            if (owner.drumPopupStep)
                owner.closeDrumPopup();
            else
                owner.closeDrumEditor();
            return true;
        }

        MainGui& owner;
        ju::TextButton close { "X" };
        ju::Label velocity;
    };

    static constexpr int drumNameWidth = 136, drumHeaderHeight = 52, drumRowHeight = 44;

    te::MidiClip* selectedDrumPattern() const
    {
        return edit && drumPatternID.isValid()
            ? dynamic_cast<te::MidiClip*>(te::findClipForID(*edit, drumPatternID)) : nullptr;
    }

    void focusDrumEditor()
    {
        actionSelection.requestedArea = EditingArea::window;
        if (drumEditor.isShowing())
            drumEditor.grabKeyboardFocus();
        updateControls();
    }

    void openDrumEditor(te::MidiClip& clip)
    {
        if (effectsEditor.isVisible())
            suspendEffectsEditor();
        hidePianoRoll();
        slideEditorOut({});
        const bool entering = !drumEditor.isVisible();
        if (entering)
        {
            drumPreviousViewStart = viewStart;
            drumPreviousTimelineScale = pixelsPerSecond;
        }
        hideDrumEditor();
        drumPatternID = clip.itemID;
        actionSelection.clips = { clip.itemID };
        inspectorContext = EditingArea::timeline;
        drumFirstLane = 0;
        drumEditor.setVisible(true);
        drumEditor.toFront(false);
        resized();
        fitDrumViewport(clip.getPosition().getStart().inSeconds());
        focusDrumEditor();
        if (entering)
            slideEditorIn(drumEditor);
    }

    void hideDrumEditor()
    {
        if (!drumPatternID.isValid())
            return;
        editorAnimator.cancelAnimation(&drumEditor, false);
        drumEditor.setVisible(false);
        drumPatternID = {};
        drumSteps.clear();
        drumPendingClicks.clear();
        drumAnchor.reset();
        drumSelectionBox = {};
        closeDrumPopup();
        if (actionSelection.requestedArea == EditingArea::window)
            actionSelection.requestedArea = EditingArea::timeline;
    }

    void closeDrumEditor()
    {
        const auto id = drumPatternID;
        const auto outgoing = drumEditor.createComponentSnapshot(drumEditor.getLocalBounds());
        hideDrumEditor();
        viewStart = drumPreviousViewStart;
        pixelsPerSecond = drumPreviousTimelineScale;
        actionSelection.requestedArea = EditingArea::timeline;
        if (edit)
            if (auto* clip = te::findClipForID(*edit, id))
            {
                actionSelection.clips = { id };
                selection = visibleTracks().indexOf(dynamic_cast<te::AudioTrack*>(clip->getTrack()));
            }
        if (arrangement.isShowing())
            arrangement.grabKeyboardFocus();
        slideEditorOut(outgoing);
        updateControls();
    }

    MidiNoteEditor makeDrumEditor(te::MidiClip& clip) const
    {
        std::vector<te::MidiClip*> linked;
        for (auto* member : cloneMembers(clip))
            if (member != &clip)
                if (auto* midi = dynamic_cast<te::MidiClip*>(member))
                    linked.push_back(midi);
        return MidiNoteEditor(clip, std::move(linked));
    }

    bool drumGridHasFocus() const
    {
        return drumEditor.isVisible() && activeEditingArea() == EditingArea::window && selectedDrumPattern();
    }

    void closeDrumPopup()
    {
        drumPopup.setVisible(false);
        drumPopupStep.reset();
    }

    void refreshDrumGrid()
    {
        if (!drumPatternID.isValid())
            return;
        auto* clip = selectedDrumPattern();
        if (!clip || !clip->getTrack() || !drumMaster::isMaster(*clip->getTrack()) || !clip->getTrack()->isProcessing(false))
        {
            hideDrumEditor();
            return;
        }
        if (clip->getPosition().time != drumViewportClipRange)
            fitDrumViewport(drumViewStart);
        if (!editingStopped())
        {
            closeDrumPopup();
            drumPendingClicks.clear();
            drumAnchor.reset();
            drumSelectionBox = {};
        }
        else if (drumPopupStep)
            refreshDrumPopup();

        ju::String feedback;
        if (!drumSteps.empty())
        {
            auto editor = makeDrumEditor(*clip);
            const DrumPattern pattern(*clip, editor);
            int lowest = 128, highest = 0;
            for (auto step : drumSteps)
                for (const auto& hit : pattern.hits(step))
                {
                    lowest = std::min(lowest, hit.velocity);
                    highest = std::max(highest, hit.velocity);
                }
            if (highest > 0)
                feedback = lowest == highest ? "Velocity " + ju::String(highest)
                    : "Velocity " + ju::String(lowest) + " - " + ju::String(highest);
        }
        drumEditor.velocity.setText(feedback, ju::dontSendNotification);
        drumEditor.repaint();
    }

    void fitDrumViewport(double seconds)
    {
        auto* clip = selectedDrumPattern();
        if (!clip)
            return;
        const auto& tempo = edit->tempoSequence;
        drumViewportClipRange = clip->getPosition().time;
        const auto start = DrumPattern::quarterAtTime(tempo, drumViewportClipRange.getStart());
        const auto end = DrumPattern::quarterAtTime(tempo, drumViewportClipRange.getEnd());
        drumVisibleQuarters = std::min(end - start, DrumPattern::step * DrumPattern::maxVisibleSteps);
        if (drumVisibleQuarters <= 0.0)
            return;
        const auto requested = DrumPattern::quarterAtTime(tempo, te::TimePosition::fromSeconds(seconds));
        const auto maxSteps = std::ceil((end - start - drumVisibleQuarters) / DrumPattern::step - 1.0e-8);
        const auto steps = std::clamp(std::round((requested - start) / DrumPattern::step), 0.0, std::max(0.0, maxSteps));
        drumLeftQuarter = start + steps * DrumPattern::step;
        drumViewStart = DrumPattern::timeAtQuarter(tempo, drumLeftQuarter).inSeconds();
        drumPixelsPerSecond = ju::jmax(1, drumEditor.getWidth() - drumNameWidth)
            / std::max(0.000001, DrumPattern::timeAtQuarter(tempo, drumLeftQuarter + drumVisibleQuarters).inSeconds() - drumViewStart);
        closeDrumPopup();
        drumEditor.repaint();
    }

    void scrollDrumTime(double pages)
    {
        if (selectedDrumPattern())
        {
            const auto delta = pages * drumVisibleQuarters;
            const auto step = std::copysign(std::max(DrumPattern::step, std::abs(delta)), delta);
            fitDrumViewport(DrumPattern::timeAtQuarter(edit->tempoSequence, std::max(0.0, drumLeftQuarter + step)).inSeconds());
        }
    }

    ju::Array<te::AudioTrack*> drumEditorLanes() const
    {
        auto* clip = selectedDrumPattern();
        auto* track = clip ? dynamic_cast<te::AudioTrack*>(clip->getTrack()) : nullptr;
        return track ? drumMaster::lanes(*track) : ju::Array<te::AudioTrack*>{};
    }

    void scrollDrumRows(int delta)
    {
        const int visible = ju::jmax(1, (drumEditor.getHeight() - drumHeaderHeight) / drumRowHeight);
        drumFirstLane = ju::jlimit(0, ju::jmax(0, drumEditorLanes().size() - visible), drumFirstLane + delta);
        closeDrumPopup();
        drumEditor.repaint();
    }

    int drumX(te::TimePosition time) const
    {
        return drumNameWidth + ju::roundToInt((time.inSeconds() - drumViewStart) * drumPixelsPerSecond);
    }

    double drumTimeAt(int x) const { return drumViewStart + (x - drumNameWidth) / drumPixelsPerSecond; }

    template<class Visit>
    void forDrumCells(te::MidiClip& clip, const DrumPattern& pattern, te::AudioTrack& lane,
                      int y, int height, Visit visit) const
    {
        auto* sampler = drumMaster::sampler(lane);
        if (!sampler || pattern.count() == 0)
            return;
        const auto settings = sampler->state.getChildWithName(drumMaster::laneType);
        const auto note = static_cast<int>(settings.getProperty(drumMaster::noteProperty, -1));
        if (note < 0)
            return;
        const auto& tempo = edit->tempoSequence;
        const auto leftX = drumX(clip.getPosition().getStart());
        const auto rightX = drumX(clip.getPosition().getEnd());
        const auto bounds = ju::Rectangle<int>(leftX, y, rightX - leftX, height)
            .getIntersection({ drumNameWidth, y, drumEditor.getWidth() - drumNameWidth, height });
        if (bounds.isEmpty())
            return;
        const auto origin = clip.getContentStartBeat().inBeats();
        const auto sourceStart = pattern.range.getStart();
        const auto period = pattern.range.getLength();
        const auto isLoop = clip.isLooping() && period > 0.0;
        const auto left = tempo.toBeats(te::TimePosition::fromSeconds(drumTimeAt(bounds.getX()))).inBeats();
        const auto right = tempo.toBeats(te::TimePosition::fromSeconds(drumTimeAt(bounds.getRight()))).inBeats();
        const auto firstLoop = isLoop ? std::max(0.0, std::floor((left - origin) / period)) : 0.0;
        const auto lastLoop = isLoop ? std::max(firstLoop, std::floor((right - origin) / period)) : 0.0;
        for (auto repeat = firstLoop; repeat <= lastLoop; ++repeat)
        {
            const auto absoluteStart = origin + (isLoop ? repeat * period : sourceStart);
            const int first = ju::jmax(0, pattern.indexAt(sourceStart + left - absoluteStart));
            const int last = ju::jmin(pattern.count() - 1, pattern.indexAt(sourceStart + right - absoluteStart));
            const auto x = [&](double beat) { return drumX(tempo.toTime(te::BeatPosition::fromBeats(beat))); };
            for (int i = first; i <= last; ++i)
            {
                const DrumStep step { note, i };
                const auto a = x(absoluteStart + pattern.start(step) - sourceStart);
                const auto b = x(absoluteStart + std::min(pattern.range.getEnd(), pattern.start(step, 4)) - sourceStart);
                const auto cell = ju::Rectangle<int>(a, y, b - a, height).getIntersection(bounds);
                if (!cell.isEmpty())
                    visit(DrumCell { step, cell });
            }
        }
    }

    void paintDrumGrid(ju::Graphics& g, te::MidiClip& clip, const DrumPattern& pattern,
                       te::AudioTrack& track, int y, int height, ju::Colour colour)
    {
        forDrumCells(clip, pattern, track, y, height, [&](const DrumCell& cell) {
            const auto& hits = pattern.hits(cell.step);
            const auto selected = drumSteps.contains(cell.step);
            g.setColour(ju::Colour(0xff202329).interpolatedWith(colour,
                hits.empty() ? 0.10f : 0.30f + hits.front().velocity / 255.0f));
            g.fillRect(cell.bounds.reduced(1));
            if (selected)
            {
                g.setColour(ju::Colours::white);
                g.drawRect(cell.bounds.reduced(1), 2);
            }
            if (!hits.empty() && cell.bounds.getWidth() >= 12)
            {
                std::array<bool, 4> slots {};
                for (const auto& hit : hits)
                    slots[pattern.slot(cell.step, hit.start)] = true;
                for (int i = 0; i < 4; ++i)
                {
                    g.setColour(ju::Colours::white.withAlpha(slots[static_cast<size_t>(i)] ? 0.95f : 0.23f));
                    const auto dot = cell.bounds.getX() + (i + 1) * cell.bounds.getWidth() / 5;
                    g.fillEllipse(static_cast<float>(dot - 1), static_cast<float>(cell.bounds.getBottom() - 8), 3.0f, 3.0f);
                }
            }
        });
    }

    void paintDrumEditor(ju::Graphics& g)
    {
        g.fillAll(ju::Colour(0xff23262a));
        auto* clip = selectedDrumPattern();
        if (!clip)
            return;
        g.setColour(ju::Colours::white);
        g.setFont(ju::FontOptions(16.0f, ju::Font::bold));
        g.drawFittedText("Drum Pattern Editor / " + clip->getName(),
            { 8, 0, drumEditor.getWidth() / 2 - 10, 32 }, ju::Justification::centredLeft, 1);
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        const auto lanes = drumEditorLanes();
        const ju::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(drumEditor.getLocalBounds().withTrimmedTop(32));
        // The editor's ruler uses the same time scale as the cells.
        const auto& tempo = edit->tempoSequence;
        const double start = DrumPattern::quarterAtTime(tempo, clip->getPosition().getStart());
        int step = ju::roundToInt((drumLeftQuarter - start) / DrumPattern::step);
        for (double beat = drumLeftQuarter; beat < drumLeftQuarter + drumVisibleQuarters - 1.0e-8;
             beat += DrumPattern::step, ++step)
        {
            const auto x = drumX(DrumPattern::timeAtQuarter(tempo, beat));
            const auto next = drumX(DrumPattern::timeAtQuarter(tempo, beat + DrumPattern::step));
            g.setFont(ju::FontOptions(11.0f));
            g.setColour(ju::Colours::lightgrey);
            g.drawText(ju::String(step + 1), x, 32, next - x, 20, ju::Justification::centred);
        }
        for (int i = drumFirstLane, y = drumHeaderHeight; i < lanes.size() && y < drumEditor.getHeight(); ++i, y += drumRowHeight)
        {
            g.setColour(ju::Colours::white);
            g.setFont(ju::FontOptions(14.0f));
            g.drawFittedText(lanes[i]->getName(), { 8, y, drumNameWidth - 12, drumRowHeight },
                             ju::Justification::centredLeft, 1);
            paintDrumGrid(g, *clip, pattern, *lanes[i], y, drumRowHeight, trackColour(*lanes[i]));
        }
        if (!drumSelectionBox.isEmpty())
        {
            g.setColour(ju::Colours::white.withAlpha(0.2f));
            g.fillRect(drumSelectionBox);
            g.setColour(ju::Colours::white);
            g.drawRect(drumSelectionBox);
        }
        g.setColour(drumGridHasFocus() ? ju::Colours::white : ju::Colours::grey);
        g.drawRect(drumEditor.getLocalBounds().withTrimmedTop(32), 1);
    }

    std::optional<DrumCell> drumCellAt(ju::Point<int> point)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || point.x < drumNameWidth || point.y < drumHeaderHeight)
            return {};
        const auto row = (point.y - drumHeaderHeight) / drumRowHeight;
        auto* lane = drumEditorLanes()[drumFirstLane + row];
        if (!lane)
            return {};
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        std::optional<DrumCell> result;
        forDrumCells(*clip, pattern, *lane, drumHeaderHeight + row * drumRowHeight, drumRowHeight,
                     [&](const DrumCell& cell) {
            if (cell.bounds.contains(point))
                result = cell;
        });
        return result;
    }

    void refreshDrumPopup()
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !drumPopupStep)
            return closeDrumPopup();
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        const auto& hits = pattern.hits(*drumPopupStep);
        if (!hits.empty())
            drumPopupVelocity = hits.front().velocity;
        for (size_t i = 0; i < drumSubdivisionButtons.size(); ++i)
        {
            const auto on = std::any_of(hits.begin(), hits.end(), [&](const auto& hit) {
                return pattern.slot(*drumPopupStep, hit.start) == static_cast<int>(i);
            });
            drumSubdivisionButtons[i].setToggleState(on, ju::dontSendNotification);
            drumSubdivisionButtons[i].setEnabled(pattern.start(*drumPopupStep, static_cast<int>(i))
                + MidiNoteEditor::minimumLength <= pattern.range.getEnd() + 1.0e-8);
        }
    }

    void openDrumPopup(const DrumCell& cell)
    {
        auto* clip = selectedDrumPattern();
        if (!clip)
            return;
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        if (pattern.hits(cell.step).empty())
            return;
        drumPopupStep = cell.step;
        drumPopup.setBounds(ju::jlimit(drumNameWidth, ju::jmax(drumNameWidth, drumEditor.getWidth() - 112), cell.bounds.getX()),
                            ju::jmax(0, cell.bounds.getY() - 30), 112, 30);
        drumPopup.setVisible(true);
        drumPopup.toFront(false);
        refreshDrumPopup();
    }

    void drumMouseDown(const ju::MouseEvent& event)
    {
        closeDrumPopup();
        const auto cell = editingStopped() && !event.mods.isShiftDown() ? drumCellAt(event.getPosition()) : std::nullopt;
        if (!cell)
            return;
        auto* clip = selectedDrumPattern();
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        if (event.mods.isRightButtonDown())
        {
            if (!pattern.hits(cell->step).empty() && drumSteps.erase(cell->step) == 0)
                drumSteps.insert(cell->step);
        }
        else if (event.getNumberOfClicks() > 1)
        {
            drumPendingClicks.erase(cell->step);
            openDrumPopup(*cell);
        }
        else
            drumAnchor = event.getPosition();
        updateControls();
    }

    void drumDrag(ju::Point<int> point)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !drumAnchor)
            return;
        drumSelectionBox = ju::Rectangle<int>(*drumAnchor, point).expanded(1)
            .getIntersection(drumEditor.getLocalBounds().withTrimmedLeft(drumNameWidth).withTrimmedTop(drumHeaderHeight));
        drumSteps.clear();
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        const auto tracks = drumEditorLanes();
        for (int row = drumFirstLane, y = drumHeaderHeight; row < tracks.size() && y < drumEditor.getHeight(); y += drumRowHeight, ++row)
                forDrumCells(*clip, pattern, *tracks[row], y, drumRowHeight, [&](const DrumCell& cell) {
                    if (cell.bounds.intersects(drumSelectionBox) && !pattern.hits(cell.step).empty())
                        drumSteps.insert(cell.step);
                });
        drumEditor.repaint();
    }

    void clickDrumStep(DrumStep step)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !editingStopped())
            return;
        auto editor = makeDrumEditor(*clip);
        DrumPattern pattern(*clip, editor);
        if (pattern.hits(step).empty())
        {
            pattern.toggle(step);
            drumSteps.erase(step);
            return;
        }

        // An occupied cell can also start a double-click. Delay deletion
        // until that gesture has had time to open the subdivision popup.
        const auto patternID = drumPatternID;
        const auto sequence = ++drumClickSequence;
        drumPendingClicks[step] = sequence;
        ju::Timer::callAfterDelay(ju::MouseEvent::getDoubleClickTimeout() + 10,
            [safe = ju::Component::SafePointer<MainGui>(this), step, patternID, sequence] {
                if (!safe || safe->drumPatternID != patternID)
                    return;
                const auto pending = safe->drumPendingClicks.find(step);
                if (pending == safe->drumPendingClicks.end() || pending->second != sequence)
                    return;
                safe->drumPendingClicks.erase(pending);
                if (auto* target = safe->selectedDrumPattern(); target && safe->editingStopped())
                {
                    auto editor = safe->makeDrumEditor(*target);
                    DrumPattern pattern(*target, editor);
                    if (!pattern.hits(step).empty() && pattern.toggle(step))
                        safe->drumSteps.erase(step);
                    safe->updateControls();
                }
            });
    }

    void drumMouseUp(const ju::MouseEvent& event)
    {
        if (!drumAnchor)
            return;
        const auto anchor = *drumAnchor;
        if (event.getDistanceFromDragStart() >= 5 || !drumSelectionBox.isEmpty())
            drumDrag(event.getPosition());
        else if (auto cell = drumCellAt(anchor))
            clickDrumStep(cell->step);
        drumAnchor.reset();
        drumSelectionBox = {};
        updateControls();
    }

    bool adjustDrumVelocity(float delta)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !editingStopped() || drumSteps.empty() || delta == 0.0f
            || drumEditor.velocity.getText().isEmpty())
            return false;

        // Normalize precision-wheel input and detents to a useful MIDI change.
        const auto amount = ju::jlimit(4, 16, ju::roundToInt(std::abs(delta) * 12.0f));
        auto editor = makeDrumEditor(*clip);
        DrumPattern(*clip, editor).velocity(drumSteps, delta > 0 ? amount : -amount);
        updateControls();
        return true;
    }
