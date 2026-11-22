// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

struct GridNote
{
    ju::ValueTree id;
    double start, length;
    int pitch, velocity;

    double end() const { return start + length; }
    bool operator==(const GridNote&) const = default;
};

class MidiNoteEditor
{
public:
    explicit MidiNoteEditor(te::MidiClip& target) : clip(target) {}

    ju::Range<double> range() const
    {
        if (clip.isLooping())
        {
            const auto loop = clip.getLoopRangeBeats();
            return { loop.getStart().inBeats(), loop.getEnd().inBeats() };
        }
        const auto source = clip.getContentStartBeat();
        return { (clip.getStartBeat() - source).inBeats(), (clip.getEndBeat() - source).inBeats() };
    }

    std::vector<GridNote> notes() const
    {
        std::vector<GridNote> result;
        for (auto* note : clip.getSequence().getNotes())
            result.push_back({ note->state, note->getStartBeat().inBeats(), note->getLengthBeats().inBeats(),
                               note->getNoteNumber(), note->getVelocity() });
        return result;
    }

    bool valid(const GridNote& note) const
    {
        const auto limits = range();
        constexpr auto epsilon = 1.0e-8;
        return note.pitch >= 0 && note.pitch <= 127 && note.velocity >= 1 && note.velocity <= 127
            && note.start >= std::max(0.0, limits.getStart()) - epsilon
            && note.end() <= limits.getEnd() + epsilon && note.length >= minimumLength - epsilon;
    }

    std::vector<ju::ValueTree> insert(const std::vector<GridNote>& notes, const ju::String& label)
    {
        if (notes.empty() || !std::all_of(notes.begin(), notes.end(), [this](const auto& note) { return valid(note); }))
            return {};
        auto& history = clip.edit.getUndoManager();
        history.beginNewTransaction(label);
        std::vector<ju::ValueTree> ids;
        auto& sequence = clip.getSequence();
        for (const auto& note : notes)
        {
            auto* added = note.id.isValid()
                ? sequence.addNote(te::MidiNote(note.id.createCopy()), &history)
                : sequence.addNote(note.pitch, te::BeatPosition::fromBeats(note.start),
                                   te::BeatDuration::fromBeats(note.length), note.velocity, 0, &history);
            write(*added, note, history);
            ids.push_back(added->state);
        }
        finish(history);
        return ids;
    }

    bool change(const std::vector<GridNote>& changes, const ju::String& label)
    {
        auto& sequence = clip.getSequence();
        std::vector<std::pair<te::MidiNote*, GridNote>> pending;
        for (const auto& change : changes)
        {
            auto* note = sequence.getNoteFor(change.id);
            if (!note)
                return false;
            const bool timing = note->getStartBeat().inBeats() != change.start
                || note->getLengthBeats().inBeats() != change.length;
            if ((timing && !valid(change)) || change.pitch < 0 || change.pitch > 127
                || change.velocity < 1 || change.velocity > 127)
                return false;
            if (timing || note->getNoteNumber() != change.pitch || note->getVelocity() != change.velocity)
                pending.emplace_back(note, change);
        }
        if (pending.empty())
            return false;
        auto& history = clip.edit.getUndoManager();
        history.beginNewTransaction(label);
        for (const auto& [note, value] : pending)
            write(*note, value, history);
        finish(history);
        return true;
    }

    bool remove(const std::vector<ju::ValueTree>& ids, const ju::String& label)
    {
        auto& sequence = clip.getSequence();
        if (ids.empty() || !std::all_of(ids.begin(), ids.end(), [&](const auto& id) { return sequence.getNoteFor(id); }))
            return false;
        auto& history = clip.edit.getUndoManager();
        history.beginNewTransaction(label);
        for (const auto& id : ids)
            sequence.removeNote(*sequence.getNoteFor(id), &history);
        finish(history);
        return true;
    }

    static constexpr double minimumLength = 1.0 / te::Edit::ticksPerQuarterNote;

private:
    static void write(te::MidiNote& note, const GridNote& value, ju::UndoManager& history)
    {
        if (note.getStartBeat().inBeats() != value.start || note.getLengthBeats().inBeats() != value.length)
            note.setStartAndLength(te::BeatPosition::fromBeats(value.start),
                                   te::BeatDuration::fromBeats(value.length), &history);
        if (note.getNoteNumber() != value.pitch)
            note.setNoteNumber(value.pitch, &history);
        if (note.getVelocity() != value.velocity)
            note.setVelocity(value.velocity, &history);
    }

    void finish(ju::UndoManager& history)
    {
        history.beginNewTransaction();
        clip.edit.markAsChanged();
    }

    te::MidiClip& clip;
};

// Beat/pitch geometry is independent of clip storage and can also display drum rows.
struct NoteGrid
{
    static constexpr int keys = 62, ruler = 23, lane = 57, scroll = 15;
    double left = 0.0, scale = 112.0;
    int top = 48, row = 15;

    double beat(float x) const { return left + (x - keys) / scale; }
    float x(double beat) const { return static_cast<float>(keys + (beat - left) * scale); }
    int pitch(float y) const { return ju::jlimit(0, 127, 127 - top - static_cast<int>(std::floor((y - ruler) / row))); }
    int y(int pitch) const { return ruler + (127 - pitch - top) * row; }
    ju::Rectangle<float> bounds(const GridNote& note) const
    {
        return { x(note.start), static_cast<float>(y(note.pitch) + 1),
                 std::max(2.0f, x(note.end()) - x(note.start)), static_cast<float>(row - 2) };
    }
};

class PianoRoll final : public ju::Component, private ju::Timer
{
public:
    enum class Action { cut, remove, copy, paste, up, down, left, right, startMinus, startPlus, endMinus, endPlus };

    PianoRoll()
    {
        setTitle("Piano Roll");
        setWantsKeyboardFocus(true);
        for (auto* child : std::array<ju::Component*, 11> {
                &name, &mode, &snap, &zoomOut, &zoomIn, &pitchOut, &pitchIn, &quantize, &velocity, &horizontal, &vertical })
            addAndMakeVisible(*child);
        name.setMinimumHorizontalScale(0.75f);
        mode.setClickingTogglesState(true);
        mode.setTooltip("Switch between drawing notes and dragging a selection box");
        mode.onClick = [this] { mode.setButtonText(mode.getToggleState() ? "SELECT" : "DRAW"); };
        snap.addItemList({ "Snap None", "Snap 1/4", "Snap 1/8", "Snap 1/16", "Snap 1/32" }, 1);
        snap.setSelectedId(4, ju::dontSendNotification);
        snap.onChange = [this] { notify(); };
        zoomOut.onClick = [this] { zoomTime(0.8); };
        zoomIn.onClick = [this] { zoomTime(1.25); };
        pitchOut.onClick = [this] { zoomPitch(-2); };
        pitchIn.onClick = [this] { zoomPitch(2); };
        quantize.onClick = [this] {
            if (clip && editable && step() > 0)
            {
                auto changes = selectedNotes();
                for (auto& note : changes)
                    note.start = snapped(note.start);
                MidiNoteEditor(*clip).change(changes, "Quantize MIDI notes");
                notify();
            }
        };
        velocity.setName("Note velocity");
        velocity.setSliderStyle(ju::Slider::IncDecButtons);
        velocity.setTextBoxStyle(ju::Slider::TextBoxLeft, false, 38, 24);
        velocity.setRange(1.0, 127.0, 1.0);
        velocity.setTooltip("Type a MIDI velocity or use the minus and plus buttons");
        velocity.onValueChange = [this] {
            auto changes = selectedNotes();
            if (clip && editable && changes.size() == 1)
            {
                changes.front().velocity = ju::roundToInt(velocity.getValue());
                MidiNoteEditor(*clip).change(changes, "Set MIDI velocity");
            }
            notify();
        };
        horizontal.onMove = [this](double start) { grid.left = start; repaint(); };
        vertical.onMove = [this](double start) { grid.top = ju::roundToInt(start); repaint(); };
    }

    ~PianoRoll() override { stopAudition(); }

    void refresh(te::MidiClip* target, bool canEdit, bool focused)
    {
        if (target != clip)
        {
            stopAudition();
            cancelGesture();
            clip = target;
            selected.clear();
            if (clip)
            {
                const auto model = MidiNoteEditor(*clip);
                grid.left = cursor = model.range().getStart();
                const auto notes = model.notes();
                const auto middle = notes.empty() ? 60 : notes.front().pitch;
                grid.top = 127 - middle - pitchRows() / 2;
            }
        }
        editable = canEdit && clip;
        active = focused;
        if (!editable)
        {
            stopAudition();
            cancelGesture();
        }

        if (!isVisible())
            return;
        name.setText(clip ? clip->getName() : "Double-click a MIDI clip to edit it", ju::dontSendNotification);
        for (auto* child : std::array<ju::Component*, 8> {
                &mode, &snap, &zoomOut, &zoomIn, &pitchOut, &pitchIn, &horizontal, &vertical })
            child->setEnabled(clip != nullptr);
        const auto notes = selectedNotes();
        velocity.setEnabled(editable && notes.size() == 1);
        if (notes.size() == 1)
            velocity.setValue(notes.front().velocity, ju::dontSendNotification);
        quantize.setEnabled(editable && step() > 0 && notes.size() == 1);
        updateScrollbars();
        repaint();
    }

    void clearProject()
    {
        refresh(nullptr, false, false);
        clipboard.clear();
    }

    bool canApply(Action action) const
    {
        if (!editable || !clip)
            return false;
        const auto notes = selectedNotes();
        if (action == Action::paste)
            return !pasteNotes().empty();
        if (action == Action::copy || action == Action::cut || action == Action::remove)
            return !notes.empty();
        auto changes = actionChanges(action, notes);
        return !changes.empty() && changes != notes
            && std::all_of(changes.begin(), changes.end(), [&](const auto& note) {
                return action == Action::up || action == Action::down
                    ? note.pitch >= 0 && note.pitch <= 127 : MidiNoteEditor(*clip).valid(note);
            });
    }

    void apply(Action action)
    {
        if (!canApply(action))
            return;
        MidiNoteEditor model(*clip);
        const auto notes = selectedNotes();
        if (action == Action::copy || action == Action::cut)
        {
            clipboard = notes;
            for (auto& note : clipboard)
                note.id = note.id.createCopy();
        }
        if (action == Action::cut || action == Action::remove)
        {
            std::vector<ju::ValueTree> ids;
            for (const auto& note : notes)
                ids.push_back(note.id);
            if (model.remove(ids, action == Action::cut ? "Cut MIDI notes" : "Delete MIDI notes"))
                selected.clear();
        }
        else if (action == Action::paste)
            selected = model.insert(pasteNotes(), "Paste MIDI notes");
        else if (action != Action::copy)
            model.change(actionChanges(action, notes), "Adjust MIDI notes");
        notify();
    }

    std::function<void()> onFocus, onChange;

private:
    friend class MainGui;

    struct ScrollBar final : ju::ScrollBar, private ju::ScrollBar::Listener
    {
        explicit ScrollBar(bool vertical) : ju::ScrollBar(vertical) { addListener(this); }
        std::function<void(double)> onMove;
        void scrollBarMoved(ju::ScrollBar*, double start) override
        {
            if (onMove)
                onMove(start);
        }
    };

    enum class Gesture { none, draw, move, resize, box, addBox, velocity };
    using G = NoteGrid;

    void notify() { if (onChange) onChange(); }
    double step() const { return snap.getSelectedId() <= 1 ? 0.0 : std::ldexp(1.0, 2 - snap.getSelectedId()); }
    double snapped(double beat) const
    {
        if (const auto increment = step(); increment > 0)
        {
            const auto origin = MidiNoteEditor(*clip).range().getStart();
            return origin + std::round((beat - origin) / increment) * increment;
        }
        return beat;
    }
    bool contains(const ju::ValueTree& id) const { return std::find(selected.begin(), selected.end(), id) != selected.end(); }
    std::vector<GridNote> selectedNotes() const
    {
        if (!clip || selected.empty())
            return {};
        const auto model = MidiNoteEditor(*clip);
        const auto range = model.range();
        auto notes = model.notes();
        std::erase_if(notes, [&](const auto& note) {
            return !contains(note.id) || note.start >= range.getEnd() || note.end() <= range.getStart();
        });
        return notes;
    }
    std::vector<GridNote> pasteNotes() const
    {
        auto notes = clipboard;
        if (notes.empty())
            return {};
        const auto earliest = std::min_element(notes.begin(), notes.end(),
            [](const auto& a, const auto& b) { return a.start < b.start; })->start;
        for (auto& note : notes)
        {
            note.start += snapped(cursor) - earliest;
            if (!MidiNoteEditor(*clip).valid(note))
                return {};
        }
        return notes;
    }

    std::vector<GridNote> actionChanges(Action action, std::vector<GridNote> notes) const
    {
        if (action >= Action::startMinus && notes.size() != 1)
            return {};
        const auto increment = step() > 0 ? step() : 1.0 / 96.0;
        for (auto& note : notes)
            switch (action)
            {
                case Action::up: ++note.pitch; break;
                case Action::down: --note.pitch; break;
                case Action::left: note.start -= increment; break;
                case Action::right: note.start += increment; break;
                case Action::startMinus: note.start -= increment; note.length += increment; break;
                case Action::startPlus: note.start += increment; note.length -= increment; break;
                case Action::endMinus: note.length -= increment; break;
                case Action::endPlus: note.length += increment; break;
                default: break;
            }
        return notes;
    }

    ju::Rectangle<int> canvas() const { return getLocalBounds().withTrimmedTop(32); }
    int laneY() const { return std::max(G::ruler, canvas().getHeight() - G::scroll - G::lane); }
    int pitchRows() const { return std::max(1, (laneY() - G::ruler) / grid.row); }
    double visibleBeats() const { return std::max(1, getWidth() - G::keys - G::scroll) / grid.scale; }
    ju::Rectangle<int> noteArea() const { return { G::keys, G::ruler, std::max(0, getWidth() - G::keys - G::scroll), laneY() - G::ruler }; }
    void updateScrollbars()
    {
        if (!clip)
            return;
        const auto range = MidiNoteEditor(*clip).range();
        const auto width = visibleBeats();
        grid.left = ju::jlimit(range.getStart(), std::max(range.getStart(), range.getEnd() - width), grid.left);
        grid.top = ju::jlimit(0, std::max(0, 128 - pitchRows()), grid.top);
        horizontal.setRangeLimits(range.getStart(), std::max(range.getEnd(), range.getStart() + width), ju::dontSendNotification);
        horizontal.setCurrentRange(grid.left, width, ju::dontSendNotification);
        vertical.setRangeLimits(0, 128, ju::dontSendNotification);
        vertical.setCurrentRange(grid.top, pitchRows(), ju::dontSendNotification);
    }

    void zoomTime(double factor)
    {
        const auto middle = grid.left + visibleBeats() / 2;
        grid.scale = ju::jlimit(18.0, 640.0, grid.scale * factor);
        grid.left = middle - visibleBeats() / 2;
        updateScrollbars();
        repaint();
    }

    void zoomPitch(int delta)
    {
        const auto center = grid.top + pitchRows() / 2;
        grid.row = ju::jlimit(9, 30, grid.row + delta);
        grid.top = center - pitchRows() / 2;
        updateScrollbars();
        repaint();
    }

    void resized() override
    {
        auto header = getLocalBounds().removeFromTop(32).reduced(5, 3);
        for (auto [control, width] : std::array<std::pair<ju::Component*, int>, 8> {{
                 { &mode, 64 }, { &snap, 104 }, { &zoomOut, 56 }, { &zoomIn, 56 },
                 { &pitchOut, 64 }, { &pitchIn, 64 }, { &quantize, 82 }, { &velocity, 86 } }})
            control->setBounds(header.removeFromLeft(width).reduced(2, 0));
        name.setBounds(header.reduced(5, 0));
        auto area = canvas();
        horizontal.setBounds(area.removeFromBottom(G::scroll).withTrimmedLeft(G::keys).withTrimmedRight(G::scroll));
        vertical.setBounds(area.removeFromRight(G::scroll).withTrimmedTop(G::ruler).withTrimmedBottom(G::lane));
        updateScrollbars();
    }

    float velocityY(int value) const { return static_cast<float>(laneY() + G::lane - 4 - (value - 1) * (G::lane - 8) / 126.0); }
    ju::Rectangle<float> velocityHandle(const GridNote& note) const
    {
        return { grid.x(note.start) - 4, velocityY(note.velocity) - 4, 8, 8 };
    }

    void paint(ju::Graphics& g) override
    {
        g.fillAll(ju::Colour(0xff252525));
        if (!clip)
            return;
        g.setOrigin(0, canvas().getY());
        const auto area = noteArea();
        const auto model = MidiNoteEditor(*clip);
        const auto limits = model.range();
        const auto notes = model.notes();
        g.setFont(ju::FontOptions(11.0f));
        for (int index = grid.top; index < 128 && grid.y(127 - index) < laneY(); ++index)
        {
            const int pitch = 127 - index;
            const auto y = grid.y(pitch);
            const auto height = std::min(grid.row, laneY() - y);
            const bool black = ju::MidiMessage::isMidiNoteBlack(pitch);
            g.setColour(ju::Colour(black ? 0xff202831 : 0xff2a3440));
            g.fillRect(G::keys, y, area.getWidth(), height);
            g.setColour(ju::Colour(black ? 0xff282b32 : 0xffdce0e4));
            g.fillRect(0, y, G::keys - 1, height - 1);
            if (pitch % 12 == 0)
            {
                g.setColour(ju::Colour(0xff202831));
                g.drawText(ju::MidiMessage::getMidiNoteName(pitch, true, true, 4),
                           4, y, G::keys - 8, height, ju::Justification::centredRight);
            }
        }

        {
            ju::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(area.withTop(0).withBottom(laneY() + G::lane));
            const auto origin = limits.getStart();
            const auto& meter = clip->edit.tempoSequence.getTimeSigAt(clip->getPosition().getStart());
            const double bar = std::max(0.25, 4.0 * meter.numerator.get() / meter.denominator.get());
            const auto subdivision = step() > 0 && step() * grid.scale >= 7 ? step() : 1.0;
            for (double beat = origin + std::floor((grid.left - origin) / subdivision) * subdivision;
                 beat <= std::min(limits.getEnd(), grid.left + visibleBeats()); beat += subdivision)
            {
                g.setColour(ju::Colour(0xff3d4a58));
                g.drawVerticalLine(ju::roundToInt(grid.x(beat)), G::ruler, static_cast<float>(laneY() + G::lane));
            }
            for (double index = std::ceil((grid.left - origin) / bar); origin + index * bar <= std::min(limits.getEnd(), grid.left + visibleBeats()); ++index)
            {
                const auto x = ju::roundToInt(grid.x(origin + index * bar));
                g.setColour(ju::Colour(0xff637589));
                g.drawVerticalLine(x, 0, static_cast<float>(laneY() + G::lane));
                g.setColour(ju::Colours::white);
                g.drawText(ju::String(static_cast<int>(index) + 1), x + 4, 0, 42, G::ruler, ju::Justification::centredLeft);
            }
            g.setColour(ju::Colour(0xff191e26).withAlpha(0.8f));
            const auto end = ju::roundToInt(grid.x(limits.getEnd()));
            if (end < area.getRight())
                g.fillRect(end, 0, area.getRight() - end, laneY() + G::lane);
            g.setColour(ju::Colour(0xff70859a));
            g.drawVerticalLine(ju::roundToInt(grid.x(cursor)), 0, static_cast<float>(laneY() + G::lane));
            for (const bool selectedPass : { false, true })
                for (const auto& note : notes)
                    if (limits.contains(note.start) && contains(note.id) == selectedPass)
                        paintVelocity(g, previewFor(note));
        }

        g.setColour(ju::Colours::white);
        g.drawText("Velocity", 4, laneY(), G::keys - 8, G::lane, ju::Justification::centred);
        {
            ju::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(area.withRight(std::min(area.getRight(), ju::roundToInt(grid.x(limits.getEnd())))));
            for (const auto& note : notes)
                if (note.end() > limits.getStart() && note.start < limits.getEnd())
                    paintNote(g, previewFor(note));
            if (gesture == Gesture::draw && !preview.empty())
                paintNote(g, preview.front());
            if (!box.isEmpty())
            {
                g.setColour(ju::Colours::white.withAlpha(0.15f));
                g.fillRect(box);
                g.setColour(ju::Colours::white);
                g.drawRect(box);
            }
        }
        if (active)
        {
            g.setOrigin(0, 0);
            g.setColour(ju::Colours::white);
            g.drawRect(getLocalBounds(), 2);
        }
    }

    const GridNote& previewFor(const GridNote& note) const
    {
        const auto found = std::find_if(preview.begin(), preview.end(), [&](const auto& next) { return next.id == note.id; });
        return found == preview.end() ? note : *found;
    }

    void paintNote(ju::Graphics& g, const GridNote& note) const
    {
        const auto bounds = grid.bounds(note);
        g.setColour(ju::Colour(0xff387eb3).interpolatedWith(ju::Colour(0xff68b9ee), note.velocity / 127.0f));
        g.fillRoundedRectangle(bounds, 2);
        if (contains(note.id) || !note.id.isValid())
        {
            g.setColour(active ? ju::Colours::white : ju::Colour(0xffa9b8c5));
            g.drawRoundedRectangle(bounds.reduced(0.5f), 2, active ? 2.0f : 1.0f);
        }
    }

    void paintVelocity(ju::Graphics& g, const GridNote& note) const
    {
        const auto handle = velocityHandle(note);
        g.setColour(contains(note.id) ? ju::Colours::white : ju::Colour(0xff68b9ee));
        g.drawLine(handle.getCentreX(), handle.getCentreY(), handle.getCentreX(),
                   static_cast<float>(laneY() + G::lane - 3), 2);
        g.fillEllipse(handle);
    }

    void audition(int pitch, int strength)
    {
        if (clip && editable)
            if (auto* track = dynamic_cast<te::AudioTrack*>(clip->getTrack()))
            {
                stopAudition();
                clip->edit.getTransport().ensureContextAllocated();
                auditionTrack = track;
                auditionChannel = clip->getMidiChannel();
                track->playGuideNote(pitch, auditionChannel, strength, false, true, false);
                startTimer(120);
            }
    }

    bool onResizeEdge(const ju::Rectangle<float>& bounds, ju::Point<int> point, double end) const
    {
        return bounds.getWidth() >= 10 && point.x >= bounds.getRight() - 6
            && bounds.getRight() <= grid.x(end) + 0.01f;
    }

    void mouseMove(const ju::MouseEvent& event) override
    {
        auto cursorType = ju::MouseCursor::NormalCursor;
        const auto point = event.getPosition() - canvas().getPosition();
        if (clip && editable && noteArea().contains(point))
        {
            const auto model = MidiNoteEditor(*clip);
            const auto notes = model.notes();
            for (auto it = notes.rbegin(); it != notes.rend(); ++it)
            {
                const auto bounds = grid.bounds(*it);
                if (!bounds.contains(point.toFloat()))
                    continue;
                if (onResizeEdge(bounds, point, model.range().getEnd())
                    && (!contains(it->id) || selected.size() <= 1))
                    cursorType = ju::MouseCursor::LeftRightResizeCursor;
                break;
            }
        }
        setMouseCursor(cursorType);
    }

    void mouseDown(const ju::MouseEvent& event) override
    {
        if (!event.mods.isLeftButtonDown() && !event.mods.isRightButtonDown())
            return;
        if (isShowing())
            grabKeyboardFocus();
        if (onFocus)
            onFocus();
        cancelGesture();
        if (!clip)
            return;
        const auto point = event.getPosition() - canvas().getPosition();
        const auto right = event.mods.isRightButtonDown();
        const auto area = noteArea();
        const auto model = MidiNoteEditor(*clip);
        const auto limits = model.range();
        if (point.x < G::keys && point.y >= G::ruler && point.y < laneY() && !right)
        {
            audition(grid.pitch(static_cast<float>(point.y)), 96);
            return;
        }
        if (point.x < G::keys || point.x >= area.getRight() || !limits.contains(grid.beat(static_cast<float>(point.x))))
            return;
        anchor = point;
        cursor = grid.beat(static_cast<float>(point.x));
        auto notes = model.notes();
        const bool inLane = point.y >= laneY() && point.y < laneY() + G::lane;
        if (inLane)
            std::stable_partition(notes.begin(), notes.end(), [this](const auto& note) { return !contains(note.id); });
        for (auto it = notes.rbegin(); it != notes.rend(); ++it)
        {
            const auto& note = *it;
            const auto bounds = grid.bounds(note);
            if (!(area.contains(point) && bounds.contains(point.toFloat()))
                && !(inLane && limits.contains(note.start) && velocityHandle(note).expanded(3).contains(point.toFloat())))
                continue;
            if (right)
            {
                if (contains(note.id))
                    std::erase(selected, note.id);
                else
                    selected.push_back(note.id);
            }
            else
            {
                clicked = note;
                const bool resize = !inLane && onResizeEdge(bounds, point, limits.getEnd());
                if (!contains(note.id) || inLane)
                    selected = { note.id };
                gesture = inLane ? Gesture::velocity
                    : resize && selected.size() == 1 ? Gesture::resize : Gesture::move;
                originals = selectedNotes();
                if (!inLane)
                {
                    audition(note.pitch, note.velocity);
                    lastAudition = note.pitch;
                }
            }
            notify();
            return;
        }
        if (area.contains(point))
        {
            if (!right)
                selected.clear();
            boxSelection = selected;
            gesture = right ? Gesture::addBox : mode.getToggleState() ? Gesture::box : Gesture::draw;
            if (gesture == Gesture::draw && editable)
                updatePreview(point, false);
        }
        notify();
    }

    void updatePreview(ju::Point<int> point, bool dragged)
    {
        if (!clip)
            return;
        const auto model = MidiNoteEditor(*clip);
        const auto limits = model.range();
        if (gesture == Gesture::box || gesture == Gesture::addBox)
        {
            const auto gridBounds = noteArea().withRight(
                std::min(noteArea().getRight(), ju::roundToInt(grid.x(limits.getEnd()))));
            box = ju::Rectangle<int>(anchor, point).expanded(1).getIntersection(gridBounds);
            selected = boxSelection;
            for (const auto& note : model.notes())
                if (!contains(note.id) && note.end() > limits.getStart() && note.start < limits.getEnd()
                    && grid.bounds(note).intersects(box.toFloat()))
                    selected.push_back(note.id);
            return;
        }
        if (!editable)
            return;
        preview = originals;
        const auto raw = grid.beat(static_cast<float>(point.x));
        if (gesture == Gesture::draw)
        {
            const auto start = std::max(0.0, step() > 0
                ? limits.getStart() + std::floor((cursor - limits.getStart()) / step()) * step()
                : cursor);
            const auto length = dragged ? std::max(step(), snapped(raw) - start) : rememberedLength;
            preview = {{ {}, start, std::min(limits.getEnd() - start, std::max(MidiNoteEditor::minimumLength, length)),
                         grid.pitch(static_cast<float>(anchor.y)), 96 }};
        }
        else if (gesture == Gesture::move)
        {
            const auto shift = point.x == anchor.x ? 0.0 : snapped(clicked.start + (point.x - anchor.x) / grid.scale) - clicked.start;
            const auto pitches = grid.pitch(static_cast<float>(point.y)) - grid.pitch(static_cast<float>(anchor.y));
            for (auto& note : preview)
            {
                note.start += shift;
                note.pitch += pitches;
            }
        }
        else if (gesture == Gesture::resize)
            preview.front().length = std::min(limits.getEnd(), snapped(clicked.end() + (point.x - anchor.x) / grid.scale)) - clicked.start;
        else if (gesture == Gesture::velocity)
            preview.front().velocity = ju::jlimit(1, 127, ju::roundToInt(1.0 + 126.0 * (laneY() + G::lane - 4 - point.y) / (G::lane - 8)));
        if (!std::all_of(preview.begin(), preview.end(), [&](const auto& note) {
                if (gesture == Gesture::velocity || (gesture == Gesture::move && point.x == anchor.x))
                    return note.pitch >= 0 && note.pitch <= 127;
                return model.valid(note);
            }))
            preview.clear();
        if (gesture == Gesture::move && !preview.empty())
        {
            const auto& note = previewFor(clicked);
            if (note.pitch != lastAudition)
            {
                audition(note.pitch, note.velocity);
                lastAudition = note.pitch;
            }
        }
    }

    void mouseDrag(const ju::MouseEvent& event) override
    {
        if (gesture != Gesture::none && event.getDistanceFromDragStart() >= 4)
        {
            updatePreview(event.getPosition() - canvas().getPosition(), true);
            notify();
        }
    }

    void mouseUp(const ju::MouseEvent& event) override
    {
        if (!clip || gesture == Gesture::none)
            return;
        const bool dragged = event.getDistanceFromDragStart() >= 4;
        if (dragged)
            updatePreview(event.getPosition() - canvas().getPosition(), true);
        if (editable && !preview.empty())
        {
            MidiNoteEditor model(*clip);
            if (gesture == Gesture::draw)
            {
                selected = model.insert(preview, "Draw MIDI note");
                if (!selected.empty())
                {
                    if (dragged)
                        rememberedLength = preview.front().length;
                    audition(preview.front().pitch, preview.front().velocity);
                }
            }
            else if (dragged)
            {
                const auto label = gesture == Gesture::velocity ? "Set MIDI velocity"
                    : gesture == Gesture::resize ? "Resize MIDI note" : "Move MIDI notes";
                model.change(preview, label);
            }
        }
        if (!dragged && (gesture == Gesture::move || gesture == Gesture::resize))
            selected = { clicked.id };
        cancelGesture();
        notify();
    }

    void mouseWheelMove(const ju::MouseEvent& event, const ju::MouseWheelDetails& wheel) override
    {
        if (!clip)
            return;
        if (wheel.deltaX != 0 || event.mods.isShiftDown())
            grid.left -= (wheel.deltaX != 0 ? wheel.deltaX : wheel.deltaY) * visibleBeats();
        else
            grid.top -= ju::roundToInt(wheel.deltaY * 12);
        updateScrollbars();
        repaint();
    }

    void timerCallback() override { stopAudition(); }

    void stopAudition()
    {
        stopTimer();
        if (auditionTrack)
        {
            auditionTrack->turnOffGuideNotes(auditionChannel);
            auditionTrack = nullptr;
        }
    }

    void visibilityChanged() override
    {
        if (!isVisible())
        {
            stopAudition();
            cancelGesture();
        }
    }

    void cancelGesture()
    {
        gesture = Gesture::none;
        originals.clear();
        preview.clear();
        boxSelection.clear();
        clicked = {};
        box = {};
        lastAudition = -1;
    }

    te::MidiClip* clip = nullptr;
    te::AudioTrack::Ptr auditionTrack;
    te::MidiChannel auditionChannel { 1 };
    bool editable = false, active = false;
    NoteGrid grid;
    Gesture gesture = Gesture::none;
    GridNote clicked {};
    ju::Point<int> anchor;
    ju::Rectangle<int> box;
    double cursor = 0.0, rememberedLength = 0.25;
    int lastAudition = -1;
    std::vector<ju::ValueTree> selected, boxSelection;
    std::vector<GridNote> originals, preview, clipboard;
    ju::Label name;
    ju::TextButton mode { "DRAW" }, zoomOut { "TIME −" }, zoomIn { "TIME +" },
        pitchOut { "PITCH −" }, pitchIn { "PITCH +" }, quantize { "QUANTIZE" };
    ju::ComboBox snap;
    ju::Slider velocity;
    ScrollBar horizontal { false }, vertical { true };
};
