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
(`execa` + `getNodeExecutableName()` + release extract/copy). Current names are
inconsistent: `conceald`, `concealwallet`, `walletd`, `optimizer`.

Docs-only planning for this decision was opened against GitHub base `master`
while maintainers intend review on `development`; retargeting is a process
step, not part of the rename itself.

#### Decision

Normalize shipped executable names to a branded kebab-case set (pending
acceptance; CLI wallet spelling locked to `conceal-wallet`, not
`conceal-walletCli`):

| Current | Proposed |
| --- | --- |
| `conceald` | `conceal-daemon` |
| `concealwallet` | `conceal-wallet` |
| `walletd` | `conceal-walletd` |
| `optimizer` | `conceal-optimizer` |

Align internal CMake **executable target** names with the branded set for
maintainability (`Daemon` → `ConcealDaemon`, `PaymentGateService` →
`ConcealWalletd`, `Optimizer` → `ConcealOptimizer`; keep `ConcealWallet`).
Target alignment may land before shipped `OUTPUT_NAME` changes and does not by
itself affect packages or downstream consumers.

**Work priority:** (1) conceal-guardian dual-name support for **both** legacy
and new daemon filenames; (2) conceal-core rename/aliases plus GitHub wiki
updates; (3) verify low impact on conceal-desktop; (4) conceal-faucet-api
operator docs/templates. Prefer completing (1) before shipping a Core
rename-only release. Permit Core-first / same-day rollout only with recorded
evidence that old Guardian can extract, install or copy, restart, and later
update a transition Core build.
Package aliases, extracted release aliases, and updater behavior are different
distribution paths; archive aliases alone do not prove updater compatibility.
Windows/zip compatibility requires an explicit tested mechanism (copy, hard
link, or wrapper)—not interchangeable with Unix symlinks—especially because
Guardian’s zip extractor rejects symlink entries. Preserve explicit `node.path`
(do not silently replace an invalid configured path). Gate alias removal on
demonstrated readiness and operator notice; Guardian fallback removal is a
separate decision. Transition start, earliest removal, minimum Guardian
version, and notice interval remain **TBD** until maintainers decide.

This decision entry and [`docs/binary-naming.md`](binary-naming.md) are
documentation only until implementation is accepted.

#### Rationale

Packages made binary names a distribution API. A single `conceal-` + kebab-case
scheme reduces PATH collisions (`walletd`, `optimizer`), matches how Guardian
and operators refer to “the Conceal daemon”, and avoids mixed-case names that
are awkward on Unix. Guardian-first dual-name support reduces the chance that
auto-update users are stuck on a Core release their installed Guardian cannot
extract or copy.

#### Consequences

- Core CMake executable targets should be aligned for maintainability; shipped
  `OUTPUT_NAME`, package install loops, CI staging that hard-codes old names,
  and docs must move together for the binary rename itself.
- Guardian must learn the new daemon filename (with temporary fallback) and
  must be validated on Linux and Windows extract/copy/restart paths.
- Operator units/scripts need a migration notice even if aliases soften the
  first release; every promised alias must be validated (not optional).
- Optimizer `--walletd-*` flags can stay for now (RPC endpoint labels, not
  binary paths).
- No consensus, cryptography, RPC, wallet-format, data-directory, package-name,
  or release-asset-prefix change is intended.

#### Compatibility impact

Breaking for anything that hard-codes the old filenames without going through
validated aliases / dual-name resolvers. Wallet files, RPC protocols, and
`~/.conceal` data paths stay the same. conceal-desktop impact remains
**pending full verification**: inspected revision shows no `conceald` spawn
path, but that is not a completed downstream gate.

#### Evidence

Inventory and checklist in [`docs/binary-naming.md`](binary-naming.md),
including Core paths (`src/CMakeLists.txt`, `package/*`, package workflows,
`.github/workflows/check.yml`, `tools/deploy.sh`), Guardian revision
`63ca611ce24d42e15c6c9618de760ada0f66ff96` (`units/utils.js`, `engine.js`,
`download.js`), Desktop revision
`da23d79768b58ab34406c1987012b8f655bf5047` (partial: `CN_PROJECT_NAME`, no
`conceald` hits; MIME token `concealwallet` in shortcut script), and
faucet-api revision `da1b4fe7b885cb43768c439bcd4072769134554d` (RPC host/port
only in app code; `walletd`/`conceald` hard-coded in setup docs and
`ccx-walletd.service.template`), and Conceal Core GitHub wiki tip
`d0751870c439a7c6eb117f2585de12da8a91fd75` (`Getting-Started` / `Home` /
`Fusion-Transactions` / `Optimize-Wallet-(CLI)` / `Conceal-Wiki` hard-code
`walletd`, `conceald`, `concealwallet`). No rename implementation or Guardian
end-to-end update test is claimed by this decision.

#### Open questions

- Fill TBD: transition start release, earliest alias-removal release, minimum
  Guardian version, operator notice interval.
- Exact Windows compatibility artifact for release zips and Guardian copy.
- Whether any Core-first exception evidence will be gathered, or priority-1
  Guardian-first remains mandatory.
- Optional later rename of optimizer `--walletd-*` CLI flags.
- Priority-3 Desktop gate passed at `da23d79` (2026-11-04) for Core target /
  `OUTPUT_NAME` renames; re-open only if Desktop changes cryptonote
  integration.
- Priority-4 faucet-api doc owners / timing relative to the Core notice window.

#### Documentation impact

- [`docs/binary-naming.md`](binary-naming.md) — assessment, strategy, checklist
  (primary)
- [`docs/packaging.md`](packaging.md) — proposed-rename note after contents
  table
- Operator release notice draft (in binary-naming.md); wiki / conceal-docs /
  Guardian docs when implementation lands

---
