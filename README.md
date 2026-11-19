# SimpleDAW

SimpleDAW is a beginner-friendly recording workstation. Phase 1 is a small Windows 11 feasibility prototype with a transport, track selector, and device controls.

## Build on Windows 11

Install Git, CMake 3.22 or newer, and Visual Studio 2022 or Build Tools with **Desktop development with C++**, MSVC v143, and the Windows SDK. Run these commands in PowerShell from the repository root. CMake downloads Tracktion Engine v3.2.0 and the JUCE revision pinned by that release on the first configure.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
.\build\SimpleDAW_artefacts\Release\SimpleDAW.exe
```

The default build uses JUCE's Windows audio backends, including WASAPI. For ASIO, obtain the [Steinberg ASIO SDK](https://www.steinberg.net/developers/) separately and install the manufacturer's x64 ASIO driver. Add `-DSIMPLEDAW_ASIO_SDK=C:/path/to/asiosdk/common` to the configure command, using the directory containing `iasiodrv.h`. Rebuild and choose ASIO in **Devices**.

## Try the prototype

1. Open **Devices** to choose the backend, input/output devices, active channels, sample rate, and buffer size. Connect a MIDI keyboard before launch if you want to test MIDI.
2. Create a project in a writable folder. Use the track selector and **Add track** to select or create tracks.
3. Import a WAV at the start of a track, or choose **Arm audio input**, select a channel or pair, press **Record**, then **Stop**. Use **Play from start** to hear the tracks together. Selecting an armed input again on the same track disarms it.
4. For MIDI, add the built-in synth to a separate track and choose **Arm MIDI input**. Record a few notes and play them from the start.
5. Use **Add reverb** or **Load VST3** on the selected track. VST3 scanning inspects the selected installed file or bundle in the application process.
6. Save, reopen the project, then export to a new stereo WAV. Keep recorded files and imported source files available at their original paths. Save explicitly before closing.

The Windows workflow builds the standard configuration and runs a generated-audio smoke test. See [feasibility](docs/feasibility.md) for verification status, [architecture](docs/architecture.md) for the engine boundaries, and [licensing](docs/licensing.md) before sharing executable builds.
