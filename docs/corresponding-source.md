# Retrieving the source for a Windows build

The Windows workflow records the SimpleDAW, JUCE, and Tracktion Engine commit IDs in the run summary. Use that summary to identify the actual checkout. Pull-request builds can use a synthetic merge commit. The feature-branch push run builds the feature commit directly.

1. Check out the SimpleDAW commit reported by the run. The checkout includes CMake, the Windows workflow, the verification script, and the [build instructions](../README.md#build-on-windows-11).
2. The CMake configure step obtains JUCE 8.0.6 at `19edd538429c93d277bf95b55aaa7e3eb545f951` and Tracktion Engine v3.2.0 at `0a5f4e6a5f53d09c89b414a44386a12df7fa1ec6`. The dependency checkouts contain their bundled sources and notices. No Tracktion submodule is required since JUCE is fetched separately.
3. Configure and build the x64 Release target with the README commands. Use `-DSIMPLEDAW_ASIO_SDK:PATH=` on configure to reproduce the standard CI build. Windows system libraries and a licensed Windows C++ build toolchain are prerequisites.

For any future binary upload, stage a companion corresponding-source archive from the exact three CI checkouts, preserving sources, notices, build scripts, and any patches. Test the archive by configuring with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<local-juce>` and `-DFETCHCONTENT_SOURCE_DIR_TRACKTION=<local-tracktion>` and running the same build and tests. Publish equivalent no-charge access and clear directions from the binary download location for as long as required. Use a feature-branch push or another durable source ref for a release candidate. Keep source and licence materials accessible alongside the EXE ZIP. [AGPLv3 §6](https://www.gnu.org/licenses/agpl-3.0.en.html#section6)
