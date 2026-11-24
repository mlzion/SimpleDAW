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
            horizontal.setBounds(drumNameWidth, getHeight() - NoteGrid::scroll, gridRight() - drumNameWidth, NoteGrid::scroll);
            vertical.setBounds(gridRight(), drumHeaderHeight, NoteGrid::scroll, rowBottom() - drumHeaderHeight);
        }

        int gridRight() const { return ju::jmax(drumNameWidth + 1, getWidth() - NoteGrid::scroll); }
        int velocityBottom() const { return ju::jmax(drumHeaderHeight + NoteGrid::lane, getHeight() - NoteGrid::scroll); }
        int rowBottom() const { return velocityBottom() - NoteGrid::lane; }
        int visibleRows() const { return ju::jmax(1, (rowBottom() - drumHeaderHeight) / drumRowHeight); }

        void showVelocityAt(ju::Point<int> pointer)
        {
            velocityAnchor = pointer;
            velocityUntil = ju::Time::getMillisecondCounterHiRes() + 1100.0;
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
            if (velocityStep)
            {
                owner.previewDrumVelocity(event.getPosition());
                return;
            }
            if (event.getDistanceFromDragStart() >= 5)
                owner.drumDrag(event.getPosition());
        }
        void mouseUp(const ju::MouseEvent& event) override
        {
            if (velocityStep)
                owner.finishDrumVelocity();
            else
                owner.drumMouseUp(event);
        }
        void mouseWheelMove(const ju::MouseEvent& event, const ju::MouseWheelDetails& wheel) override
        {
            owner.focusDrumEditor();
            // Precision wheels can report both axes for an ordinary vertical
            // gesture. Give the dominant axis to the intended operation.
            if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
                owner.scrollDrumTime(-wheel.deltaX);
            else if (wheel.deltaY != 0.0f && !owner.adjustDrumVelocity(wheel.deltaY, event.getPosition()))
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
        ju::String velocityFeedback;
        EditorScrollBar horizontal { false }, vertical { true };
        std::optional<DrumStep> velocityStep;
        ju::Point<int> velocityAnchor;
        double velocityUntil = 0.0;
        int previewVelocity = 0;
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
        drumEditor.velocityStep.reset();
        drumEditor.velocityUntil = 0.0;
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
            drumEditor.velocityStep.reset();
            drumPendingClicks.clear();
            drumAnchor.reset();
            drumSelectionBox = {};
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
        drumEditor.velocityFeedback = feedback;
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
        auto* lane = trackForID(drumLaneID);
        if (!lane)
            return;
        g.setColour(ju::Colours::lightgrey);
        g.drawFittedText("Velocity / " + lane->getName(), { 8, top, drumNameWidth - 12, bottom - top },
                         ju::Justification::centredLeft, 2);
        forDrumCells(clip, pattern, *lane, top, bottom - top, [&](const DrumCell& cell) {
            const auto& hits = pattern.hits(cell.step);
            if (hits.empty())
                return;
            const auto velocity = drumEditor.velocityStep == cell.step ? drumEditor.previewVelocity : hits.front().velocity;
            const auto x = cell.bounds.getCentreX();
            const auto y = drumVelocityY(velocity);
            g.setColour(drumSteps.contains(cell.step) ? ju::Colours::white : ju::Colour(0xff68b9ee));
            g.drawVerticalLine(x, static_cast<float>(y), static_cast<float>(bottom - 3));
            g.fillRect(x - 3, y - 2, 7, 4);
        });
    }

    bool beginDrumVelocity(ju::Point<int> point)
    {
        auto* clip = selectedDrumPattern();
        auto* lane = trackForID(drumLaneID);
        if (!clip || !lane)
            return false;
        auto model = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, model);
        forDrumCells(*clip, pattern, *lane, drumEditor.rowBottom(), NoteGrid::lane, [&](const DrumCell& cell) {
            if (cell.bounds.contains(point) && !pattern.hits(cell.step).empty())
                drumEditor.velocityStep = cell.step;
        });
        if (!drumEditor.velocityStep)
            return false;
        previewDrumVelocity(point);
        return true;
    }

    void previewDrumVelocity(ju::Point<int> point)
    {
        const auto fraction = (drumEditor.velocityBottom() - 4 - point.y) / static_cast<double>(NoteGrid::lane - 8);
        drumEditor.previewVelocity = ju::jlimit(1, 127, ju::roundToInt(1.0 + 126.0 * fraction));
        drumEditor.showVelocityAt(point);
        drumEditor.repaint();
    }

    void finishDrumVelocity()
    {
        if (auto* clip = selectedDrumPattern(); clip && editingStopped())
        {
            auto model = makeDrumEditor(*clip);
            DrumPattern(*clip, model).velocity({ *drumEditor.velocityStep }, drumEditor.previewVelocity, true);
        }
        drumEditor.velocityStep.reset();
        drumEditor.velocityUntil = 0.0;
        updateControls();
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

    bool canDispatchDrumEdit(UniversalAction action) const
    {
        const bool paste = action == UniversalAction::paste;
        const bool selectionCommand = action == UniversalAction::cut || action == UniversalAction::copy
            || action == UniversalAction::remove;
        if (paste ? drumClipboard.empty() : !selectionCommand || drumSteps.empty())
            return false;
        auto* clip = selectedDrumPattern();
        auto model = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, model);
        if (paste)
            return !drumPasteNotes(pattern, model).empty();
        return std::any_of(drumSteps.begin(), drumSteps.end(), [&](auto step) { return !pattern.hits(step).empty(); });
    }

    void dispatchDrumEdit(UniversalAction action)
    {
        auto* clip = selectedDrumPattern();
        auto model = makeDrumEditor(*clip);
        DrumPattern pattern(*clip, model);
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
        if (drumEditor.velocityStep || ju::Time::getMillisecondCounterHiRes() < drumEditor.velocityUntil)
        {
            const auto text = drumEditor.velocityStep ? ju::String(drumEditor.previewVelocity)
                : drumEditor.velocityFeedback.fromFirstOccurrenceOf(" ", false, false);
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

    void drumMouseDown(const ju::MouseEvent& event)
    {
        closeDrumPopup();
        const auto point = event.getPosition();
        if (editingStopped() && event.mods.isLeftButtonDown() && point.y >= 32 && point.y < drumHeaderHeight)
        {
            setDrumPastePosition(point.x);
            updateControls();
            return;
        }
        if (point.y >= drumHeaderHeight && point.y < drumEditor.rowBottom())
            if (auto* lane = drumEditorLanes()[drumFirstLane + (point.y - drumHeaderHeight) / drumRowHeight])
                drumLaneID = lane->itemID;
        if (editingStopped() && event.mods.isLeftButtonDown() && point.y >= drumEditor.rowBottom()
            && point.y < drumEditor.velocityBottom() && beginDrumVelocity(point))
            return;
        const auto cell = editingStopped() && !event.mods.isShiftDown() ? drumCellAt(event.getPosition()) : std::nullopt;
        if (!cell)
        {
            drumEditor.repaint();
            return;
        }
        drumPasteStep = cell->step.index;
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
            .getIntersection(ju::Rectangle<int>(drumNameWidth, drumHeaderHeight, drumEditor.gridRight() - drumNameWidth,
                                                 drumEditor.rowBottom() - drumHeaderHeight));
        drumSteps.clear();
        auto editor = makeDrumEditor(*clip);
        const DrumPattern pattern(*clip, editor);
        const auto tracks = drumEditorLanes();
        for (int row = drumFirstLane, y = drumHeaderHeight; row < tracks.size() && y < drumEditor.rowBottom(); y += drumRowHeight, ++row)
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

    bool adjustDrumVelocity(float delta, ju::Point<int> pointer)
    {
        auto* clip = selectedDrumPattern();
        if (!clip || !editingStopped() || drumSteps.empty() || delta == 0.0f
            || drumEditor.velocityFeedback.isEmpty())
            return false;

        // Normalize precision-wheel input and detents to a useful MIDI change.
        const auto amount = ju::jlimit(4, 16, ju::roundToInt(std::abs(delta) * 12.0f));
        auto editor = makeDrumEditor(*clip);
        DrumPattern(*clip, editor).velocity(drumSteps, delta > 0 ? amount : -amount);
        drumEditor.showVelocityAt(pointer);
        updateControls();
        return true;
    }
