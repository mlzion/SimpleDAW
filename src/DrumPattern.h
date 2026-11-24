// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

struct DrumStep
{
    int pitch, index;
    auto operator<=>(const DrumStep&) const = default;
};

class DrumPattern
{
public:
    static constexpr double step = 0.25, subdivision = step / 4.0;
    static constexpr int maxVisibleSteps = 32, defaultVelocity = 100;

    DrumPattern(te::MidiClip& clip, MidiNoteEditor& editor)
        : range(editor.range()), model(editor), tempo(clip.edit.tempoSequence),
          origin(clip.getContentStartBeat().inBeats() - (clip.isLooping() ? range.getStart() : 0.0)),
          firstQuarter(quarterForSource(range.getStart()))
    {
        for (auto note : editor.notes())
            if (note.start >= range.getStart() - epsilon && note.start < range.getEnd() - epsilon)
            {
                const auto index = indexAt(note.start);
                hitsByStep[{ note.pitch, index }].push_back(std::move(note));
            }
    }

    static double quarterAtTime(const te::TempoSequence& tempo, te::TimePosition time)
    {
        te::tempo::Sequence::Position position(tempo.getInternalSequence());
        position.set(time);
        return position.getPPQTime();
    }

    static te::TimePosition timeAtQuarter(const te::TempoSequence& tempo, double quarter)
    {
        te::tempo::Sequence::Position position(tempo.getInternalSequence());
        position.setPPQTime(quarter);
        return position.getTime();
    }

    int count() const
    {
        return ju::jmax(0, static_cast<int>(std::ceil((quarterForSource(range.getEnd()) - firstQuarter) / step - epsilon)));
    }

    int indexAt(double sourceBeat) const
    {
        return static_cast<int>(std::floor((quarterForSource(sourceBeat) - firstQuarter + epsilon) / step));
    }

    double start(DrumStep cell, int subdivisionIndex = 0) const
    {
        const auto quarter = firstQuarter + cell.index * step + subdivisionIndex * subdivision;
        return tempo.toBeats(timeAtQuarter(tempo, quarter)).inBeats() - origin;
    }

    int slot(DrumStep cell, double sourceBeat) const
    {
        const auto offset = quarterForSource(sourceBeat) - firstQuarter - cell.index * step;
        return ju::jlimit(0, 3, static_cast<int>(std::floor((offset + epsilon) / subdivision)));
    }

    const std::vector<GridNote>& hits(DrumStep cell) const
    {
        const auto existing = hitsByStep.find(cell);
        static const std::vector<GridNote> empty;
        return existing != hitsByStep.end() ? existing->second : empty;
    }

    bool toggle(DrumStep cell)
    {
        if (cell.index < 0 || cell.index >= count())
            return false;
        const auto& existing = hits(cell);
        if (existing.empty())
            return insert(cell, defaultVelocity);

        std::vector<ju::ValueTree> ids;
        for (const auto& note : existing)
            ids.push_back(note.id);
        return model.remove(ids, "Remove drum hit");
    }

    bool toggleSubdivision(DrumStep cell, int index, int velocity)
    {
        if (cell.index < 0 || cell.index >= count() || !ju::isPositiveAndBelow(index, 4))
            return false;
        const auto& existing = hits(cell);
        std::vector<ju::ValueTree> ids;
        for (const auto& note : existing)
            if (slot(cell, note.start) == index)
                ids.push_back(note.id);
        if (!ids.empty())
            return model.remove(ids, "Remove drum subdivision");

        const auto onset = start(cell, index);
        const auto parentVelocity = existing.empty() ? velocity : existing.front().velocity;
        const GridNote added { {}, onset, std::min(start(cell, index + 1), range.getEnd()) - onset, cell.pitch, parentVelocity };
        std::vector<GridNote> changes;
        for (auto hit : existing)
        {
            const auto end = std::min(range.getEnd(), start(cell, slot(cell, hit.start) + 1));
            const auto length = end - hit.start;
            if (length >= MidiNoteEditor::minimumLength && hit.length > length + epsilon)
                hit.length = length;
            hit.velocity = parentVelocity;
            changes.push_back(hit);
        }
        return model.change(changes, "Add drum subdivision", { added });
    }

    bool velocity(const std::set<DrumStep>& selected, int amount, bool absolute = false)
    {
        std::vector<GridNote> updates;
        for (auto cell : selected)
        {
            auto group = hits(cell);
            if (group.empty())
                continue;
            const auto value = ju::jlimit(1, 127, absolute ? amount : group.front().velocity + amount);
            for (auto& note : group)
            {
                note.velocity = value;
                updates.push_back(note);
            }
        }
        return model.change(updates, "Change drum velocity");
    }

    bool remove(const std::set<DrumStep>& selected)
    {
        std::vector<ju::ValueTree> ids;
        for (auto cell : selected)
            for (const auto& note : hits(cell))
                ids.push_back(note.id);
        return model.remove(ids, "Delete drum hits");
    }

    const ju::Range<double> range;

    double quarterForSource(double beat) const
    {
        return quarterAtTime(tempo, tempo.toTime(te::BeatPosition::fromBeats(origin + beat)));
    }

    double sourceAtQuarter(double quarter) const
    {
        return tempo.toBeats(timeAtQuarter(tempo, quarter)).inBeats() - origin;
    }

private:
    bool insert(DrumStep cell, int velocity)
    {
        const auto at = start(cell);
        const auto length = std::min(start(cell, 4), range.getEnd()) - at;
        return !model.insert({ { {}, at, length, cell.pitch, velocity } }, "Add drum hit").empty();
    }

    static constexpr double epsilon = 1.0e-8;
    MidiNoteEditor& model;
    const te::TempoSequence& tempo;
    const double origin, firstQuarter;
    std::map<DrumStep, std::vector<GridNote>> hitsByStep;
};
