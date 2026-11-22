// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

namespace simpleDaw
{
inline const juce::Identifier replaceOverlapProperty { "simpleDawReplaceOverlap" };

inline const juce::Identifier overlapPlacementProperty { "simpleDawOverlapPlacement" };

inline juce::int64 overlapPlacement(const tracktion::Clip& clip)
{
    return static_cast<juce::int64>(clip.state.getProperty(overlapPlacementProperty,
        static_cast<juce::int64>(clip.itemID.getRawID())));
}

inline bool replacesAudio(const tracktion::Clip& clip)
{
    return static_cast<bool>(clip.state.getProperty(replaceOverlapProperty, false));
}

inline std::vector<tracktion::TimeRange> suppressedAudioRanges(
    const tracktion::AudioClipBase& clip, const juce::Array<tracktion::Clip*>& clips,
    const juce::Array<tracktion::Clip*>* allowedClips = nullptr)
{
    std::vector<tracktion::TimeRange> ranges;
    const auto position = clip.getPosition().time;
    const auto placement = overlapPlacement(clip);
    for (auto* candidate : clips)
    {
        auto* other = dynamic_cast<tracktion::AudioClipBase*>(candidate);
        if (!other || other == &clip || other->isMuted() || !replacesAudio(*other)
            || (allowedClips && !allowedClips->contains(other)))
            continue;
        const auto range = other->getPosition().time;
        if (!position.overlaps(range))
            continue;
        if (replacesAudio(clip) && (range.getStart() < position.getStart()
            || (range.getStart() == position.getStart() && overlapPlacement(*other) <= placement)))
            continue;
        ranges.push_back({ std::max(position.getStart(), range.getStart()),
                           std::min(position.getEnd(), range.getEnd()) });
    }
    return ranges;
}
}
