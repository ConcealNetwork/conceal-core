# Conceal PQ Decisions Log

Status: Active
Maintainers: Conceal contributors working on post-quantum planning, architecture, benchmarking, and implementation
Scope: Decision tracking for Conceal post-quantum research, migration strategy, and implementation work

## How to use this file

Use this file to record major technical and strategic decisions related to Conceal post-quantum work.

Create a new entry whenever the team changes or confirms:
- PQ scheme direction
- Hybrid versus full migration approach
- Signature, KEM, or key-management strategy
- Benchmark-driven selection criteria
- Wallet, daemon, RPC, or storage implications
- Compatibility, rollout, fallback, or deprecation policy
- Architecture decisions with multi-file or multi-component impact

Guidelines:
- Prefer appending new entries over overwriting history
- If a decision changes, mark the old one as superseded
- Keep entries concise but explicit about rationale and consequences
- Link related docs, issues, PRs, benchmarks, or design notes whenever available

## Entry template

---

### DEC-XXXX: Short decision title

- Date: YYYY-MM-DD
- Status: Proposed | Accepted | Deferred | Superseded | Rejected
- Owners: @handle, @handle
- Related work: issue/PR/doc links
- Supersedes: DEC-XXXX or none
- Superseded by: DEC-XXXX or none

#### Context

Describe the technical, strategic, or operational situation that required a decision.

#### Decision

State the decision clearly and directly.

#### Rationale

Explain why this option was chosen over alternatives.

#### Consequences

List the expected effects on implementation, planning, benchmarks, operations, or contributor workflow.

#### Compatibility impact

State the expected effect on backward compatibility, mixed deployments, migration, or external interfaces.

#### Evidence

Reference benchmarks, research notes, prototype results, or linked design material.

#### Open questions

List unresolved items that could change follow-on work.

#### Documentation impact

List which files in `docs/` should be updated because of this decision.

---

## Suggested tags

Use these tags inside entries when helpful:
- `PQ-schemes`
- `hybrid-migration`
- `benchmarks`
- `wallet`
- `daemon`
- `rpc`
- `storage`
- `consensus-assumptions`
- `compatibility`
- `rollout`
- `security`

## Example entry

---

### DEC-0001: Record benchmark gate before narrowing PQ candidates

- Date: 2026-07-15
- Status: Proposed
- Owners: @team
- Related work: pq-conceal benchmark notes, scheme evaluation docs
- Supersedes: none
- Superseded by: none

#### Context

Multiple post-quantum candidate paths may remain viable until benchmark data is collected across the daemon, wallet, and transaction-critical flows.

#### Decision

Do not finalize a preferred PQ candidate set until benchmark criteria and test conditions are documented and repeatable.

#### Rationale

A scheme choice made before consistent measurements are available could optimize for incomplete assumptions and force rework later.

#### Consequences

Benchmark definitions become a gating deliverable for strategy updates, implementation sequencing, and documentation claims.

#### Compatibility impact

No immediate compatibility change, but future rollout planning should treat benchmark outcomes as a decision dependency.

#### Evidence

Link benchmark plans, prototype measurements, and evaluation dashboards.

#### Open questions

Which transaction paths, key sizes, verification costs, and storage impacts must be measured first?

#### Documentation impact

Update benchmark docs, migration planning docs, and any decision dashboard that references candidate narrowing.

---

### DEC-0002: Normalize Conceal Core CLI binary names under a `conceal-` prefix

- Date: 2026-11-04
- Status: Proposed
- Owners: @Acktarius / Conceal Core maintainers
- Related work: Arch/Debian packaging (`docs/packaging.md`),
  [`docs/binary-naming.md`](binary-naming.md), conceal-guardian daemon spawn /
  auto-update, upcoming operator release notes
- Supersedes: none
- Superseded by: none
- Tags: `compatibility`, `rollout`, `daemon`, `wallet`

#### Context

Manual Arch `.pkg.tar.zst` and Debian `.deb` workflows now install Core CLI
binaries into `/usr/bin`. That makes on-disk names part of an operational
contract for packages, systemd `ExecStart`, scripts, and Conceal Guardian
(`execa` + `getNodeExecutableName()` + release extract). Current names are
inconsistent: `conceald`, `concealwallet`, `walletd`, `optimizer`.

#### Decision

Normalize shipped executable names to a branded kebab-case set (pending final
acceptance of the exact CLI wallet spelling):

| Current | Proposed |
| --- | --- |
| `conceald` | `conceal-daemon` |
| `concealwallet` | `conceal-wallet` (not `conceal-walletCli`) |
| `walletd` | `conceal-walletd` |
| `optimizer` | `conceal-optimizer` |

Ship transitional legacy aliases for one release window, and coordinate
conceal-guardian before dropping them. Do not treat conceal-desktop builds as
blocked: Desktop does not spawn these CLI binaries by name.

#### Rationale

Packages made binary names a distribution API. A single `conceal-` + kebab-case
scheme reduces PATH collisions (`walletd`, `optimizer`), matches how Guardian
and operators refer to “the Conceal daemon”, and avoids mixed-case names that
are awkward on Unix.

#### Consequences

- Core CMake `OUTPUT_NAME`, package install loops, CI staging, and docs must
  move together.
- Guardian must learn the new daemon filename (with temporary fallback).
- Operator units/scripts need a migration notice even if symlinks soften the
  first release.
- Optimizer `--walletd-*` flags can stay for now (RPC endpoint labels, not
  binary paths).

#### Compatibility impact

Breaking for anything that hard-codes the old filenames without going through
aliases. Wallet files, RPC protocols, and `~/.conceal` data paths stay the
same. conceal-desktop compile/link is unaffected by Core `OUTPUT_NAME` alone.

#### Evidence

Inventory in [`docs/binary-naming.md`](binary-naming.md): Core packaging files,
Guardian `units/utils.js` / `engine.js` / `download.js`, Desktop
`CN_PROJECT_NAME=conceal-desktop` with in-process node (no CLI spawn).

#### Open questions

- Exact transition length before removing legacy symlinks.
- Whether Guardian must release on the same day as Core, or a day earlier with
  dual-name support only.
- Optional later rename of optimizer `--walletd-*` CLI flags.

#### Documentation impact

- [`docs/binary-naming.md`](binary-naming.md) — assessment and plan (primary)
- [`docs/packaging.md`](packaging.md) — forward reference under package contents
- Operator release notice draft (in binary-naming.md); wiki / conceal-docs /
  Guardian docs when implementation lands

---
