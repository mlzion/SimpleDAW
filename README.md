# SimpleDAW

SimpleDAW is a beginner-friendly recording workstation. Phase 1 is a small Windows 11 feasibility prototype with a transport, track selector, and device controls.

SimpleDAW is free and open-source software. Its original code is released under the GNU Affero General Public License, version 3. See [LICENSE](LICENSE), [COPYRIGHT](COPYRIGHT), and the [third-party notices](THIRD_PARTY_NOTICES.md).

## Download and run on Windows x64

1. Open a successful feature-branch run of the Windows workflow. Under **Artifacts**, download `SimpleDAW-Windows-x64`. GitHub may ask you to sign in. [Windows workflow runs](https://github.com/mlzion/SimpleDAW/actions/workflows/windows.yml?query=branch%3Afeature%2Fphase1-audio-engine)
2. Install Microsoft's x64 Visual C++ v14 Redistributable if a compatible version is absent. [Official Microsoft x64 download](https://aka.ms/vc14/vc_redist.x64.exe)
3. Extract `SimpleDAW.exe` to any folder you can read and execute from, then double-click it. SimpleDAW has no installer. Preferences live under your application-data folder. Projects and recorded media need a writable project location.

The download also contains the exact corresponding source archive, build instructions, revision list and notices. Keep the source archive if you want to rebuild this exact version. The distributed build uses `/MD`, the system runtime, and Windows audio backends. It can host separately installed x64 VST3 plugins. [Licence and distribution details](docs/licensing.md)

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

The Windows workflow runs a generated-audio smoke test, inspects the executable's DLL imports, and launches a copy from a write-denied directory with a separate working directory. The hosted runner has Visual Studio. See the feasibility notes for clean-machine and hardware testing still needed. See [feasibility](docs/feasibility.md) for verification status, [architecture](docs/architecture.md) for the engine boundaries, and [licensing](docs/licensing.md) before sharing executable builds.
