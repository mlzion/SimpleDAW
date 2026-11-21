# Windows x64 with the dynamic MSVC runtime

Windows x64 development, acceptance and distribution use `/MD`. After explicit authorization, the manually dispatched workflow builds from the matching source archive with Visual Studio 2022, runs CTest, inspects PE imports and verifies isolated startup. A hosted runner's successful launch does not establish that the runtime is available on a clean Windows installation. [Windows build policy](build/windows-build-policy.md) · [Archived-source build instructions](corresponding-source.md)

## Requested /MT diagnostics

Use a fresh local build directory. The verifier defaults to `/MD`, so select `MT` explicitly for this diagnostic.

```powershell
cmake -S . -B build-mt -G "Visual Studio 17 2022" -A x64 -DSIMPLEDAW_MSVC_DYNAMIC_RUNTIME=OFF
cmake --build build-mt --config Release --parallel 2
ctest --test-dir build-mt -C Release --output-on-failure
./scripts/Verify-Portable.ps1 -BuildDirectory build-mt -MsvcRuntime MT
```

## Operating-system and runtime prerequisites

Microsoft ships the Universal C Runtime (UCRT) as a Windows 10 and 11 system component. The Visual C++ v14 C++ standard library and compiler runtime are separately serviced. A Windows machine can already have them from another application or Visual Studio. An `/MD` user needs Microsoft's supported x64 VC++ v14 Redistributable at least as recent as the build tools if a compatible version is absent. The EXE can be moved as one file after that prerequisite is installed. On a clean test machine, install the x64 package from Microsoft before running the `/MD` startup check. The verification script reports the installed versions of the Visual C++ DLLs it finds. [UCRT deployment](https://learn.microsoft.com/en-us/cpp/windows/universal-crt-deployment) · [Supported VC++ Redistributables](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/) · [Microsoft x64 download](https://aka.ms/vc14/vc_redist.x64.exe)

## Publication terms

**The qualified form of this path is an EXE-only download with a separately installed Microsoft runtime.** Microsoft's redistribution documentation limits repackaging its redistributable package, merge modules and binaries to licensed Visual Studio users under the product terms and the REDIST list. The SimpleDAW artifact carries its own executable and source materials. Users who need the runtime get it from Microsoft and use it under Microsoft's runtime terms. Do not include Microsoft's installer or DLLs in the SimpleDAW download. [Microsoft redistribution conditions](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files) · [Visual Studio 2022 REDIST list](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution#visual-c-runtime-files) · [Microsoft v14 Runtime terms](https://visualstudio.microsoft.com/license-terms/vs2026-ga-visualcpp-v14-redist-runtime/)

The FSF's Windows guidance treats the dynamically linked Visual C++ runtime as a GPLv3 System Library when supplied separately. AGPLv3 §1 uses the same System Library and Corresponding Source definitions. The matching archive therefore needs the exact SimpleDAW, JUCE, Tracktion and other included source, with build scripts and licences, while the system runtime is separately obtained from Microsoft. Under AGPLv3/GPLv3 §6(d), publish equivalent no-charge access to the matching archive and clear retrieval directions at the binary download location. Include the project and third-party notices, and preserve the source-based SoundTouch relinking instructions. The existing archive pipeline can produce this material. [FSF Windows runtime guidance](https://www.gnu.org/licenses/gpl-faq.html#WindowsRuntimeAndGPL) · [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html) · [Third-party notices](../THIRD_PARTY_NOTICES.md)

Publishing is limited to the `/MD` variant.
