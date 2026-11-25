// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace simpleDaw
{
class DrumVoices
{
public:
    void prepare(double rate)
    {
        fadeSamples = std::max(1, static_cast<int>(std::round(rate * 0.003)));
        reset();
    }

    void reset()
    {
        voices = {};
        activeCount = 0;
    }

    bool isActive() const { return activeCount > 0; }

    void release()
    {
        for (auto& voice : voices)
            if (voice.active && voice.releaseRemaining == 0)
                voice.releaseRemaining = fadeSamples;
    }

    void trigger(float velocity, bool overlap)
    {
        if (!overlap)
            release();

        // Reserve bounded polyphony without cutting an active voice mid-waveform.
        for (auto& voice : voices)
            if (!voice.active)
            {
                voice = { 0.0, velocity, 0, true };
                ++activeCount;
                break;
            }
    }

    template<class ReadSample>
    void render(float& left, float& right, int length, double increment, ReadSample read)
    {
        for (auto& voice : voices)
        {
            if (!voice.active)
                continue;

            const auto index = static_cast<int>(voice.position);
            const auto next = std::min(index + 1, length - 1);
            const auto fraction = static_cast<float>(voice.position - index);
            const auto naturalEnd = std::min(1.0, (length - voice.position) / (increment * fadeSamples));
            const auto releaseGain = voice.releaseRemaining > 0
                ? static_cast<float>(voice.releaseRemaining) / fadeSamples : 1.0f;
            const auto gain = voice.velocity * releaseGain * static_cast<float>(naturalEnd);
            left += std::lerp(read(0, index), read(0, next), fraction) * gain;
            right += std::lerp(read(1, index), read(1, next), fraction) * gain;
            voice.position += increment;
            if (voice.position >= length || (voice.releaseRemaining > 0 && --voice.releaseRemaining == 0))
            {
                voice.active = false;
                --activeCount;
            }
        }
    }

private:
    struct Voice
    {
        double position = 0;
        float velocity = 0;
        int releaseRemaining = 0;
        bool active = false;
    };

    std::array<Voice, 64> voices {};
    int fadeSamples = 1, activeCount = 0;
};
}
