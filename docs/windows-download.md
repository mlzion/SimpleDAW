# SimpleDAW for Windows x64

## v0.25.1 MD0 integration

The Windows x64 Release `/MD` integration package is `SimpleDAW-v0.25.1-Windows-x64.zip`. Use the artifact linked in draft PR #27 and extract it in full. Launch `SimpleDAW-v0.25.1.exe` with the bundled `Drum Samples` directory beside it. Use the same separately installed Microsoft x64 runtime described below. This build is for Windows live acceptance. [MD0 integration draft](https://github.com/mlzion/SimpleDAW/pull/27)

## Accepted v0.24.1 Piano Roll P0

1. Install the [Microsoft Visual C++ v14 x64 Redistributable](https://aka.ms/vc14/vc_redist.x64.exe) if a compatible version is absent. Microsoft requires a version at least as recent as the build tools used for the app. [Microsoft support and version information](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/)
2. Download and extract the accepted Windows x64 ZIP, then open `SimpleDAW-v0.24.1.exe`. [SimpleDAW-v0.24.1-Windows-x64.zip](https://github.com/mlzion/SimpleDAW/actions/runs/45291914568/artifacts/14875361245) SimpleDAW has no installer. Microsoft runtime files are supplied by Windows and the separately installed Microsoft package.

The ZIP comes from the explicitly authorized Windows x64 `/MD` workflow. [Windows build policy](build/windows-build-policy.md)

## Corresponding source and notices

The same download contains `SimpleDAW-Windows-x64-source.tar.gz`. It is the archive CI extracted to build this executable. It includes SimpleDAW, JUCE, Tracktion Engine, their bundled source dependencies, and build scripts. `BUILD.md` describes the build and SoundTouch relinking. `SOURCE-REVISIONS.txt` identifies the exact revisions. The workflow run summary gives SHA-256 hashes for the executable and source archive.

SimpleDAW's original code is licensed under AGPLv3. See `LICENSE`, `COPYRIGHT`, and `THIRD_PARTY_NOTICES.md` in this download. The source archive includes the additional licence texts and upstream notices. The binary and corresponding source are available together at no charge in the same GitHub Actions artifact.
