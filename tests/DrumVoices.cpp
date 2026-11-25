// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#include "DrumVoices.h"
#include <cstdio>

int main()
{
    simpleDaw::DrumVoices voices;
    voices.prepare(48000);
    const auto render = [&] {
        float left = 0, right = 0;
        voices.render(left, right, 1000, 1, [](int, int) { return 1.0f; });
        return left;
    };
    const auto check = [](bool passes, const char* message) {
        if (!passes)
            std::fprintf(stderr, "%s\n", message);
        return passes;
    };

    voices.trigger(0.25f, true);
    bool passed = check(render() == 0.25f, "Velocity must scale gain");
    voices.trigger(0.5f, true);
    passed &= check(render() == 0.75f, "Overlap must sum independent voices");
    voices.trigger(0.125f, false);
    passed &= check(render() == 0.875f, "Retrigger should start a release at full previous level");
    for (int i = 1; i < 144; ++i)
        render();
    passed &= check(render() == 0.125f, "Retrigger should release previous voices over 3 ms");
    voices.release();
    passed &= check(render() == 0.125f, "Stop should start a release at full previous level");
    for (int i = 1; i < 144; ++i)
        render();
    passed &= check(render() == 0.0f, "Stop should terminate all voices");

    voices.trigger(1.0f, false);
    float last = 0;
    for (int i = 0; i < 1000; ++i)
        last = render();
    passed &= check(last > 0 && last < 0.01f && render() == 0.0f, "One shot must fade and end");
    return passed ? 0 : 1;
}
