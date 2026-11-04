# Binary naming normalization

Status: Proposed (assessment + alignment plan; no rename implemented yet)
Scope: Conceal Core CLI executable names (`OUTPUT_NAME`), packages, release
archives, and downstream consumers (Guardian, Desktop, docs, operators).

Last updated because: packaging workflows for Arch `.pkg` / Debian `.deb` made
the `/usr/bin` layout a real distribution surface; this doc records the
brainstorming assessment and the proposed rename plan before any code change.

## Current state (as of 6.7.5 / packaging workflows)

Source of truth: [`src/CMakeLists.txt`](../src/CMakeLists.txt) `OUTPUT_NAME`:

| CMake target | Current binary | Role |
| --- | --- | --- |
| `Daemon` | `conceald` | Full node / P2P + RPC daemon |
| `ConcealWallet` | `concealwallet` | Interactive CLI wallet |
| `PaymentGateService` | `walletd` | Payment-gate / wallet RPC service |
| `Optimizer` | `optimizer` | Standalone optimizer talking to `walletd` RPC |

Package install paths (see [`docs/packaging.md`](packaging.md)):

- `/usr/bin/conceald`
- `/usr/bin/concealwallet`
- `/usr/bin/walletd`
- `/usr/bin/optimizer`

Release tarballs (`ccx-cli-*`) copy whatever executables CMake produced under
`build/src` (or `build/src/Release` on MSVC). Archive **asset** names
(`ccx-cli-ubuntu-2404-vX.Y.Z.tar.gz`) are independent of binary names; the
**contents** of the archive are not.

Problems with the current set:

1. Inconsistent branding: only the node and CLI wallet carry `conceal*`;
   `walletd` and `optimizer` are generic and collide easily on `PATH`.
2. Inconsistent style: `conceald` (suffix `d`) vs `concealwallet` (no separator)
   vs bare tool names.
3. Packages and Guardian now make binary names part of an operational contract
   (`ExecStart=`, `node.path`, `execa`, auto-update extract).

## Proposed names (brainstorm input)

| Current | Proposed | Notes |
| --- | --- | --- |
| `conceald` | `conceal-daemon` | Clear role; kebab-case; branded |
| `walletd` | `conceal-walletd` | Keeps familiar `walletd` stem; branded |
| `optimizer` | `conceal-optimizer` | Branded; avoids generic `optimizer` |
| `concealwallet` | `conceal-walletCli`? | See recommendation below |

### Recommendation on the CLI wallet name

Do **not** ship `conceal-walletCli`.

- Mixed camelCase is hostile on Unix (`PATH`, tab-completion, docs copy-paste)
  and fights the kebab-case pattern of the other three proposals.
- Prefer one of:
  - **`conceal-wallet`** (recommended): GUI is already `conceal-desktop`, so
    there is no product-name clash; shortest accurate name.
  - **`conceal-wallet-cli`**: explicit if you want “CLI” in the filename for
    users who still say “ConcealWallet”.

Recommended target set for the next breaking CLI release:

| Current | Recommended new name |
| --- | --- |
| `conceald` | `conceal-daemon` |
| `concealwallet` | `conceal-wallet` |
| `walletd` | `conceal-walletd` |
| `optimizer` | `conceal-optimizer` |

### Naming consistency note (`daemon` vs `walletd`)

The proposal expands `conceald` → `conceal-daemon` but keeps the `d` inside
`conceal-walletd`. That is acceptable if the goal is “brand prefix + familiar
stem”. An alternative for full role consistency would be
`conceal-wallet-service` (or `conceal-payment-service`), but that is a larger
vocabulary change for operators who already say “walletd”. **Plan default:
keep `conceal-walletd`.**

Open decision (does not block packaging prep): whether optimizer CLI flags
`--walletd-ip`, `--walletd-port`, `--walletd-user`, `--walletd-password` stay as
RPC-endpoint labels (recommended for now) or later gain aliases that say
`conceal-walletd`. Renaming flags is a separate break from renaming the binary.

## Impact assessment

### Low / none: conceal-desktop

`conceal-desktop` builds its own GUI binary (`CN_PROJECT_NAME=conceal-desktop`)
and vendors Cryptonote sources into that process. It does **not** shell out to
`conceald`, `walletd`, `concealwallet`, or `optimizer` by those names.

Renaming Core CLI `OUTPUT_NAME` values should **not** break a Desktop build by
itself. Residual coupling to watch:

- Shared protocol / RPC behavior (unchanged by rename).
- Docs or installer text that still say `concealwallet` for the **GUI**
  (historical `.desktop` / doc paths inside Desktop: e.g. install rules that
  still mention `concealwallet` as a package/doc name — Desktop-side cleanup,
  not Core).
- Any future Desktop feature that downloads Core release archives and expects
  nested filenames (none found in the current Desktop tree for CLI spawn).

**Conclusion:** safe for Desktop **builds**. Coordinate only if Desktop ships
or documents Core CLI side-by-side.

### High: conceal-guardian

Guardian treats the daemon filename as a hard contract:

- `getNodeExecutableName()` returns `conceald` / `conceald.exe`
  (`units/utils.js`).
- `startDaemonProcess()` uses `execa(getNodeActualPath(...), ...)`
  (`units/engine.js`).
- Auto-update extracts the release archive and copies
  `getNodeExecutableName()` into the Guardian directory
  (`units/download.js`).
- Interactive setup and docs ask for the path to `"conceald"`.
- `config.json` `node.path` can override the default; operators who set an
  absolute path keep working **only if** that path still exists after upgrade.
- Guardian’s own systemd unit (`ccx-guardian.service`) `ExecStart=` points at
  the **Guardian** binary, not `conceald`. The break is the **child** process
  name Guardian resolves when `node.path` is empty or when auto-update
  replaces the daemon next to Guardian.

**Conclusion:** a Core rename without a Guardian release (or a transition
strategy) breaks default Guardian installs and auto-update. This is the
highest-risk external consumer.

### High: operator systemd / scripts (not shipped by Core packages today)

Core Arch/Debian packages intentionally install **binaries only** — no
`.service` files, no `ExecStart`. Many node operators still write units like:

```ini
ExecStart=/usr/bin/conceald --rpc-bind-ip 0.0.0.0 ...
```

or invoke `walletd` / `optimizer` from cron and compose files. Those break on
a hard rename unless transitional aliases exist.

### Medium: conceal-core internal surface

Must change together when the rename lands:

| Area | Files / notes |
| --- | --- |
| CMake `OUTPUT_NAME` | `src/CMakeLists.txt` |
| Arch package | `package/arch/PKGBUILD` install loop + `pkgdesc` |
| Debian package | `package/deb/rules`, `package/deb/control` Description |
| Install test | `package/test-install.sh` |
| Package workflows | `.github/workflows/archlinux.yml`, `debian.yml` (checks + artifact `README.txt`) |
| CI / release staging | `.github/workflows/check.yml` and tag workflows that `mv`/`cp` named binaries into release folders |
| Deploy helper | `tools/deploy.sh` (`WALLETBIN`, `which conceald`) |
| User-facing strings | e.g. `ConcealWallet.cpp` messages that quote `"concealwallet"` |
| Docs | this file, `docs/packaging.md`, root `README.md` if it lists binary names, package READMEs |

Optional / later (not required for binary rename):

- CMake **target** names (`Daemon`, `ConcealWallet`, …) and source directory
  `src/ConcealWallet/` — internal; leave stable unless a broader cleanup.
- Logger tags (`"daemon"`, `"concealwallet"`, `"optimizer"`) — cosmetic.
- Windows service registration string `"PaymentGate"` in
  `PaymentGateService/main.cpp` — separate from Unix binary name.

### Medium: documentation and community sites outside this repo

Known references (non-exhaustive):

- `conceal-docs` `optimize-cli.md` — instructs users to use **concealwallet**.
- Guardian `INSTALL.md` / `docs/Configuration.md` / setup prompts — **conceald**.
- Wiki / Discord runbooks / explorer pool docs — likely `conceald` paths.

These need a release-note pass; they do not block implementing the rename in
Core, but they do block calling the rollout “done” for operators.

### Archive asset names

Keep `ccx-cli-<platform>-v<ver>.*` unless there is a separate branding task.
Guardian matches assets by substrings such as `ubuntu-2404` / `win64`, then
looks for **`conceald` inside the extracted tree**. Renaming the binary
inside the tarball is enough to break Guardian extract; renaming the
`.tar.gz` is a different, optional change.

## Compatibility strategy (recommended)

Do **not** hard-cut without a transition. Recommended sequence:

### Phase 0 — Decide and announce (this doc)

1. Lock the four new names (recommended set above, or an explicit variant).
2. Publish operator-facing notice: next Core release changes on-disk binary
   names; systemd `ExecStart`, scripts, and Guardian `node.path` /
   auto-update are affected.
3. Open coordinated Guardian issue/PR: teach new names + temporary fallback.

### Phase 1 — Implement in Core with transitional aliases

In the same Core release that changes `OUTPUT_NAME`:

1. Install/ship the four **new** names as the real executables.
2. For one release series (or until Guardian ≥ required version is ubiquitous),
   also install **compatibility symlinks** (Unix) / hard links or small stub
   copies (Windows if symlinks are awkward):

   | Legacy name | Points to |
   | --- | --- |
   | `conceald` | `conceal-daemon` |
   | `concealwallet` | `conceal-wallet` |
   | `walletd` | `conceal-walletd` |
   | `optimizer` | `conceal-optimizer` |

3. Package metadata (`pkgdesc`, Debian Description, artifact `README.txt`)
   list both names and mark legacy as transitional.
4. `package/test-install.sh` asserts **new** names are present and mode 755;
   optionally asserts legacy symlinks resolve.
5. Release notes: **BREAKING** section with copy-paste migration for
   `ExecStart=` and Guardian config.

### Phase 2 — Guardian (and any downloaders)

Minimum Guardian changes:

1. `getNodeExecutableName()` returns `conceal-daemon` / `conceal-daemon.exe`.
2. Resolution order when `node.path` is empty: prefer new name; if missing,
   fall back to `conceald` / `conceald.exe` during the transition window.
3. Auto-update: copy the new name from the archive; if only the legacy name
   exists (old Core), still accept it.
4. Setup prompt / docs: say `conceal-daemon`, mention legacy name once.
5. Ship Guardian release **with or immediately after** the Core rename
   release so auto-update users are not stuck.

Operators with an explicit `node.path` to `/usr/bin/conceald` keep working
while the symlink exists; after symlink removal they must edit config.

### Phase 3 — Remove legacy names

After the transition window (one major Core release is a reasonable minimum;
longer if pool nodes update slowly):

1. Drop symlinks from packages and release archives.
2. Drop Guardian fallback to `conceald`.
3. Mark this doc’s transition section superseded; keep the final name table.

## Implementation checklist (when coding starts)

Core (this repo):

- [ ] Change the four `OUTPUT_NAME` values in `src/CMakeLists.txt`
- [ ] Add install rules or packaging loops for new names + transitional links
- [ ] Update `package/arch/PKGBUILD`, `package/deb/rules`, `package/deb/control`
- [ ] Update `package/test-install.sh` and package workflow checks / README.txt
- [ ] Update `.github/workflows/check.yml` and tag release workflows that stage binaries by name
- [ ] Update `tools/deploy.sh` and in-tree user-facing strings that quote old names
- [ ] Update `docs/packaging.md` package contents table
- [ ] Draft GitHub Release / Discord operator notice text (see below)

Downstream / out-of-repo:

- [ ] conceal-guardian: executable name, fallback, docs, setup prompt
- [ ] conceal-docs / wiki: concealwallet → conceal-wallet, conceald → conceal-daemon
- [ ] Spot-check conceal-desktop docs only (build should be unaffected)
- [ ] Remind operators: edit systemd `ExecStart` and Guardian `node.path`

## Draft operator notice (for the rename release)

```text
BREAKING — Conceal Core CLI binary names

Starting with Conceal Core vX.Y.Z, command-line binaries are renamed:

  conceald        -> conceal-daemon
  concealwallet   -> conceal-wallet
  walletd         -> conceal-walletd
  optimizer       -> conceal-optimizer

Impact:
  - systemd/service ExecStart paths that call the old names
  - scripts and cron jobs
  - Conceal Guardian: default daemon filename and auto-update extract
    (upgrade Guardian to vA.B.C+, or set node.path to the new binary)

Transition:
  This release still provides legacy names as symlinks/aliases.
  They will be removed in a later release. Prefer the new names now.

Packages (Arch/Debian) install the new names under /usr/bin.
Data directories (~/.conceal) are unchanged. RPC protocols are unchanged.
```

## Risks and non-goals

Risks:

- Guardian auto-update writing the wrong filename after a Core-only rename.
- Operators removing packages and losing symlinks while units still use legacy
  names.
- Docs and Discord advice lagging the binaries (support load).

Non-goals for this rename:

- Changing RPC method names, ports, or wallet file formats.
- Renaming the `conceal-core` package name.
- Renaming release asset prefixes (`ccx-cli-…`).
- Forcing a conceal-desktop rebuild solely for CLI name changes.
- Shipping systemd units inside `conceal-core` packages (still out of scope
  unless a later packaging decision adds them under the **new** names).

## Decision status

See [`docs/DECISIONS.md`](DECISIONS.md) entry **DEC-0002**. Names and the
symlink transition are **Proposed** until maintainers accept them on the
development branch / in review. No `OUTPUT_NAME` change should land before
that acceptance and a Guardian coordination note.
