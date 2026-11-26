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
            setMouseClickGrabsKeyboardFocus(false);
            addAndMakeVisible(close);
            addAndMakeVisible(mode);
            mode.setClickingTogglesState(true);
            mode.onClick = [this] { mode.setButtonText(mode.getToggleState() ? "Select" : "Draw"); };

            addChildComponent(horizontal);
            addChildComponent(vertical);
            horizontal.onMove = [this](double quarter) {
                if (owner.selectedDrumPattern())
                    owner.fitDrumViewport(DrumPattern::timeAtQuarter(owner.edit->tempoSequence, quarter).inSeconds());
            };
            vertical.onMove = [this](double row) {
                owner.scrollDrumRows(ju::roundToInt(row) - owner.drumFirstLane);
            };
            horizontal.setSingleStepSize(DrumPattern::step);
            vertical.setSingleStepSize(1.0);
            close.setTooltip("Close Drum Pattern Editor");
            close.onClick = [this] { owner.closeDrumEditor(); };
        }

        void resized() override
        {
            close.setBounds(getWidth() - 36, 3, 32, 26);
            mode.setBounds(getWidth() - 106, 3, 64, 26);
            horizontal.setBounds(drumNameWidth, getHeight() - NoteGrid::scroll, gridRight() - drumNameWidth, NoteGrid::scroll);
            vertical.setBounds(gridRight(), drumHeaderHeight, NoteGrid::scroll, rowBottom() - drumHeaderHeight);
        }

        int gridRight() const { return ju::jmax(drumNameWidth + 1, getWidth() - NoteGrid::scroll); }
        int velocityBottom() const { return ju::jmax(drumHeaderHeight + NoteGrid::lane, getHeight() - NoteGrid::scroll); }
        int rowBottom() const { return velocityBottom() - NoteGrid::lane; }
        int visibleRows() const { return ju::jmax(1, (rowBottom() - drumHeaderHeight) / drumRowHeight); }

        void paint(ju::Graphics& g) override { owner.paintDrumEditor(g); }
        void mouseDown(const ju::MouseEvent& event) override
        {
            contextPress = owner.contextMode && event.mods.isMiddleButtonDown()
                && !event.mods.isLeftButtonDown() && !event.mods.isRightButtonDown();
            if (contextPress)
            {
                owner.tryOpenContextToolbar(event);
                return;
            }
            if (event.mods.isLeftButtonDown() || event.mods.isRightButtonDown())
                owner.drumMouseDown(event);
        }
        void mouseDrag(const ju::MouseEvent& event) override
        {
            if (!contextPress && !rightWheel && (dragged || event.getDistanceFromDragStart() >= 4))
            {
                dragged = true;
                owner.drumDrag(event.getPosition());
            }
        }
        void mouseUp(const ju::MouseEvent& event) override
        {
            if (!std::exchange(contextPress, false))
                owner.drumMouseUp(event);
        }
        void mouseWheelMove(const ju::MouseEvent& event, const ju::MouseWheelDetails& wheel) override
        {
            if (owner.routeContextWheel(event, wheel))
                return;
            if (rightPress)
            {
                rightWheel = true;
                velocityPreview.clear();
                repaint();
                if (rightPan)
                {
                    owner.scrollDrumTime(-(wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX));
                    return;
                }
            }
            if (wheel.deltaY != 0.0f)
                owner.scrollDrumRows(wheel.deltaY > 0 ? -1 : 1);
            else if (wheel.deltaX != 0.0f)
                owner.scrollDrumTime(-wheel.deltaX);
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
        ju::TextButton close { "X" }, mode { "Draw" };
        EditorScrollBar horizontal { false }, vertical { true };
        std::optional<DrumStep> velocityStep;
        ju::Point<int> velocityAnchor;
        bool rightPress = false, rightPan = false, rightWheel = false, dragged = false, selecting = false;
        std::optional<DrumStep> pressedStep;
        std::vector<GridNote> velocityOriginals, velocityPreview;
        std::vector<GridNote> movePreview;
        std::vector<ju::ValueTree> displaced;
        std::set<DrumStep> moveSources, moveDestinations, drawPreview;
        bool contextPress = false;
    };

    te::EditItemID drumLaneID;
    std::vector<GridNote> drumClipboard;
    std::optional<int> drumPasteStep;

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
        const auto lanes = drumEditorLanes();
        drumLaneID = lanes.isEmpty() ? te::EditItemID{} : lanes.getFirst()->itemID;
        drumEditor.setVisible(true);
        drumEditor.toFront(false);
        resized();
        selectDrumVelocityLane(actionTrack());
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
        drumPasteStep.reset();
        resetDrumGesture();
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
            if (drumAnchor)
                resetDrumGesture();
        }
        else if (drumPopupStep)
            refreshDrumPopup();

        const auto lanes = drumEditorLanes();
        if (std::none_of(lanes.begin(), lanes.end(), [this](auto* lane) { return lane->itemID == drumLaneID; }))
            drumLaneID = lanes.isEmpty() ? te::EditItemID{} : lanes.getFirst()->itemID;
        const auto firstLane = ju::jlimit(0, drumLastFirstLane(), drumFirstLane);
        if (firstLane != drumFirstLane)
            scrollDrumRows(0);
        syncDrumScrollbars();

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
        drumPixelsPerSecond = (drumEditor.gridRight() - drumNameWidth)
            / std::max(0.000001, DrumPattern::timeAtQuarter(tempo, drumLeftQuarter + drumVisibleQuarters).inSeconds() - drumViewStart);
        closeDrumPopup();
        syncDrumScrollbars();
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

    void selectDrumVelocityLane(te::AudioTrack* track)
    {
        if (!track || !drumEditor.isVisible())
            return;
        const auto row = drumEditorLanes().indexOf(track);
        if (row < 0)
            return;
        drumLaneID = track->itemID;
        const auto visible = drumEditor.visibleRows();
        if (row < drumFirstLane)
            scrollDrumRows(row - drumFirstLane);
        else if (row >= drumFirstLane + visible)
            scrollDrumRows(row - (drumFirstLane + visible) + 1);
    }

    int drumLastFirstLane() const
    {
        return ju::jmax(0, drumEditorLanes().size() - drumEditor.visibleRows());
    }

    void syncDrumScrollbars()
    {
        auto* clip = selectedDrumPattern();
        if (!clip)
            return;
        const auto& tempo = edit->tempoSequence;
        const auto start = DrumPattern::quarterAtTime(tempo, clip->getPosition().getStart());
        const auto length = DrumPattern::quarterAtTime(tempo, clip->getPosition().getEnd()) - start;
        const auto scroll = std::max(0.0, std::ceil((length - drumVisibleQuarters) / DrumPattern::step - 1.0e-8))
            * DrumPattern::step;
        auto& horizontal = drumEditor.horizontal;
        horizontal.setRangeLimits(start, start + drumVisibleQuarters + scroll, ju::dontSendNotification);
        horizontal.setCurrentRange(drumLeftQuarter, drumVisibleQuarters, ju::dontSendNotification);
        horizontal.setVisible(scroll > 0.0);
        auto& vertical = drumEditor.vertical;
        const auto rows = drumEditor.visibleRows();
        vertical.setRangeLimits(0.0, rows + drumLastFirstLane(), ju::dontSendNotification);
        vertical.setCurrentRange(drumFirstLane, rows, ju::dontSendNotification);
        vertical.setVisible(drumLastFirstLane() > 0);
    }

    void scrollDrumRows(int delta)
    {
        drumFirstLane = ju::jlimit(0, drumLastFirstLane(), drumFirstLane + delta);
        closeDrumPopup();
        syncDrumScrollbars();
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
            .getIntersection({ drumNameWidth, y, drumEditor.gridRight() - drumNameWidth, height });
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
            const bool preview = drumEditor.moveDestinations.contains(cell.step) || drumEditor.drawPreview.contains(cell.step);
            g.setColour(ju::Colour(0xff202329).interpolatedWith(colour,
                preview ? 0.85f : hits.empty() ? 0.10f : 0.30f + hits.front().velocity / 255.0f));
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

    void paintDrumRuler(ju::Graphics& g, const te::MidiClip& clip) const
    {
        const ju::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion({ drumNameWidth, 32, drumEditor.gridRight() - drumNameWidth,
                             drumEditor.velocityBottom() - 32 });
        const auto& tempo = edit->tempoSequence;
        const auto end = DrumPattern::timeAtQuarter(tempo, drumLeftQuarter + drumVisibleQuarters);
        const auto firstBar = tempo.toBarsAndBeats(te::TimePosition::fromSeconds(drumViewStart)).bars;
        const auto lastBar = tempo.toBarsAndBeats(end).bars;
        const auto clipBar = tempo.toBarsAndBeats(clip.getPosition().getStart()).bars;
        const auto bottom = static_cast<float>(drumEditor.velocityBottom());
        g.setColour(ju::Colour(0xff3d4a58));
        for (int step = 0; step <= DrumPattern::maxVisibleSteps
             && step * DrumPattern::step <= drumVisibleQuarters; ++step)
            g.drawVerticalLine(drumX(DrumPattern::timeAtQuarter(tempo, drumLeftQuarter + step * DrumPattern::step)),
                               drumHeaderHeight - 4, bottom);
        g.setFont(ju::FontOptions(12.0f));
        for (int bar = firstBar; bar <= lastBar; ++bar)
        {
            const auto start = barTime(bar);
            const auto beat = tempo.toBeats(start);
            const auto beats = tempo.getTimeSigAt(start).numerator.get();
            for (int i = 1; i < beats; ++i)
            {
                g.setColour(ju::Colour(0xff536475));
                g.drawVerticalLine(drumX(tempo.toTime(beat + te::BeatDuration::fromBeats(i))), 44, bottom);
            }
            const int x = drumX(start);
            g.setColour(ju::Colour(0xff8296ac));
            g.drawVerticalLine(x, 32, bottom);
            if (bar == clipBar || x >= drumNameWidth)
            {
                g.setColour(ju::Colours::white);
                g.drawText(ju::String(bar - clipBar + 1), std::max(x, drumNameWidth) + 4, 32, 35, 20,
                           ju::Justification::centredLeft);
            }
        }
    }

    int drumVelocityY(int velocity) const
    {
        return drumEditor.velocityBottom() - 4 - ju::roundToInt((velocity - 1) * (NoteGrid::lane - 8) / 126.0);
    }

    void paintDrumVelocity(ju::Graphics& g, te::MidiClip& clip, const DrumPattern& pattern)
    {
        const int top = drumEditor.rowBottom();
        const int bottom = drumEditor.velocityBottom();
        g.setColour(ju::Colour(0xff192028));
        g.fillRect(0, top, drumEditor.gridRight(), bottom - top);
        g.setColour(ju::Colours::lightgrey);
        g.drawText("Velocity", 8, top, drumNameWidth - 12, bottom - top, ju::Justification::centredLeft);
        for (const bool selectedPass : { false, true })
            for (auto* lane : drumEditorLanes())
                forDrumCells(clip, pattern, *lane, top, bottom - top, [&](const DrumCell& cell) {
                    const auto& hits = pattern.hits(cell.step);
                    if (hits.empty() || drumSteps.contains(cell.step) != selectedPass)
                        return;
                    const auto preview = std::find_if(drumEditor.velocityPreview.begin(), drumEditor.velocityPreview.end(),
                        [&](const auto& note) { return note.id == hits.front().id; });
                    const auto velocity = preview == drumEditor.velocityPreview.end() ? hits.front().velocity : preview->velocity;
                    const auto x = cell.bounds.getCentreX();
                    const auto y = drumVelocityY(velocity);
                    g.setColour(selectedPass ? ju::Colours::white : ju::Colour(0xff68b9ee));
                    g.drawVerticalLine(x, static_cast<float>(y), static_cast<float>(bottom - 3));
                    g.fillRect(x - 3, y - 2, 7, 4);
                });
    }

    std::optional<DrumStep> drumVelocityAt(ju::Point<int> point, te::MidiClip& clip, const DrumPattern& pattern)
    {
        std::optional<DrumStep> result;
        int closest = std::numeric_limits<int>::max();
        for (const bool selectedPass : { true, false })
        {
            for (auto* lane : drumEditorLanes())
                forDrumCells(clip, pattern, *lane, drumEditor.rowBottom(), NoteGrid::lane, [&](const DrumCell& cell) {
                    const auto& hits = pattern.hits(cell.step);
                    if (hits.empty() || drumSteps.contains(cell.step) != selectedPass)
                        return;
                    const auto distance = std::abs(drumVelocityY(hits.front().velocity) - point.y);
                    if (std::abs(cell.bounds.getCentreX() - point.x) <= 6
                        && point.y >= drumVelocityY(hits.front().velocity) - 6 && distance < closest)
                    {
                        result = cell.step;
                        closest = distance;
                    }
                });
            if (result)
                break;
        }
        return result;
    }

    void beginDrumVelocity(DrumStep step, const DrumPattern& pattern)
    {
        drumEditor.velocityStep = step;
        for (auto cell : drumSteps)
        {
            const auto& hits = pattern.hits(cell);
            drumEditor.velocityOriginals.insert(drumEditor.velocityOriginals.end(), hits.begin(), hits.end());
        }
    }

    void previewDrumVelocity(ju::Point<int> point)
    {
        const auto dy = drumAnchor->y - point.y;
        auto delta = drumEditor.rightPress ? dy : ju::roundToInt(dy * 126.0 / (NoteGrid::lane - 8));
        drumEditor.velocityPreview = drumEditor.velocityOriginals;
        for (const auto& note : drumEditor.velocityOriginals)
            delta = std::clamp(delta, 1 - note.velocity, 127 - note.velocity);
        for (auto& note : drumEditor.velocityPreview)
            note.velocity += delta;
        drumEditor.velocityAnchor = point;
        drumEditor.repaint();
    }

    std::vector<GridNote> drumPasteNotes(const DrumPattern& pattern, const MidiNoteEditor& model) const
    {
        if (drumClipboard.empty() || pattern.count() == 0)
            return {};
        std::set<int> pitches;
        for (auto* lane : drumEditorLanes())
            if (auto* sampler = drumMaster::sampler(*lane))
                pitches.insert(static_cast<int>(sampler->state.getChildWithName(drumMaster::laneType)
                                                       .getProperty(drumMaster::noteProperty, -1)));
        const DrumStep target { 0, drumPasteStep.value_or(0) };
        const auto base = pattern.quarterForSource(pattern.start(target));
        auto result = drumClipboard;
        for (auto& note : result)
        {
            const auto end = pattern.sourceAtQuarter(base + note.end());
            note.start = pattern.sourceAtQuarter(base + note.start);
            note.length = end - note.start;
            if (!pitches.contains(note.pitch) || !model.valid(note))
                return {};
        }
        return result;
    }

    std::vector<ju::ValueTree> selectedDrumNoteIDs() const
    {
        std::vector<ju::ValueTree> ids;
        if (auto* clip = selectedDrumPattern(); clip && !drumSteps.empty())
        {
            auto model = makeDrumEditor(*clip);
            const DrumPattern pattern(*clip, model);
            for (auto step : drumSteps)
                for (const auto& note : pattern.hits(step))
                    ids.push_back(note.id);
        }
        return ids;
    }

    void restoreDrumNoteSelection(const std::vector<ju::ValueTree>& ids)
    {
        if (auto* clip = selectedDrumPattern(); clip && !ids.empty())
        {
            auto model = makeDrumEditor(*clip);
            const DrumPattern pattern(*clip, model);
            drumSteps.clear();
            for (const auto& note : model.notes())
                if (std::find(ids.begin(), ids.end(), note.id) != ids.end())
                    drumSteps.insert({ note.pitch, pattern.indexAt(note.start) });
        }
    }

    static bool isDrumMove(UniversalAction action)
    {
        return action == UniversalAction::left || action == UniversalAction::right
            || action == UniversalAction::up || action == UniversalAction::down;
    }

    std::vector<GridNote> drumMoveNotes(UniversalAction action, const DrumPattern& pattern,
                                        const MidiNoteEditor& model) const
    {
        const bool vertical = action == UniversalAction::up || action == UniversalAction::down;
        const int direction = action == UniversalAction::up || action == UniversalAction::left ? -1 : 1;
        std::vector<int> pitches;
        if (vertical)
            for (auto* lane : drumEditorLanes())
            {
                auto* sampler = drumMaster::sampler(*lane);
                pitches.push_back(sampler ? static_cast<int>(sampler->state.getChildWithName(drumMaster::laneType)
                    .getProperty(drumMaster::noteProperty, -1)) : -1);
            }

        std::set<DrumStep> destinations;
        std::vector<GridNote> changes;
        for (auto source : drumSteps)
        {
            const auto& hits = pattern.hits(source);
            if (hits.empty())
                continue;
            auto destination = source;
            if (vertical)
            {
                const auto row = std::find(pitches.begin(), pitches.end(), source.pitch);
                if (row == pitches.end() || std::find(std::next(row), pitches.end(), source.pitch) != pitches.end())
                    return {};
                const auto target = static_cast<int>(std::distance(pitches.begin(), row)) + direction;
                if (!ju::isPositiveAndBelow(target, static_cast<int>(pitches.size())) || pitches[target] < 0)
                    return {};
                destination.pitch = pitches[target];
            }
            else
                destination.index += direction;

            if (!ju::isPositiveAndBelow(destination.index, pattern.count()) || !destinations.insert(destination).second
                || (!drumSteps.contains(destination) && !pattern.hits(destination).empty()))
                return {};

            for (auto note : hits)
            {
                note.pitch = destination.pitch;
                if (!vertical)
                {
                    const auto shift = direction * DrumPattern::step;
                    const auto end = pattern.sourceAtQuarter(pattern.quarterForSource(note.end()) + shift);
                    note.start = pattern.sourceAtQuarter(pattern.quarterForSource(note.start) + shift);
                    note.length = end - note.start;
                }
                if (!model.valid(note))
                    return {};
                changes.push_back(note);
            }
        }
        return changes;
    }

    bool canDispatchDrumEdit(UniversalAction action) const
    {
        const bool paste = action == UniversalAction::paste;
        const bool selectionCommand = action == UniversalAction::cut || action == UniversalAction::copy
            || action == UniversalAction::remove || isDrumMove(action);
        if (paste ? drumClipboard.empty() : !selectionCommand || drumSteps.empty())
            return false;
        auto* clip = selectedDrumPattern();
        auto model = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, model);
        if (paste)
            return !drumPasteNotes(pattern, model).empty();
        if (isDrumMove(action))
            return !drumMoveNotes(action, pattern, model).empty();
        return std::any_of(drumSteps.begin(), drumSteps.end(), [&](auto step) { return !pattern.hits(step).empty(); });
    }

    void dispatchDrumEdit(UniversalAction action)
    {
        auto* clip = selectedDrumPattern();
        auto model = makeDrumEditor(*clip);
        DrumPattern pattern(*clip, model);
        if (isDrumMove(action))
        {
            const auto changes = drumMoveNotes(action, pattern, model);
            if (model.change(changes, "Move drum hits"))
            {
                drumSteps.clear();
                for (const auto& note : changes)
                    drumSteps.insert({ note.pitch, pattern.indexAt(note.start) });
            }
            updateControls();
            return;
        }
        if (action == UniversalAction::copy || action == UniversalAction::cut)
        {
            drumClipboard.clear();
            double first = std::numeric_limits<double>::infinity();
            for (auto step : drumSteps)
                if (!pattern.hits(step).empty())
                    first = std::min(first, pattern.quarterForSource(pattern.start(step)));
            for (auto step : drumSteps)
                for (auto note : pattern.hits(step))
                {
                    const auto start = pattern.quarterForSource(note.start);
                    note.length = pattern.quarterForSource(note.end()) - start;
                    note.start = start - first;
                    note.id = note.id.createCopy();
                    drumClipboard.push_back(std::move(note));
                }
        }
        if (action == UniversalAction::cut || action == UniversalAction::remove)
        {
            if (pattern.remove(drumSteps))
                drumSteps.clear();
        }
        else if (action == UniversalAction::paste)
        {
            const auto notes = drumPasteNotes(pattern, model);
            if (!model.insert(notes, "Paste drum hits").empty())
            {
                drumSteps.clear();
                for (const auto& note : notes)
                    drumSteps.insert({ note.pitch, pattern.indexAt(note.start) });
            }
        }
        updateControls();
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
        const auto activeRows = ju::Rectangle<int>(0, drumHeaderHeight, drumEditor.gridRight(),
                                                    drumEditor.rowBottom() - drumHeaderHeight);
        {
            const ju::Graphics::ScopedSaveState rows(g);
            g.reduceClipRegion(activeRows);
            for (int i = drumFirstLane, y = drumHeaderHeight;
                 i < lanes.size() && y < drumEditor.rowBottom(); ++i, y += drumRowHeight)
            {
                if (lanes[i]->itemID == drumLaneID)
                {
                    g.setColour(ju::Colour(0xff35485a));
                    g.fillRect(0, y, drumNameWidth, drumRowHeight);
                }
                g.setColour(ju::Colours::white);
                g.setFont(ju::FontOptions(14.0f));
                g.drawFittedText(lanes[i]->getName(), { 8, y, drumNameWidth - 12, drumRowHeight },
                                 ju::Justification::centredLeft, 1);
                paintDrumGrid(g, *clip, pattern, *lanes[i], y, drumRowHeight, trackColour(*lanes[i]));
            }
        }
        paintDrumVelocity(g, *clip, pattern);
        paintDrumRuler(g, *clip);
        if (drumPasteStep && !lanes.isEmpty())
            forDrumCells(*clip, pattern, *lanes.getFirst(), 32, 5, [&](const DrumCell& cell) {
                if (cell.step.index == *drumPasteStep)
                {
                    g.setColour(ju::Colour(0xffacc3d8));
                    g.fillRect(cell.bounds.getX(), cell.bounds.getY(), ju::jmin(7, cell.bounds.getWidth()), 4);
                }
            });
        if (!drumEditor.velocityPreview.empty())
        {
            const auto [low, high] = std::minmax_element(drumEditor.velocityPreview.begin(), drumEditor.velocityPreview.end(),
                [](const auto& a, const auto& b) { return a.velocity < b.velocity; });
            const auto text = low->velocity == high->velocity ? ju::String(low->velocity)
                : ju::String(low->velocity) + "–" + ju::String(high->velocity);
            const auto point = drumEditor.velocityAnchor;
            const auto feedbackY = point.y >= 59 ? point.y - 27 : point.y + 6;
            const auto bounds = ju::Rectangle<int>(point.x + 6, feedbackY, 74, 21)
                .constrainedWithin(drumEditor.getLocalBounds().withTrimmedTop(32));
            g.setColour(ju::Colour(0xff141b24));
            g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
            g.setColour(ju::Colours::white);
            g.drawText(text, bounds, ju::Justification::centred);
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
        if (!clip || point.x < drumNameWidth || point.x >= drumEditor.gridRight()
            || point.y < drumHeaderHeight || point.y >= drumEditor.rowBottom())
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

    void setDrumPastePosition(int x)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || x < drumNameWidth || x >= drumEditor.gridRight())
            return;
        auto model = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, model);
        const auto absolute = edit->tempoSequence.toBeats(te::TimePosition::fromSeconds(drumTimeAt(x)));
        auto source = (absolute - clip->getContentStartBeat()).inBeats();
        if (clip->isLooping() && pattern.range.getLength() > 0.0)
            source = pattern.range.getStart() + std::fmod(std::max(0.0, source), pattern.range.getLength());
        const auto index = pattern.indexAt(source);
        if (ju::isPositiveAndBelow(index, pattern.count()))
            drumPasteStep = index;
    }

    void resetDrumGesture()
    {
        drumAnchor.reset();
        drumSelectionBox = {};
        drumEditor.pressedStep.reset();
        drumEditor.velocityStep.reset();
        drumEditor.velocityOriginals.clear();
        drumEditor.velocityPreview.clear();
        drumEditor.movePreview.clear();
        drumEditor.moveSources.clear();
        drumEditor.moveDestinations.clear();
        drumEditor.displaced.clear();
        drumEditor.drawPreview.clear();
        drumEditor.rightPress = drumEditor.rightPan = drumEditor.rightWheel = drumEditor.dragged = false;
    }

    void drumMouseDown(const ju::MouseEvent& event)
    {
        resetDrumGesture();
        auto* clip = selectedDrumPattern();
        if (!clip)
            return;
        const auto point = event.getPosition();
        const auto right = event.mods.isRightButtonDown();
        drumEditor.rightPress = right;
        drumEditor.selecting = drumEditor.mode.getToggleState();
        const auto cell = drumCellAt(point);
        auto model = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, model);
        const auto laneHit = point.y >= drumEditor.rowBottom() && point.y < drumEditor.velocityBottom()
            ? drumVelocityAt(point, *clip, pattern) : std::nullopt;
        const auto hit = laneHit ? laneHit
            : cell && !pattern.hits(cell->step).empty() ? std::optional{cell->step} : std::nullopt;
        const bool grid = point.x >= drumNameWidth && point.x < drumEditor.gridRight()
            && point.y >= drumHeaderHeight && point.y < drumEditor.velocityBottom();
        if (right && !hit)
        {
            drumEditor.rightPan = grid;
            return;
        }

        closeDrumPopup();
        focusDrumEditor();
        if (!editingStopped())
            return;
        if (right)
        {
            drumEditor.pressedStep = hit;
            drumAnchor = point;
            if (drumSteps.contains(*hit))
                beginDrumVelocity(*hit, pattern);
            return;
        }
        if (point.y >= 32 && point.y < drumHeaderHeight)
        {
            setDrumPastePosition(point.x);
            updateControls();
            return;
        }
        if (point.y >= drumHeaderHeight && point.y < drumEditor.rowBottom())
            if (auto* lane = drumEditorLanes()[drumFirstLane + (point.y - drumHeaderHeight) / drumRowHeight])
                drumLaneID = lane->itemID;
        if (cell && event.getNumberOfClicks() > 1)
        {
            openDrumPopup(*cell);
            return;
        }
        if (!cell && !laneHit)
            return;
        drumAnchor = point;
        drumEditor.pressedStep = hit;
        if (cell)
            drumPasteStep = cell->step.index;
        if (hit && !drumSteps.contains(*hit))
            drumSteps = { *hit };
        if (!hit)
            drumSteps.clear();
        if (laneHit)
            beginDrumVelocity(*laneHit, pattern);
        else if (hit)
            drumEditor.moveSources = drumSteps;
        updateControls();
    }

    void previewDrumMove(ju::Point<int> point, te::MidiClip& clip, const DrumPattern& pattern, const MidiNoteEditor& model)
    {
        drumEditor.movePreview.clear();
        drumEditor.moveDestinations.clear();
        drumEditor.displaced.clear();
        if (drumEditor.moveSources != drumSteps)
            return;
        const auto tracks = drumEditorLanes();
        std::vector<int> pitches;
        for (auto* lane : tracks)
        {
            auto* sampler = drumMaster::sampler(*lane);
            pitches.push_back(sampler ? static_cast<int>(sampler->state.getChildWithName(drumMaster::laneType)
                .getProperty(drumMaster::noteProperty, -1)) : -1);
        }
        const auto rowFor = [&](int pitch) {
            const auto it = std::find(pitches.begin(), pitches.end(), pitch);
            return it == pitches.end() || std::find(std::next(it), pitches.end(), pitch) != pitches.end()
                ? -1 : static_cast<int>(it - pitches.begin());
        };
        auto dRow = (std::max(drumHeaderHeight, point.y) - drumHeaderHeight) / drumRowHeight
            - (drumAnchor->y - drumHeaderHeight) / drumRowHeight;
        const auto quarter = [&](int x) {
            return DrumPattern::quarterAtTime(clip.edit.tempoSequence, te::TimePosition::fromSeconds(drumTimeAt(x)));
        };
        auto dStep = ju::roundToInt((quarter(point.x) - quarter(drumAnchor->x)) / DrumPattern::step);
        const auto endQuarter = pattern.quarterForSource(pattern.range.getEnd());
        for (auto source : drumSteps)
        {
            const auto& hits = pattern.hits(source);
            if (hits.empty())
                return;
            const auto row = rowFor(source.pitch);
            if (row < 0)
                dRow = 0;
            else
                dRow = std::clamp(dRow, -row, static_cast<int>(pitches.size()) - 1 - row);
            dStep = std::clamp(dStep, -source.index, pattern.count() - 1 - source.index);
            for (const auto& hit : hits)
            {
                const auto available = (endQuarter - pattern.quarterForSource(hit.end())) / DrumPattern::step;
                dStep = std::min(dStep, std::max(0, static_cast<int>(std::floor(available + 1.0e-8))));
            }
        }
        if (dRow == 0 && dStep == 0)
            return;

        std::vector<GridNote> changes;
        std::vector<ju::ValueTree> displaced;
        std::set<DrumStep> destinations;
        for (auto source : drumSteps)
        {
            const auto row = rowFor(source.pitch);
            const auto pitch = dRow == 0 || row < 0 ? source.pitch : pitches[row + dRow];
            const DrumStep target { pitch, source.index + dStep };
            if (pitch < 0 || (dRow != 0 && (row < 0 || rowFor(pitch) < 0))
                || !destinations.insert(target).second)
                return;
            if (!drumSteps.contains(target))
                for (const auto& hit : pattern.hits(target))
                    displaced.push_back(hit.id);
            for (auto note : pattern.hits(source))
            {
                if (dStep != 0)
                {
                    const auto shift = dStep * DrumPattern::step;
                    const auto end = pattern.sourceAtQuarter(pattern.quarterForSource(note.end()) + shift);
                    note.start = pattern.sourceAtQuarter(pattern.quarterForSource(note.start) + shift);
                    note.length = end - note.start;
                }
                note.pitch = pitch;
                if (!model.valid(note))
                    return;
                changes.push_back(note);
            }
        }
        drumEditor.movePreview = std::move(changes);
        drumEditor.moveDestinations = std::move(destinations);
        drumEditor.displaced = std::move(displaced);
    }

    void drumDrag(ju::Point<int> point)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !drumAnchor || !editingStopped())
            return;
        if (drumEditor.velocityStep)
        {
            previewDrumVelocity(point);
            return;
        }
        if (drumEditor.rightPress)
            return;
        auto model = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, model);
        if (drumEditor.pressedStep)
            previewDrumMove(point, *clip, pattern, model);
        else if (!drumEditor.selecting)
        {
            drumEditor.drawPreview.clear();
            const auto start = drumCellAt(*drumAnchor);
            const auto end = drumCellAt({ point.x, drumAnchor->y });
            if (start && end)
                for (int i = std::min(start->step.index, end->step.index); i <= std::max(start->step.index, end->step.index); ++i)
                {
                    const DrumStep step { start->step.pitch, i };
                    if (pattern.hits(step).empty())
                        drumEditor.drawPreview.insert(step);
                }
        }
        else
        {
            drumSelectionBox = ju::Rectangle<int>(*drumAnchor, point).expanded(1)
                .getIntersection({drumNameWidth, drumHeaderHeight, drumEditor.gridRight() - drumNameWidth,
                    drumEditor.rowBottom() - drumHeaderHeight});
            drumSteps.clear();
            const auto tracks = drumEditorLanes();
            for (int row = drumFirstLane, y = drumHeaderHeight; row < tracks.size() && y < drumEditor.rowBottom(); y += drumRowHeight, ++row)
                forDrumCells(*clip, pattern, *tracks[row], y, drumRowHeight, [&](const DrumCell& cell) {
                    if (cell.bounds.intersects(drumSelectionBox) && !pattern.hits(cell.step).empty())
                        drumSteps.insert(cell.step);
                });
        }
        drumEditor.repaint();
    }

    void drumMouseUp(const ju::MouseEvent& event)
    {
        if (auto* clip = selectedDrumPattern(); clip && drumAnchor && !drumEditor.rightWheel && editingStopped())
        {
            auto model = makeDrumEditor(*clip);
            const DrumPattern pattern(*clip, model);
            const auto dragged = drumEditor.dragged || event.getDistanceFromDragStart() >= 4;
            if (dragged)
            {
                drumDrag(event.getPosition());
                if (!drumEditor.velocityPreview.empty())
                    model.change(drumEditor.velocityPreview, "Change drum velocity");
                else if (!drumEditor.movePreview.empty()
                    && model.change(drumEditor.movePreview, "Move drum hits", {}, drumEditor.displaced))
                    drumSteps = drumEditor.moveDestinations;
            }
            else if (drumEditor.rightPress)
            {
                if (const auto step = drumEditor.pressedStep; step && drumSteps.erase(*step) == 0)
                    drumSteps.insert(*step);
            }
            else if (drumEditor.pressedStep)
                drumSteps = { *drumEditor.pressedStep };
            else if (!drumEditor.selecting && !drumEditor.velocityStep)
                if (const auto cell = drumCellAt(*drumAnchor); cell && pattern.hits(cell->step).empty())
                    drumEditor.drawPreview.insert(cell->step);

            if (!drumEditor.drawPreview.empty())
            {
                std::vector<GridNote> additions;
                for (const auto cell : drumEditor.drawPreview)
                {
                    const auto start = pattern.start(cell);
                    const auto end = std::min(pattern.range.getEnd(), pattern.start(cell, 4));
                    additions.push_back({ {}, start, end - start, cell.pitch, DrumPattern::defaultVelocity });
                }
                if (!model.insert(additions, "Draw drum hits").empty())
                    drumSteps = drumEditor.drawPreview;
            }
        }
        resetDrumGesture();
        updateControls();
    }
