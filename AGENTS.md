# AGENTS.md

## Purpose
This repository must keep implementation, planning, and documentation aligned at all times.

## Core rule
Whenever code, architecture, roadmap, PQ strategy, benchmarks, storage design, consensus behavior, wallet/RPC behavior, build steps, or operational procedures change, review the `docs/` directory and update existing documentation or append new documentation before considering the task complete.

## Documentation sync policy
For every meaningful change, the agent must:

1. Inspect `docs/` for an existing relevant document.
2. Prefer updating an existing doc instead of creating a duplicate.
3. Create a new doc only when no existing doc covers the change.
4. Update cross-references when a renamed concept, flow, or decision affects multiple documents.
5. Keep code and docs consistent in terminology, filenames, feature names, and status.

## Required doc checks
The agent must check whether the change affects any of these:
- Architecture or component boundaries
- PQ migration strategy or cryptographic choices
- Benchmarks, measurements, or performance claims
- Build, test, deployment, or developer workflow
- Wallet, daemon, RPC, or database behavior
- Config flags, CLI options, env vars, or defaults
- Risk, tradeoff, compatibility, or rollout guidance
- Roadmap, milestone, or decision status

## Required actions on change
When a relevant change is made, the agent must:
- Update the affected document in `docs/`
- Add a short “last updated because” note in the changed doc or changelog section
- Preserve historical context for major decisions instead of overwriting it silently
- Mark outdated assumptions as superseded
- Add TODO markers only when the missing information is genuinely unknown

## Completion gate
A task is not complete until:
- Code and docs are consistent
- Any impacted file in `docs/` has been reviewed
- New behavior or decisions are documented
- Stale statements are corrected or removed

## Preferred documentation behavior
- Append incremental findings to the most relevant existing document
- Use concise technical prose
- Record rationale, not just outcome
- Link implementation files, PRs, issues, benchmarks, or design notes when available
- Do not invent benchmark values or claim a decision is final unless the source task says so

## If no docs update is needed
The agent must explicitly state:
“No documentation update required after reviewing docs/, because ...”
and give a one-sentence reason.
