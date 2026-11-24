// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    struct DrumCell
    {
        DrumStep step;
        ju::Rectangle<int> bounds;
    };

    te::MidiClip* selectedDrumPattern() const
    {
        auto* clip = dynamic_cast<te::MidiClip*>(selectedClip());
        auto* track = clip ? clip->getTrack() : nullptr;
        if (track && drumMaster::isMaster(*track) && track->isProcessing(false)
            && !static_cast<bool>(track->state[drumMaster::collapsedProperty]))
            return clip;
        return nullptr;
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
        return drumGridFocused && activeEditingArea() == EditingArea::timeline && selectedDrumPattern();
    }

    void closeDrumPopup()
    {
        drumPopup.setVisible(false);
        drumPopupStep.reset();
    }

    void refreshDrumGrid()
    {
        auto* clip = selectedDrumPattern();
        const auto id = clip ? clip->itemID : te::EditItemID{};
        if (dragItem.isValid() && id != drumPatternID)
            return;
        if (id == drumPatternID)
        {
            if (clip && clip->getPosition().time != drumViewportClipRange)
                setViewStart(viewStart);
            if (!editingStopped())
            {
                closeDrumPopup();
                drumPendingClicks.clear();
            }
            else if (drumPopupStep)
                refreshDrumPopup();
            return;
        }
        const bool wasEditing = drumPatternID.isValid();
        drumPatternID = id;
        drumSteps.clear();
        drumAnchor.reset();
        drumSelectionBox = {};
        drumGridFocused = false;
        drumPendingClicks.clear();
        closeDrumPopup();
        if (clip)
        {
            if (!wasEditing)
                ordinaryTimelineScale = pixelsPerSecond;
            setViewStart(clip->getPosition().getStart().inSeconds());
        }
        else if (wasEditing)
            pixelsPerSecond = ordinaryTimelineScale;
    }

    // Keep the existing ruler and clips on the same time transform as the step grid.
    // Derive the visible seconds from the tempo map at the current scroll position.
    bool fitDrumViewport(double seconds)
    {
        auto* clip = drumPatternID.isValid() ? selectedDrumPattern() : nullptr;
        if (!clip)
            return false;
        const auto& tempo = edit->tempoSequence;
        const auto range = clip->getPosition().time;
        drumViewportClipRange = range;
        const auto start = DrumPattern::quarterAtTime(tempo, range.getStart());
        const auto end = DrumPattern::quarterAtTime(tempo, range.getEnd());
        const auto span = std::min(end - start, DrumPattern::step * DrumPattern::maxVisibleSteps);
        if (span <= 0.0)
            return false;
        const auto requested = DrumPattern::quarterAtTime(tempo, te::TimePosition::fromSeconds(std::clamp(seconds, 0.0, totalSeconds())));
        // Keep the step phase consistent while scrolling through or beyond the clip.
        const auto left = std::max(0.0, start + std::round((requested - start) / DrumPattern::step) * DrumPattern::step);
        viewStart = DrumPattern::timeAtQuarter(tempo, left).inSeconds();
        const auto width = ju::jmax(1, arrangement.getWidth() - headerWidth);
        pixelsPerSecond = width / std::max(0.000001, DrumPattern::timeAtQuarter(tempo, left + span).inSeconds() - viewStart);
        closeDrumPopup();
        return true;
    }

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
        const auto bounds = clipBounds(clip.getPosition().time, y, height);
        if (bounds.isEmpty())
            return;
        const auto origin = clip.getContentStartBeat().inBeats();
        const auto sourceStart = pattern.range.getStart();
        const auto period = pattern.range.getLength();
        const auto isLoop = clip.isLooping() && period > 0.0;
        const auto left = tempo.toBeats(te::TimePosition::fromSeconds(timeAt(bounds.getX()))).inBeats();
        const auto right = tempo.toBeats(te::TimePosition::fromSeconds(timeAt(bounds.getRight()))).inBeats();
        const auto firstLoop = isLoop ? std::max(0.0, std::floor((left - origin) / period)) : 0.0;
        const auto lastLoop = isLoop ? std::max(firstLoop, std::floor((right - origin) / period)) : 0.0;
        for (auto repeat = firstLoop; repeat <= lastLoop; ++repeat)
        {
            const auto absoluteStart = origin + (isLoop ? repeat * period : sourceStart);
            const int first = ju::jmax(0, pattern.indexAt(sourceStart + left - absoluteStart));
            const int last = ju::jmin(pattern.count() - 1, pattern.indexAt(sourceStart + right - absoluteStart));
            const auto x = [&](double beat) { return timeX(tempo.toTime(te::BeatPosition::fromBeats(beat))); };
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

    std::optional<DrumCell> drumCellAt(ju::Point<int> point)
    {
        auto* clip = selectedDrumPattern();
        const int row = rowAt(point.y);
        auto* track = trackAt(row);
        if (!clip || !track || !drumMaster::isLane(*track) || point.x < headerWidth || point.y < rulerHeight)
            return {};
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        std::optional<DrumCell> result;
        forDrumCells(*clip, pattern, *track, rowTop(row), rowHeight(row), [&](const DrumCell& cell) {
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
        drumPopup.setBounds(ju::jlimit(headerWidth, ju::jmax(headerWidth, arrangement.getWidth() - 112), cell.bounds.getX()),
                            ju::jmax(0, cell.bounds.getY() - 30), 112, 30);
        drumPopup.setVisible(true);
        drumPopup.toFront(false);
        refreshDrumPopup();
    }

    bool drumMouseDown(const ju::MouseEvent& event)
    {
        closeDrumPopup();
        const auto cell = editingStopped() && !event.mods.isShiftDown() ? drumCellAt(event.getPosition()) : std::nullopt;
        if (!cell)
        {
            drumGridFocused = false;
            return false;
        }
        drumGridFocused = true;
        actionSelection.requestedArea = EditingArea::timeline;
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
        return true;
    }

    void drumDrag(ju::Point<int> point)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !drumAnchor)
            return;
        drumSelectionBox = ju::Rectangle<int>(*drumAnchor, point).expanded(1)
            .getIntersection(arrangementAreaBounds(EditingArea::timeline).withTrimmedTop(rulerHeight));
        drumSteps.clear();
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        const auto tracks = visibleTracks();
        for (int row = firstRow, y = rulerHeight; row < tracks.size() && y < arrangement.getHeight(); y += rowHeight(row), ++row)
            if (drumMaster::isLane(*tracks[row]))
                forDrumCells(*clip, pattern, *tracks[row], y, rowHeight(row), [&](const DrumCell& cell) {
                    if (cell.bounds.intersects(drumSelectionBox) && !pattern.hits(cell.step).empty())
                        drumSteps.insert(cell.step);
                });
        arrangement.repaint();
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

    bool drumMouseUp(const ju::MouseEvent& event)
    {
        if (!drumGridFocused)
            return false;
        if (!drumAnchor)
            return true;
        const auto anchor = *drumAnchor;
        if (event.getDistanceFromDragStart() >= 5 || !drumSelectionBox.isEmpty())
            drumDrag(event.getPosition());
        else if (auto cell = drumCellAt(anchor))
            clickDrumStep(cell->step);
        drumAnchor.reset();
        drumSelectionBox = {};
        updateControls();
        return true;
    }

    bool adjustDrumVelocity(ju::Point<int> point, float delta)
    {
        const auto cell = editingStopped() && delta != 0.0f ? drumCellAt(point) : std::nullopt;
        if (!cell || !drumSteps.contains(cell->step))
            return false;
        if (auto* clip = selectedDrumPattern())
        {
            auto editor = makeDrumEditor(*clip);
            DrumPattern(*clip, editor).velocity(drumSteps, delta > 0 ? 1 : -1);
            updateControls();
        }
        return true;
    }
