// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstdint>

namespace simpleDaw
{
class MidiPlaybackNotes
{
public:
    MidiPlaybackNotes()
    {
        for (auto& channel : pitchedNotes)
            channel.fill(-1);
        for (auto& channel : pitchOwners)
            channel.fill(-1);
    }

    template<class SendNoteOff>
    bool map(juce::MidiMessage& message, int transpose, int velocity, SendNoteOff&& sendNoteOff)
    {
        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            pitchedNotes[message.getChannel() - 1].fill(-1);
            pitchOwners[message.getChannel() - 1].fill(-1);
            return true;
        }

        if (!message.isNoteOn() && !message.isNoteOff() && !message.isAftertouch())
            return true;

        const auto channel = message.getChannel() - 1;
        const auto sourcePitch = message.getNoteNumber();
        auto& activePitch = pitchedNotes[channel][sourcePitch];

        if (message.isNoteOn())
        {
            if (velocity == 0)
                return false;

            const auto outputPitch = juce::jlimit(0, 127, sourcePitch + transpose);
            if (activePitch >= 0 && activePitch != outputPitch)
            {
                auto off = juce::MidiMessage::noteOff(channel + 1, activePitch);
                off.setTimeStamp(message.getTimeStamp());
                sendNoteOff(std::move(off));
                pitchOwners[channel][activePitch] = -1;
            }

            auto& owner = pitchOwners[channel][outputPitch];
            if (owner >= 0 && owner != sourcePitch)
                pitchedNotes[channel][owner] = -1;
            owner = static_cast<std::int8_t>(sourcePitch);
            activePitch = static_cast<std::int8_t>(outputPitch);
            const auto scaled = juce::jlimit(1, 127, (message.getVelocity() * velocity + 50) / 100);
            message.setVelocity(static_cast<float>(scaled) / 127.0f);
        }
        if (activePitch < 0)
            return false;

        message.setNoteNumber(activePitch);
        if (message.isNoteOff())
        {
            pitchOwners[channel][activePitch] = -1;
            activePitch = -1;
        }
        return true;
    }

private:
    std::array<std::array<std::int8_t, 128>, 16> pitchedNotes, pitchOwners;
};
}
