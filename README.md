# SimpleDAW

SimpleDAW is a beginner-friendly recording workstation. v0.24.1 adds Piano Roll MIDI note editing to the Windows x64 workstation.

SimpleDAW is free and open-source software. Its original code is released under the GNU Affero General Public License, version 3. See [LICENSE](LICENSE), [COPYRIGHT](COPYRIGHT), and the [third-party notices](THIRD_PARTY_NOTICES.md).

## Download and run on Windows x64

1. Download the accepted Windows x64 Release `/MD` ZIP. GitHub may ask you to sign in. [SimpleDAW-v0.24.1-Windows-x64.zip](https://github.com/mlzion/SimpleDAW/actions/runs/45291914568/artifacts/14875361245)
2. Install Microsoft's x64 Visual C++ v14 Redistributable if a compatible version is absent. [Official Microsoft x64 download](https://aka.ms/vc14/vc_redist.x64.exe)
3. Extract the ZIP and open `SimpleDAW-v0.24.1.exe`. SimpleDAW has no installer. Preferences live under your application-data folder. Projects and recorded media need a writable project location.

The download also contains the exact corresponding source archive, build instructions, revision list and notices. Keep the source archive if you want to rebuild this exact version. The distributed build uses `/MD`, the system runtime, and Windows audio backends. It can host separately installed x64 VST3 plugins. [Licence and distribution details](docs/licensing.md)

## Build on Windows 11

Install Git, CMake 3.22 or newer, and Visual Studio 2022 or Build Tools with **Desktop development with C++**, MSVC v143, and the Windows SDK. Run these commands in PowerShell from the repository root. CMake downloads Tracktion Engine v3.2.0 and the JUCE revision pinned by that release on the first configure.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
$release = Get-Content build/SimpleDAW-Release.json -Raw | ConvertFrom-Json
& $release.executable
```

The default build uses JUCE's Windows audio backends, including WASAPI. For ASIO, obtain the [Steinberg ASIO SDK](https://www.steinberg.net/developers/) separately and install the manufacturer's x64 ASIO driver. Add `-DSIMPLEDAW_ASIO_SDK=C:/path/to/asiosdk/common` to the configure command, using the directory containing `iasiodrv.h`. Rebuild and choose ASIO in **Devices**.

## Try v0.24.1

1. Open **Options → Audio and MIDI Devices** to choose an interface, active channels, sample rate, and buffer size.
2. Create a project from **File → New**. Use **Insert → Audio Track** or **MIDI Track** and click a track header to select it.
3. Use **File → Import Audio** to place a WAV at the snapped edit cursor on the selected track. For recording, choose **Options → Arm Audio Input**, select an input, then press **Record** and **Stop**. Choose a checked input again to remove its assignment from the selected track. A physical input can feed multiple armed tracks.
4. For MIDI, add the built-in synth from **Insert** and arm the controller through **Options**. Use **Insert → Add Reverb** or **Load VST3** for effects on the selected track.
5. Use the ruler to position the edit cursor. Drag across the ruler to set a time range for Loop or Time-Selection Punch. Space plays or stops, R records, and Ctrl+S saves. Timeline Copy, Paste and Delete are available in the toolbar for single-clip editing while stopped. The Delete key removes a selected clip or clears the ruler range when Timeline has focus with no selected clips. Space and Delete stay with the editor while typing.
6. Save after recording to retain the new items in the project. New, Open and Exit prompt if there are unsaved edits. Keep recorded and imported media at their original paths. Use **File → Export Audio** to render a new stereo WAV.

Windows x64 development, acceptance and distribution use `/MD`. The manually dispatched Windows workflow requires explicit approval and runs CTest, inspects DLL imports, and checks isolated startup. Linux CTests remain available through CMake. [Windows build policy](docs/build/windows-build-policy.md) See the [v0.24.1 acceptance record](docs/research/piano-roll-p0.md#acceptance) and [project status](PROJECT_STATUS.md).
