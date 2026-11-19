# LLVM/MinGW-w64 Windows qualification

The MSVC workflow remains the supported Windows build. A separate Windows workflow tests the pinned native x64 UCRT build of LLVM/MinGW-w64 release 20251118, with a SHA-256 check before installation. The toolchain release builds LLVM 21.1.6 and MinGW-w64 at `f6a7b25bc70e9c3f5d0a8535ad112b08de564b33`. It builds and launches a small statically linked C++ probe with a thread and an exception, then tries to configure SimpleDAW from the same archived sources used by the MSVC build. [LLVM/MinGW release](https://github.com/mstorsjo/llvm-mingw/releases/tag/20251118) · [LLVM build script](https://github.com/mstorsjo/llvm-mingw/blob/20251118/build-llvm.sh) · [MinGW-w64 build script](https://github.com/mstorsjo/llvm-mingw/blob/20251118/build-mingw-w64.sh)

## Compatibility

Pinned JUCE 8.0.6 rejects Windows compilers defining `__MINGW32__` with the error `MinGW is not supported. Please use an alternative compiler.` Clang targeting MinGW uses this macro. JUCE removed MinGW support in version 8. The qualification leaves the pinned sources and their support checks in place. Clang in MSVC ABI mode, such as clang-cl, is a different build configuration and by itself does not replace the Microsoft runtime. [Pinned JUCE guard](https://github.com/juce-framework/JUCE/blob/19edd538429c93d277bf95b55aaa7e3eb545f951/modules/juce_core/system/juce_TargetPlatform.h#L112-L114) · [JUCE support discussion](https://forum.juce.com/t/cmake-windows/64753/2)

## Windows results

The Windows x64 probe built with `-static` and ran with PATH restricted to System32. Its PE import table listed `KERNEL32.dll` and nine Windows UCRT API-set DLLs. It imported no app-local LLVM, MinGW or Visual C++ redistributable DLLs. The linked probe is a toolchain check, so its import table does not measure SimpleDAW's imports.

The matching archive was generated and extracted successfully. CMake reached JUCE's nested `juceaide` build using Clang in MinGW mode and hit the platform guard. The same compilation also reported pointer-width errors and missing Windows graphics and accessibility declarations. SimpleDAW and Tracktion were never linked, so there is no alternative application binary or application startup, smoke test or import result. [Windows diagnostic run](https://github.com/mlzion/SimpleDAW/actions/runs/44471557555)

## Runtime licences

- LLVM's libc++, libc++abi, libunwind and compiler-rt use Apache-2.0 with LLVM exceptions. LLVM's policy describes the exception for code embedded in compiler output. Preserve the pinned runtime licence and notices when packaging runtime materials. [LLVM policy](https://llvm.org/docs/DeveloperPolicy.html#llvm-license) · [LLVM 21.1.6 runtime licence](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.6/libcxx/LICENSE.TXT)
- MinGW-w64's runtime contains components under several terms. Its upstream instructions recommend including the exact runtime notice bundle for binaries built with that toolchain. Review the actual linked components and any file-specific terms before distribution. The LLVM/MinGW build and wrapper scripts have their own ISC notice. [Pinned MinGW runtime notices](https://github.com/mingw-w64/mingw-w64/blob/f6a7b25bc70e9c3f5d0a8535ad112b08de564b33/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt) · [LLVM/MinGW packaging licence](https://github.com/mstorsjo/llvm-mingw/blob/20251118/LICENSE.txt)
- The UCRT variant uses the Windows system UCRT through imports. Windows 11 provides it. This differs from embedding static UCRT or the MSVC compiler runtime in the executable. Check the final binary's PE imports, rather than inferring dependencies from the compiler choice. [Microsoft UCRT deployment](https://learn.microsoft.com/en-us/cpp/windows/universal-crt-deployment)

No LLVM/MinGW application payload is cleared for publication. The probe only characterizes a representative runtime and cannot establish the full application's linked libraries. A supported JUCE/Tracktion combination for this toolchain and a passing archived-source application build are required before releasing an alternative executable.
