# Binary naming normalization

Status: Proposed (assessment + alignment plan; **no rename implemented**)
Scope: Conceal Core CLI executable names (`OUTPUT_NAME`), packages, release
archives, and downstream consumers (Guardian, Desktop, faucet-api, docs,
operators).

Last updated because: 2026-11-04 — reassessed impact from Core + related-repo
sources, corrected the Guardian/Windows compatibility strategy, and replaced
the implementation checklist with an actionable implementation / validation /
release-gating checklist. Same day: added `conceal-faucet-api` impact from
py-indexer (`da1b4fe`), GitHub wiki impact from
`ConcealNetwork/conceal-core.wiki` (`d075187`), revised DEC-0002 to **align**
CMake executable targets, and set explicit downstream **work priority**
(Guardian → Core+wiki → Desktop verify → faucet-api). Same day: Priority-3
Desktop gate verified **zero impact** for Core CMake target/`OUTPUT_NAME`
renames (GUI builds cryptonote sources into a private static lib; no
`add_subdirectory` of Core executable CMake). Still documentation only;
DEC-0002 remains Proposed.

## Work priority (downstream + this repo)

Ordered for rollout coordination. Higher items gate or precede lower ones
unless maintainers explicitly waive with evidence.

| Priority | Workstream | Intent |
| --- | --- | --- |
| **1** | **conceal-guardian** | Dual-name compatible with **both** legacy and new daemon filenames (resolve, extract/copy, restart) **before** Core ships a rename-only release |
| **2** | **conceal-core + GitHub wiki** | Implement Core rename/aliases/CMake target alignment as planned; update wiki pages that hard-code old names in the same notice window |
| **3** | **conceal-desktop** | Re-verify low impact (no CLI spawn by old names; no archive consumer surprises). Treat as a verification gate, not a large implementation track |
| **4** | **conceal-faucet-api** | Update operator docs/systemd template naming; API remains RPC host/port — lowest urgency among these four |

Do not reorder casually: skipping (1) risks auto-update breakage; doing (2)
without wiki updates leaves official copy-paste commands wrong; (3) and (4)
are confirmation/doc follow-through after the high-risk paths are covered.

## Change classes (keep these distinct)

| Class | What changes | What does not |
| --- | --- | --- |
| **This docs-only work** | Planning text under `docs/` | No executable, package, workflow, or runtime change |
| **Future rename implementation** | On-disk CLI names, staging, package install loops, aliases, operator docs | Consensus, cryptography, RPC protocols/ports, wallet formats, data directories (`~/.conceal`), package name (`conceal-core`), release **asset** prefixes (`ccx-cli-…`) |

Internal CMake **executable target** names should be aligned with the branded
set when the rename lands (or in a low-risk prep change that does **not**
change `OUTPUT_NAME`). Source directories may stay as-is initially; target
alignment is for maintainability and does not by itself change shipped
binaries.

## Working-branch / PR context (verify before editing)

Snapshot for the docs PR that introduced this plan:

| Item | Value |
| --- | --- |
| Head branch | `cursor/binary-naming-alignment-dc99` |
| Reviewed head SHA | `fd8833b3fc7997220c808ed068fb2e1b3def6ded` |
| GitHub-reported base | `master` |
| Maintainer-intended base | `development` |
| Files in that PR | `docs/binary-naming.md`, `docs/DECISIONS.md`, `docs/packaging.md` |

Findings at reassessment:

- Base still differs from intended `development` (do **not** silently retarget,
  rebase, or reset).
- Three-dot diff vs `origin/development` and vs `origin/master` is the same
  three documentation files only (no unrelated code/package diff on that head).
- Local unrelated working-tree changes (for example `.gitignore`) must be
  preserved when continuing this branch.

## Current state (as of packaging workflows / Core `OUTPUT_NAME`)

Source of truth: [`src/CMakeLists.txt`](../src/CMakeLists.txt) `OUTPUT_NAME`:

| CMake target (current) | Current binary | Role |
| --- | --- | --- |
| `Daemon` | `conceald` | Full node / P2P + RPC daemon |
| `ConcealWallet` | `concealwallet` | Interactive CLI wallet |
| `PaymentGateService` | `walletd` | Payment-gate / wallet RPC service |
| `Optimizer` | `optimizer` | Standalone optimizer talking to `walletd` RPC |

Proposed executable **target** alignment (PascalCase; keep `OUTPUT_NAME` on the
current binaries until the rename release):

| Current target | Aligned target | Current `OUTPUT_NAME` | Future `OUTPUT_NAME` |
| --- | --- | --- | --- |
| `Daemon` | `ConcealDaemon` | `conceald` | `conceal-daemon` |
| `ConcealWallet` | `ConcealWallet` (unchanged) | `concealwallet` | `conceal-wallet` |
| `PaymentGateService` | `ConcealWalletd` | `walletd` | `conceal-walletd` |
| `Optimizer` | `ConcealOptimizer` | `optimizer` | `conceal-optimizer` |

Notes for that prep step (docs plan only — not implemented here):

- Rename only `add_executable` / `target_*` / `add_dependencies` /
  `set_property(TARGET … OUTPUT_NAME …)` references in
  [`src/CMakeLists.txt`](../src/CMakeLists.txt).
- Update CI paths that read `CMakeFiles/Daemon.dir/…` (Arch/Debian workflow
  compile-flag echoes) to the new target directory names.
- Leave source trees (`src/Daemon/`, `src/PaymentGateService/`, …) and static
  libs (`PaymentGate`, …) alone unless a later cleanup explicitly renames them.
- A target-only rename with unchanged `OUTPUT_NAME` has **no** package,
  Guardian, faucet, or wiki distribution impact.

Package install paths (see [`docs/packaging.md`](packaging.md)):

- `/usr/bin/conceald`
- `/usr/bin/concealwallet`
- `/usr/bin/walletd`
- `/usr/bin/optimizer`

Release tarballs / zips ship whatever executables CMake produced. Archive
**asset** names (`ccx-cli-ubuntu-2404-vX.Y.Z.tar.gz`) are independent of binary
names; the **contents** of the archive are not.

Problems with the current set:

1. Inconsistent branding: only the node and CLI wallet carry `conceal*`;
   `walletd` and `optimizer` are generic and collide easily on `PATH`.
2. Inconsistent style: `conceald` (suffix `d`) vs `concealwallet` (no separator)
   vs bare tool names.
3. Packages and Guardian make binary names part of an operational contract
   (`ExecStart=`, `node.path`, `execa`, auto-update extract/copy).

## Proposed names (retain this mapping)

| Current | Proposed | Notes |
| --- | --- | --- |
| `conceald` | `conceal-daemon` | Clear role; kebab-case; branded |
| `concealwallet` | `conceal-wallet` | Recommended; **not** `conceal-walletCli` |
| `walletd` | `conceal-walletd` | Brand prefix + familiar stem |
| `optimizer` | `conceal-optimizer` | Branded; avoids generic `optimizer` |

Do **not** ship `conceal-walletCli` (mixed camelCase is hostile on Unix).
Alternative `conceal-wallet-cli` remains available if maintainers want “CLI” in
the filename; plan default is **`conceal-wallet`**.

### Naming consistency note (`daemon` vs `walletd`)

The proposal expands `conceald` → `conceal-daemon` but keeps the `d` inside
`conceal-walletd`. That is acceptable for “brand prefix + familiar stem”. An
alternative for full role consistency would be `conceal-wallet-service` (or
`conceal-payment-service`), but that is a larger vocabulary change. **Plan
default: keep `conceal-walletd`.**

Open decision (does not block packaging prep): whether optimizer CLI flags
`--walletd-ip`, `--walletd-port`, `--walletd-user`, `--walletd-password` stay as
RPC-endpoint labels (recommended for now) or later gain aliases that say
`conceal-walletd`. Renaming flags is a separate break from renaming the binary.

## Impact assessment

### Confirmed Core consumers (concrete paths)

These paths hard-code or stage the **current** executable names. Any future
rename must update them **together**. Explicit old-name staging in CI and
package checks is a high implementation dependency: leaving one loop on the
old names produces incomplete artifacts or false-green install tests.

| Area | Paths |
| --- | --- |
| CMake `OUTPUT_NAME` | `src/CMakeLists.txt` |
| Arch package | `package/arch/PKGBUILD` (install loop + `pkgdesc`); `package/arch/README.md` |
| Debian package | `package/deb/rules`, `package/deb/control` |
| Install test | `package/test-install.sh` (`bins=(…)` + version/help smoke) |
| Package workflows | `.github/workflows/archlinux.yml`, `.github/workflows/debian.yml` (content checks + artifact `README.txt` text) |
| Dev CI staging | `.github/workflows/check.yml` (`mv build/src/conceald` / `concealwallet` / `walletd` / `optimizer` into `build/conceal` on multiple jobs) |
| Tag release workflows | `.github/workflows/ubuntu22.yml`, `ubuntu24.yml`, `macOS.yml` copy all executable files under `src/` by discovery; `.github/workflows/windows.yml` zips/copies `*.exe` from `src/Release` — still need alias **creation** rules if transition artifacts are required |
| Deploy helper | `tools/deploy.sh` (`WALLETBIN="concealwallet"`, `which conceald`) |
| User-facing strings | e.g. messages under `src/ConcealWallet/` that quote `"concealwallet"` |
| Docs | this file, `docs/packaging.md`, package READMEs; root `README.md` if it lists binary names |

Optional / later:

- Align CMake executable targets as in the table above (recommended; low cost
  if done before or with the `OUTPUT_NAME` change).
- Source directory renames (`src/Daemon/` → …) — optional later cleanup; not
  required for target or binary rename.
- Logger tags (`"daemon"`, `"concealwallet"`, `"optimizer"`) — cosmetic.
- Windows service registration string `"PaymentGate"` in
  `PaymentGateService/main.cpp` — separate from Unix binary name.

### High: conceal-guardian (confirmed at indexed revision)

| Field | Value |
| --- | --- |
| Repository | `conceal-guardian` (local path from py-indexer: sibling checkout) |
| Revision inspected | `63ca611ce24d42e15c6c9618de760ada0f66ff96` |
| Status | **Confirmed** findings below; **no** end-to-end update test was run for this docs revision |

Confirmed source paths and behavior:

- `units/utils.js` — `getNodeExecutableName()` returns `conceald` /
  `conceald.exe`; `getNodeActualPath()` prefers `configOpts.node.path`, else
  `cmdOptions.daemon`, else `path.join(rootPath, getNodeExecutableName())`.
- `units/engine.js` — starts the daemon via `execa(getNodeActualPath(...), …)`.
- `units/download.js` — downloads a Core release asset, extracts it, then
  `fs.cpSync` from a path that **must** contain `getNodeExecutableName()` into
  the Guardian node directory. Windows vs Linux join the extracted layout
  differently, but both look up the **hard-coded** daemon filename.
- `units/download.js` `extractZipSafe()` — **rejects zip entries that are
  symlinks** (`Unsafe zip entry: symlink detected`). Archive-level symlink
  aliases are therefore **not** a proven Windows/zip updater strategy.
- `units/download.js` tar extract uses `tar` with `preservePaths: true`;
  symlink treatment after extract, and whether `fs.cpSync` follows or copies a
  legacy alias, is **pending verification** (not tested here).
- `units/setup.js` + docs — interactive setup / README still describe the
  `"conceald"` path.
- Guardian’s own service unit points at the Guardian binary; the break is the
  **child** daemon name when `node.path` is empty or when auto-update replaces
  the daemon next to Guardian.

**Conclusion:** a Core rename without Guardian dual-name support (or a proven
Core-first exception — see compatibility strategy) breaks default Guardian
installs and auto-update. Highest-risk external consumer.

### Low / verified zero build impact: conceal-desktop

| Field | Value |
| --- | --- |
| Repository | `conceal-desktop` |
| Revision inspected | `da23d79768b58ab34406c1987012b8f655bf5047` |
| Verification | 2026-11-04 — py-indexer + Sonnet 4.6 subagent review; parent spot-check of CMake/`cryptonote` |
| Status | **ZERO IMPACT** on Desktop GUI build for planned Core CMake **target** rename and Core **`OUTPUT_NAME`** rename |

How Desktop consumes Core (build graph):

- Top-level [`CMakeLists.txt`](../../conceal-desktop/CMakeLists.txt) sets
  `CRYPTONOTE_LIB` / includes under `cryptonote/…`, then builds a **hand-picked**
  `CRYPTONOTE_SOURCES` list of `.c`/`.cpp` files from that tree into a private
  static library (`add_library(${CRYPTONOTE_LIB} STATIC …)`), then
  `add_executable(${PROJECT_NAME} …)` linked against that lib + Qt.
- There is **no** `add_subdirectory(cryptonote)` (or equivalent) in Desktop’s
  top-level CMake — Core’s own `src/CMakeLists.txt` executable targets are
  **not** part of the Desktop configure/build graph.
- `CryptoNoteWallet.cmake` sets `CN_PROJECT_NAME` to `conceal-desktop` (GUI
  binary name), independent of Core CLI `OUTPUT_NAME`.
- A vendored copy of Core’s CMake still exists at
  `cryptonote/src/CMakeLists.txt` (including `Daemon` / `OUTPUT_NAME
  "conceald"` etc.), but that file is only relevant if someone builds the
  vendored Core tree **standalone**. The GUI path does not parse it.

Confirmed non-coupling to Core CLI names:

- No Desktop references to Core `OUTPUT_NAME` values `conceald` /
  `concealwallet` / `walletd` / `optimizer` as spawn/install targets for the
  GUI build.
- `src/gui/MainWindow.cpp` `QProcess` is used for `gsettings` (dock), not Core
  CLI.
- Residual string `concealwallet` appears as Desktop packaging/MIME naming
  (`install(… concealwallet.desktop …)`,
  `MimeType=x-scheme-handler/concealwallet;`) — **GUI product naming**, not
  Core CLI `OUTPUT_NAME`.

| Planned Core change | Desktop GUI impact |
| --- | --- |
| Rename CMake targets `Daemon` → `ConcealDaemon`, etc. | **None** — Desktop does not consume those targets |
| Change Core `OUTPUT_NAME` to `conceal-daemon`, etc. | **None** — Desktop does not ship/link those executables |
| Sync newer Core sources into `cryptonote/` | Still **none** for GUI **as long as** Desktop keeps source-list + static-lib integration (not `add_subdirectory` of Core’s executable CMake) |

Residual risks (do not reopen the rename gate; track separately):

- Future Desktop CMake refactor that `add_subdirectory(cryptonote)` or builds
  Core CLI tools from the vendored tree would suddenly inherit Core target /
  `OUTPUT_NAME` changes — out of scope today.
- Syncing Core into `cryptonote/` can still break Desktop if **source paths**
  in the hand-picked list move/rename; that is unrelated to CLI binary naming.
- Historical Desktop package paths named `concealwallet` may confuse operators
  in docs; cosmetic/product naming only.

**Conclusion (Priority 3 gate):** planned Core CMake executable-target
alignment and CLI `OUTPUT_NAME` rename have **zero impact** on conceal-desktop
at `da23d79` given the current cryptonote static-lib integration.

### Medium: conceal-faucet-api (confirmed at indexed revision)

| Field | Value |
| --- | --- |
| Repository | `conceal-faucet-api` |
| Revision inspected | `da1b4fe7b885cb43768c439bcd4072769134554d` |
| Index | ready (`conceal-faucet-api.sqlite3`, 22 files, same revision) |
| Status | **Confirmed** lexical + source read; no faucet end-to-end test run |

Confirmed runtime coupling (low for a Core binary rename):

- Application code talks to wallet/daemon over **HTTP JSON-RPC host/port**, not
  by spawning CLI filenames. See `routes/faucet.js` (`WALLET_RPC_PORT`,
  `DAEMON_RPC_PORT`, optional RPC auth) and `.env.example` /
  `WALLET_HOST` / `DAEMON_HOST`.
- Renaming Core `OUTPUT_NAME` values does **not** by itself break faucet API
  process startup if operators keep serving the same RPC endpoints.

Confirmed operator / docs coupling (medium — breaks on hard rename without
aliases or doc updates):

- `ccx-walletd.service.template` — `ExecStart=/path/to/walletd …` (binary path
  placeholder; unit **name** `ccx-walletd` is independent of the executable
  basename).
- `WALLETD_SETUP.md` — install/enable steps and example
  `ExecStart=/home/pi/conceal-core/build/src/walletd`, plus `pkill` / `ps`
  guidance using `walletd`; daemon peer described as `conceald`.
- `LOCAL_TESTING.md` / `README.md` — require a running `walletd` (and optional
  `conceald`) by those names; troubleshooting uses `ps aux | grep walletd`.
- `.env.example` — comments name `walletd` / `conceald` as the RPC providers
  (env keys remain host/port, not binary paths).

Pending / out of faucet-api scope:

- Whether deployed hosts use absolute paths under `/usr/bin/walletd` vs a
  custom build path (operator-specific; transitional aliases help `/usr/bin`).
- Docker images in this repo do not appear to vendor Core CLI binaries by name
  in the indexed tree; treat any unpublished deploy scripts as unverified.

**Conclusion:** faucet-api is an **RPC consumer**, not a Guardian-style
extract/spawn consumer. Coordinate doc + systemd template updates
(`walletd` → `conceal-walletd`, `conceald` → `conceal-daemon`) for the rename
release; Core aliases cover package/`PATH` installs during transition. Do not
block Core rename on faucet code changes; do include faucet operator docs in
the release notice checklist.

### High: operator systemd / scripts (not shipped by Core packages today)

Core Arch/Debian packages intentionally install **binaries only** — no
`.service` files. Operators still write units and scripts that invoke the
current names. Those break on a hard rename unless transitional aliases exist
**and** are validated on each distribution path (packages vs extracted
archives vs Guardian-managed copies are different).

Neutral example (illustrative only):

```ini
ExecStart=/usr/bin/conceald --help
```

### Medium: Conceal Core GitHub wiki (confirmed clone)

| Field | Value |
| --- | --- |
| URL | https://github.com/ConcealNetwork/conceal-core/wiki |
| Wiki git | `https://github.com/ConcealNetwork/conceal-core.wiki.git` |
| Revision inspected | `d0751870c439a7c6eb117f2585de12da8a91fd75` |
| Status | **Confirmed** lexical scan of all wiki `.md` pages at that tip |

The wiki is operator/integrator documentation for the RPC wallet. It does not
ship or spawn binaries, but it hard-codes current CLI names in copy-paste
commands. A Core rename without a wiki pass leaves official docs teaching the
old filenames after aliases eventually drop.

Confirmed pages with binary-name coupling:

| Page | Names / pattern |
| --- | --- |
| `Home.md` | Introduces RPC Wallet as **walletd** |
| `Getting-Started.md` | Many `./walletd` / `walletd.exe` examples; remote mode requires **conceald**; Windows `--register-service` / `--unregister-service` on `walletd.exe`; default log name `walletd.log` |
| `Fusion-Transactions.md` | Names RPC Wallet as **walletd** |
| `Optimize-Wallet-(CLI).md` | “Always use the latest version of **concealwallet**” |
| `Conceal-Wiki.md` | FAQ text referring to command-line **Concealwallet** |

Notes:

- Most RPC method pages document JSON-RPC only (method names unchanged by this
  rename).
- Some wiki text still cites older daemon ports (`32347` / `8081` in
  `Getting-Started.md`); that is separate stale content, not introduced by the
  rename plan.
- Related public docs outside this wiki (for example third-party pool notes
  that cite the wiki) will lag until the wiki is updated.

**Conclusion:** schedule a wiki edit pass for the rename release notice window
(`walletd` → `conceal-walletd`, `conceald` → `conceal-daemon`,
`concealwallet` → `conceal-wallet`). Does not block Core implementation while
aliases exist; blocks calling operator documentation “done.”

### Medium: documentation and community sites outside this repo

Known references (non-exhaustive; not fully re-audited here):

- External docs that instruct users to run **concealwallet** / **conceald**.
- Guardian setup prompts and configuration docs.
- `conceal-faucet-api` setup docs/templates (see section above).
- Conceal Core GitHub wiki (see section above) — concrete page list at
  `d075187`.
- Discord runbooks / explorer pool docs — likely legacy paths.

These need a release-note pass; they do not block implementing the rename in
Core, but they do block calling the rollout “done” for operators.

### Archive asset names

Keep `ccx-cli-<platform>-v<ver>.*` unless there is a separate branding task.
Guardian matches assets by platform substrings (for example `ubuntu-2404` /
`win64`), then looks for the daemon **filename inside** the extracted tree.
Renaming the binary inside the archive is enough to break current Guardian
extract/copy; renaming the `.tar.gz` / `.zip` asset is a different, optional
change.

## Compatibility strategy (revised)

Prefer **Guardian dual-name support before** the Core rename release (priority
**1** in Work priority). Core + wiki are priority **2**; Desktop verification
**3**; faucet-api docs **4**.

### Why archive aliases alone are insufficient

Distinguish three distribution paths:

1. **Package aliases** — files or symlinks under `/usr/bin` managed by
   pacman/dpkg.
2. **Extracted release aliases** — names present inside `ccx-cli-*` trees after
   `tar`/`zip` extract on disk.
3. **Updater behavior** — Guardian download → extract → `fs.cpSync` of
   `getNodeExecutableName()` into the node directory, then later restart.

Passing (1) or (2) does **not** prove (3). In particular, Guardian’s Windows
zip extractor **refuses symlink entries**, so “ship a symlink named
`conceald.exe` in the zip” is not an interchangeable substitute for dual-name
Guardian logic or for a tested Windows compatibility artifact (real file copy,
hard link where supported, or an explicit wrapper). Treat copies, hard links,
and wrappers as **different** mechanisms; pick one per platform only after it
is tested.

### Preferred sequence

Maps to Work priority: Phase 1 = priority 1; Phase 2 = priority 2 (Core +
wiki); Desktop verify and faucet-api follow as priorities 3–4 (not separate
numbered Core phases).

#### Phase 0 — Decide and announce (this doc / DEC-0002)

1. Confirm PR base should be `development` (process item; do not silently
   retarget).
2. Lock the four new names (table above). Prefer aligning CMake executable
   targets (`ConcealDaemon`, `ConcealWalletd`, `ConcealOptimizer`; keep
   `ConcealWallet`) before or with the `OUTPUT_NAME` change.
3. Publish operator-facing notice when implementation is scheduled (versions
   remain TBD placeholders until maintainers decide).
4. Open coordinated **priority-1** Guardian work: dual-name resolution +
   extract/copy for **both** filename types, with tested fallback.

Placeholders until maintainers fill them:

| Placeholder | Meaning |
| --- | --- |
| Transition start release | First Core release that ships new names (+ aliases if any) — **TBD** |
| Earliest alias-removal release | First Core release allowed to drop legacy names — **TBD** |
| Minimum Guardian version | First Guardian release with dual-name support — **TBD** |
| Support / notice interval | Operator communication window before removal — **TBD** |

#### Phase 1 — Priority 1: Guardian dual-name (before or blocking Core rename)

Minimum Guardian changes (implementation is out of scope for this docs PR):

1. Prefer `conceal-daemon` / `conceal-daemon.exe` when resolving the default
   daemon path.
2. Fall back to `conceald` / `conceald.exe` only when the new name is absent,
   during the transition window.
3. Auto-update: locate and copy the new name from the archive when present;
   accept legacy name when updating from old Core.
4. **Preserve explicit `node.path`:** if the configured path is set, use it; do
   **not** silently replace an invalid configured path with a different
   executable.
5. After in-place upgrade, ensure a **stale** real `conceald` next to a new
   `conceal-daemon` cannot be selected by fallback once the new binary is the
   intended target (define and test the precedence rules).
6. Document failed-update recovery and rollback expectations (interrupted
   download/extract/copy, partial replace, restart against old binary).
7. Setup prompt / docs: primary name `conceal-daemon`, mention legacy once.

#### Phase 2 — Priority 2: Core rename + wiki (with transitional aliases)

In the Core release that changes `OUTPUT_NAME` (and the matching wiki edit
pass under https://github.com/ConcealNetwork/conceal-core/wiki):

1. Ship the four **new** names as the real executables.
2. Ship **platform-appropriate** legacy compatibility artifacts for every
   promised alias — packages **and** release archives as applicable:

   | Legacy name | Points to / provides |
   | --- | --- |
   | `conceald` | `conceal-daemon` |
   | `concealwallet` | `conceal-wallet` |
   | `walletd` | `conceal-walletd` |
   | `optimizer` | `conceal-optimizer` |

3. Unix packages may use symlinks **if** install/upgrade/remove tests prove
   file→symlink transitions and ownership are correct.
4. Windows / zip paths need an **explicit tested** strategy (duplicate file,
   hard link, or wrapper) — not “same as Unix symlink.”
5. Package metadata and artifact `README.txt` list both names and mark legacy
   as transitional.
6. `package/test-install.sh` and package workflow checks assert **new** names
   **and** every promised legacy alias (mandatory, not optional).
7. Release notes: **BREAKING** section with migration for `ExecStart=` and
   Guardian config.
8. Update wiki pages listed under Impact assessment (at least `Home`,
   `Getting-Started`, `Fusion-Transactions`, `Optimize-Wallet-(CLI)`,
   `Conceal-Wiki`).

#### Priority 3 — conceal-desktop verification

**Passed (2026-11-04)** at Desktop `da23d79`: GUI build uses hand-picked
`cryptonote/` sources → private static lib → `conceal-desktop` executable; no
`add_subdirectory` of Core’s executable CMake. Core target / `OUTPUT_NAME`
renames do not affect Desktop. Re-verify only if Desktop changes how it
integrates `cryptonote/`.

#### Priority 4 — conceal-faucet-api docs/templates

Update `WALLETD_SETUP.md`, `ccx-walletd.service.template`, README /
LOCAL_TESTING naming. No faucet API protocol change required for the rename.

#### Core-first / same-day exception (only with evidence)

Core-first or same-day rollout is permitted **only** with recorded evidence
that **old** Guardian can still extract, install/copy, restart, and later
update a transition Core build. Archive aliases alone are not that evidence.
Without that evidence, ship Guardian dual-name support first.

#### Phase 3 — Remove legacy names (separately reviewed)

Gate alias removal on **demonstrated readiness** and operator notice — not on
vague claims that a Guardian version is “ubiquitous.”

1. Record compatible Guardian release + test evidence.
2. Specify transition start and earliest removal releases (fill TBD table).
3. Land a **separate** reviewed change that drops aliases from packages and
   archives.
4. Re-test supported consumers **without** aliases before removal.
5. Guardian fallback removal is a **separately evaluated** decision (may lag
   Core alias removal).

## Implementation / validation / release checklist

Use these groups when implementation starts. Leave items unchecked until there
is actual evidence. Checking a box without evidence is not allowed.

### A. Decision / scope

- [ ] Confirm intended PR base (`development`) and process for any retarget.
- [ ] Accept exact executable names above; accept aligned CMake executable
      target names (`ConcealDaemon` / `ConcealWallet` / `ConcealWalletd` /
      `ConcealOptimizer`).
- [ ] Accept work priority order: (1) Guardian dual-name, (2) Core + wiki,
      (3) Desktop verify, (4) faucet-api docs.
- [ ] Define transition policy owners; fill TBD placeholders (start release,
      earliest removal, minimum Guardian version, notice interval).
- [ ] Record downstream evidence (Guardian/Desktop/faucet-api/wiki revisions +
      paths) and unresolved questions.

### A2. Priority trackers (keep unchecked until evidence exists)

- [ ] **P1** Guardian accepts both legacy and new daemon filenames
      (resolve + extract/copy + restart on Linux and Windows).
- [ ] **P2** Core rename/aliases (and optional target alignment) landed with
      tests; wiki pages updated in the notice window.
- [x] **P3** conceal-desktop low-impact verification recorded at `da23d79`
      (2026-11-04): zero impact for Core target/`OUTPUT_NAME` rename; GUI uses
      cryptonote sources as a static lib, not Core executable CMake.
- [ ] **P4** faucet-api doc/template updates scheduled or landed
      (`WALLETD_SETUP.md`, `ccx-walletd.service.template`, README /
      LOCAL_TESTING).

### B. Core implementation planning

- [ ] Inventory every executable-path consumer (table in Impact assessment).
- [ ] Plan CMake executable target alignment (and any `CMakeFiles/<Target>.dir`
      CI path updates) with or before `OUTPUT_NAME` changes.
- [ ] Plan `OUTPUT_NAME` and all staging/package/check changes together so no
      workflow still moves only old names.
- [ ] Specify compatibility artifacts per supported platform (symlink vs copy
      vs hard link vs wrapper) — mechanisms not interchangeable.
- [ ] Keep protocols, ports, data paths, wallet formats, package name, and
      release asset prefixes out of scope.

### C. Build / artifacts

- [ ] Clean-build each supported CI platform/toolchain.
- [ ] Verify exact canonical names and executable permissions.
- [ ] Inspect actual distributable archives and their extracted contents.
- [ ] Verify aliases resolve within the installed/extracted layout.
- [ ] Confirm required DLLs and other runtime files remain available.
- [ ] Check no stale real legacy executable shadows the new executable.

### D. Package lifecycle

- [ ] Fresh-install Arch and Debian packages.
- [ ] Upgrade old packages to transition packages.
- [ ] Validate canonical commands **and all promised legacy aliases**.
- [ ] Check file ownership and file-to-symlink (or alias) transitions.
- [ ] Test removal and supported downgrade behavior.
- [ ] Verify package operations leave wallet/blockchain data untouched.

### E. Guardian compatibility

- [ ] Old Guardian with old Core baseline.
- [ ] Old Guardian updating to transition Core (only if claiming Core-first).
- [ ] Updated Guardian with old Core.
- [ ] Updated Guardian updating to transition Core.
- [ ] Default path and explicit old/new `node.path` (invalid path must not be
      silently replaced).
- [ ] Linux and Windows extraction/copy/restart behavior.
- [ ] Second update after initial migration.
- [ ] Interrupted/failed update and rollback.
- [ ] Ensure no stale daemon is selected after migration.

### F. Operational validation

- [ ] Safely smoke-test supported help/version invocations.
- [ ] Validate a representative legacy systemd `ExecStart` during transition.
- [ ] Validate migrated service configuration.
- [ ] Use isolated disposable data for any runtime tests.
- [ ] Record tests not run and why.

### G. Release / removal gates

- [ ] Publish precise migration instructions and operator notice.
- [ ] Record compatible Guardian release and test evidence.
- [ ] Specify transition start and earliest alias-removal release.
- [ ] Require a separate reviewed alias-removal change.
- [ ] Re-test supported consumers without aliases before removal.

## Draft operator notice (for the rename release)

Versions below are placeholders — do not invent final numbers in commits.

```text
BREAKING — Conceal Core CLI binary names

Starting with Conceal Core <TRANSITION_START_TBD>, command-line binaries are renamed:

  conceald        -> conceal-daemon
  concealwallet   -> conceal-wallet
  walletd         -> conceal-walletd
  optimizer       -> conceal-optimizer

Impact:
  - systemd/service ExecStart paths that call the old names
  - scripts and cron jobs
  - Conceal Guardian: default daemon filename and auto-update extract/copy
    (use Guardian <MIN_GUARDIAN_TBD>+, or set node.path to the intended binary)

Transition:
  This release still provides legacy names as compatibility aliases where promised.
  They will be removed no earlier than <EARLIEST_REMOVAL_TBD>, after operator notice
  and demonstrated consumer readiness. Prefer the new names now.

Packages (Arch/Debian) install the new names under /usr/bin.
Data directories (~/.conceal) are unchanged. RPC protocols are unchanged.
```

## Risks and non-goals

Risks:

- Guardian auto-update failing zip extract if transition zips contain symlink
  aliases (extractor rejects symlinks).
- Guardian copying/selecting a **stale** legacy daemon after a partial upgrade.
- Operators removing packages and losing aliases while units still use legacy
  names.
- Docs and Discord advice lagging the binaries (support load).
- Treating package-install success as proof of updater compatibility.

Non-goals for this rename:

- Changing RPC method names, ports, or wallet file formats.
- Renaming the `conceal-core` package name.
- Renaming release asset prefixes (`ccx-cli-…`).
- Claiming conceal-desktop verification complete without evidence.
- Shipping systemd units inside `conceal-core` packages (still out of scope
  unless a later packaging decision adds them under the **new** names).

## Decision status

See [`docs/DECISIONS.md`](DECISIONS.md) entry **DEC-0002**. Names and the
transition policy are **Proposed** until maintainers accept them (intended
review base: `development`). No `OUTPUT_NAME` change should land before that
acceptance, Guardian coordination, and checklist evidence for the chosen
rollout order.
