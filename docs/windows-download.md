# SimpleDAW for Windows x64

1. Install the [Microsoft Visual C++ v14 x64 Redistributable](https://aka.ms/vc14/vc_redist.x64.exe) if a compatible version is absent. Microsoft requires a version at least as recent as the build tools used for the app. [Microsoft support and version information](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/)
2. Extract `SimpleDAW.exe` to any folder and double-click it. SimpleDAW has no installer. Microsoft runtime files are supplied by Windows and the separately installed Microsoft package.

## Corresponding source and notices

The same download contains `SimpleDAW-Windows-x64-source.tar.gz`. It is the archive CI extracted to build this executable. It includes SimpleDAW, JUCE, Tracktion Engine, their bundled source dependencies, and build scripts. `BUILD.md` describes the build and SoundTouch relinking. `SOURCE-REVISIONS.txt` identifies the exact revisions. The workflow run summary gives SHA-256 hashes for the executable and source archive.

SimpleDAW's original code is licensed under AGPLv3. See `LICENSE`, `COPYRIGHT`, and `THIRD_PARTY_NOTICES.md` in this download. The source archive includes the additional licence texts and upstream notices. The binary and corresponding source are available together at no charge in the same GitHub Actions artifact.
