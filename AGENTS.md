# SimpleDAW repository instructions

## Start of every task

- Read `AGENTS.md`, `PROJECT_STATUS.md`, and any specifications relevant to the assignment. Use the status document to locate the current implementation branch and its technical documentation.
- Review recent commits and relevant pull requests when more context is needed. Check which changes are merged, open for review, or present only on a branch. Use repository evidence and current task instructions as the source of truth.

## Development and review

- Follow the existing architecture, approved design decisions, and development conventions. Get authorization before changing previously approved functionality.
- Work on a feature branch and submit a pull request for review. Use a dedicated branch for documentation-only work. Keep changes scoped to the assignment, and respect active development branches.
- Keep project documentation current. Clearly distinguish completed and merged work, work in progress, planned work, and functionality awaiting verification. Claim a test result only when the test actually ran. Identify the build or environment and link the result when available.

## Mandatory clarification and CI gate

- **Before implementation and before requesting CI**, read all relevant specifications and task sheets, inspect affected code, and identify missing, conflicting, or ambiguous user-visible behavior, units, increments, edge cases, and acceptance criteria. Present a concise consolidated set of questions to the user; do not invent decisions. If there are no questions, explicitly report "Clarification checkpoint: clear" with a short summary of assumptions.
- Wait for the user's decisions when questions exist. Update the authoritative specification/task sheet on the appropriate branch and incorporate the decisions **before** requesting the Windows CI build. If an ambiguity appears during coding, stop and ask before proceeding with that affected behavior.
- Perform available local/static/automated checks first. The Windows CI build is a **deliberate, separately authorized step**, after the clarification gate and code readiness review. Do not trigger workflow_dispatch without the user's approval. Opening a PR is not approval to run CI.
- Documentation-only changes should not trigger Windows compilation. Record skipped/not-run tests accurately; never describe a test as passed unless it ran.
- When ready, ask the user to approve a CI run on the intended branch, then invoke the manually dispatched Windows x64 workflow. Report the exact run and artifact. A new run after minor changes should be justified rather than automatic.
- Keep implementation PRs unmerged until the user completes live acceptance and explicitly approves merging.

## End of every development assignment

1. Update `PROJECT_STATUS.md` with relevant changes and record important architecture or design decisions.
2. Record tests that actually ran and their outcomes. Update outstanding issues, untested functionality, and remaining work.
3. Include the status update and any relevant documentation changes in the assignment's pull request. Keep the status concise and link to specifications for technical detail.
