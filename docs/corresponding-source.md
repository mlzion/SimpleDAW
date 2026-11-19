# Build from the matching Windows source archive

The source archive contains three top-level source directories, `SimpleDAW`, `JUCE` and `Tracktion`, and a `SOURCE-REVISIONS.txt` file that records their exact Git commits. The SimpleDAW tree contains its licence, third-party notices, build scripts and tests. JUCE and Tracktion include the bundled dependency sources and notices. A copy of these instructions is included as `BUILD.md` at the archive root.

The Windows workflow creates the archive from the exact three Git checkouts, extracts it into a fresh directory, and uses the extracted sources to build and run the Windows tests. Its run summary records the revisions and SHA-256 checksums for the source archive and executable. The workflow keeps these files on the runner pending release clearance.

## Rebuild on Windows

Install Git if creating an archive from checkouts. To build from an extracted archive, install CMake 3.22 or newer, Visual Studio 2022 with **Desktop development with C++**, the x64 MSVC v143 toolset and the Windows SDK. Run in PowerShell from the archive root.

```powershell
cmake -S SimpleDAW -B build -G "Visual Studio 17 2022" -A x64 `
    -DSIMPLEDAW_ASIO_SDK:PATH= -DFETCHCONTENT_FULLY_DISCONNECTED=ON `
    "-DFETCHCONTENT_SOURCE_DIR_JUCE=$PWD/JUCE" `
    "-DFETCHCONTENT_SOURCE_DIR_TRACKTION=$PWD/Tracktion"
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
./SimpleDAW/scripts/Verify-Portable.ps1 -Executable build/SimpleDAW_artefacts/Release/SimpleDAW.exe
```

To replace SoundTouch, work on a copy of the archive, modify or replace its compatible sources under `Tracktion/modules/tracktion_engine/3rd_party/soundtouch`, and rerun the commands in a fresh build directory. Tracktion compiles the bundled source into the executable. The complete application source and scripts in the archive allow rebuilding and relinking with that modified version while retaining a single EXE.

For a future binary release, publish the matching source archive and the applicable notices in the same download location, with equivalent no-charge access for recipients and clear directions. Use a durable feature-branch commit or release tag to identify the SimpleDAW revision. The EXE archive can contain one file. [AGPLv3 §6](https://www.gnu.org/licenses/agpl-3.0.en.html#section6)

## Attempt the LLVM/MinGW build

Install Ninja and run the pinned installer script from the archive root in PowerShell. This is a qualification command. Pinned JUCE rejects the MinGW target during configure. See the compatibility notes in `SimpleDAW/docs/llvm-mingw.md`.

```powershell
$bin = ./SimpleDAW/scripts/Install-LlvmMinGW.ps1 -Destination "$env:TEMP/SimpleDAW-toolchain"
$env:PATH = "$bin;$env:PATH"
$env:CC = (Get-Command x86_64-w64-mingw32-clang.exe).Source
$env:CXX = (Get-Command x86_64-w64-mingw32-clang++.exe).Source
$env:RC = (Get-Command x86_64-w64-mingw32-windres.exe).Source
cmake -S SimpleDAW -B clang-build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS=-static `
    -DSIMPLEDAW_ASIO_SDK:PATH= -DFETCHCONTENT_FULLY_DISCONNECTED=ON `
    "-DFETCHCONTENT_SOURCE_DIR_JUCE=$PWD/JUCE" `
    "-DFETCHCONTENT_SOURCE_DIR_TRACKTION=$PWD/Tracktion"
```
