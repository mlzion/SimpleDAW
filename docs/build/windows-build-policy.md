# SimpleDAW — Windows Build Policy

**Status:** Accepted — effective immediately (2026-11-21)  
**Scope:** Windows build configurations, CI, acceptance testing, and distribution.  
**Owner:** SimpleDAW project.

## Mandatory policy

1. Windows x64 **/MD** is the standard configuration for development, acceptance testing, and distribution.
2. Discontinue routine Windows **/MT** builds and tests. Keep /MT build capability available for explicitly requested diagnostics only.
3. Retain existing automated Linux CTests.
4. Produce Windows /MD acceptance builds **only after explicit authorization**. Do not interpret a PR, commit, or documentation update as authorization.
5. Do not trigger Windows CI for documentation-only changes.
6. Preserve application behavior and existing licensing/source-distribution obligations.
7. Keep the currently open acceptance PR open and unmerged until acceptance testing is completed and approved.

## Codex implementation checklist

- Inspect Windows build scripts, CMake defaults/presets, GitHub Actions workflows, acceptance instructions, distribution packaging, and project status documents.
- Change Windows defaults and routine test paths to x64 /MD; remove routine /MT matrix jobs and redundant /MT checks without deleting diagnostic support.
- Gate acceptance-build workflows behind explicit authorization (for example, manually dispatched workflow); ensure documentation-only changes do not trigger Windows jobs. Preserve Linux CTests.
- Update relevant build, testing, licensing/distribution references, README and PROJECT_STATUS.md to reflect this policy without altering licensing requirements.
- Report the changed files and exact workflow triggers, plus how a future diagnostic /MT build can be requested.
- Do not run a Windows acceptance build as part of implementing this policy unless separately approved.
- Do not merge the existing acceptance PR.

## Acceptance criteria

- Routine workflows no longer build or test /MT.
- Windows acceptance /MD jobs cannot run solely due to pushes or documentation-only PRs.
- Linux CTests continue to run as before.
- A deliberate /MT diagnostic build remains possible.
- Documentation consistently states the /MD policy and existing runtime/source obligations.
- No application functionality changes.

## Build paths

- Acceptance uses `.github/workflows/windows.yml` with the sole trigger `workflow_dispatch`. After explicit authorization, select the approved ref and run `gh workflow run windows.yml --ref <approved-ref>`. This builds and tests `/MD` and publishes the executable with its matching source and notices.
- Request an `/MT` diagnostic separately and use the [local diagnostic commands](../msvc-dynamic-runtime.md#requested-mt-diagnostics). The standard workflow has one runtime.
- The separate LLVM MinGW investigation remains manually dispatched and does not publish an acceptance build.
- Normal Linux CMake builds retain the existing CTest suite.
