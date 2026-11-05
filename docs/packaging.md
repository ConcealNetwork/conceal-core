# Linux packages: Arch `.pkg.tar.zst` and Debian `.deb`

Status: Active. Manual test builds, plus release upload on tag push (added
2026-11-04, not yet run).
Scope: native Linux packages of the Conceal Core command-line binaries.

Last updated because: first version, added with the `Arch Linux package` and
`Debian package` workflows (2026-10-29). Same day: Debian LTO turned off after
link failures, and the observed Ubuntu dependencies and lintian results added.
Container job hosts pinned to `ubuntu-24.04` before the `ubuntu-latest` move
to 26.04. After the first Arch test passed on Omarchy, log artifacts are
uploaded only on failure (both workflows). After the Ubuntu 22.04 test, the
missing LGPL-3.0-or-later license was added to both packages. Artifacts
slimmed to package, `SHA256SUMS`, and `README.txt`; the CI upgrade test
package was dropped after it passed. 2026-11-04: linked the proposed CLI
binary rename plan (`docs/binary-naming.md`); package contents table below
still describes **current** names until that plan is accepted and
implemented. Same day (docs revision): moved the proposed-rename note below
the full contents table (including license rows) so the table markup stays
valid; see DEC-0002 / `docs/binary-naming.md` for rollout policy.

Last updated because: 2026-11-04 (6.7.6) — packages install only the new
names `conceal-daemon`, `conceal-wallet`, `conceal-walletd`, and
`conceal-optimizer` (no legacy aliases); default logs moved to the data
directory; Ubuntu 26.04 `.deb` added; both workflows also run on tag push and
attach to the GitHub Release. The earlier "manual only", "current names", and
"proposed rename" statements are superseded below. Same day: Debian versions
changed from `+ubuntu<release>` to `-<revision>~<codename>1` so a planned
Launchpad PPA upload sorts below an Ubuntu archive package of the same
upstream version. No PPA upload job exists yet. Same day: Arch test builds
use `pkgrel=0` instead of `0.<date>`. AUR and Omarchy keep `pkgrel=1`.

Last updated because: 2026-11-05 — Ubuntu 26.04 `.deb` configure failed because
Boost 1.90 has no `boost_system` CMake package. CMake no longer requests the
`system` component (header-only since Boost 1.69); Apple/MinGW retry path
removed. Debian `Depends` unchanged.

## Channels

| Package | Built by | Who installs it | Distribution |
| --- | --- | --- | --- |
| `conceal-core-<ver>-<rel>-x86_64.pkg.tar.zst` | [`.github/workflows/archlinux.yml`](../.github/workflows/archlinux.yml) | Arch Linux and Omarchy, with `pacman -U` | Workflow artifact for testing. Later submitted to the Omarchy package repository. |
| `conceal-core_<ver>-<rev>~jammy1_amd64.deb` | [`.github/workflows/debian.yml`](../.github/workflows/debian.yml) | Ubuntu 22.04 only | GitHub Release binary. PPA source upload planned |
| `conceal-core_<ver>-<rev>~noble1_amd64.deb` | same workflow | Ubuntu 24.04 only | same |
| `conceal-core_<ver>-<rev>~resolute1_amd64.deb` | same workflow | Ubuntu 26.04 only | same |

- Both workflows run on `workflow_dispatch` (test builds: Arch `pkgrel=0`,
  Debian revision `0`) and on any tag push (revision `1`). On a tag, a `release` job
  that runs after `test` has `contents: write` and attaches the packages,
  `<artifact>.SHA256SUMS`, and `<artifact>.README.txt` to that tag's GitHub
  Release with `softprops/action-gh-release` (`append_body`), as the tarball
  workflows do. Other jobs keep `contents: read`. *(Supersedes "dispatch
  only", 2026-11-04.)*
- A tag push fails the `test` job if the tag differs from CMake `VERSION`,
  because `--version` must report the package's upstream version.
- Workflow artifacts expire on GitHub's retention schedule. Manual runs are
  test builds, not a pacman or apt repository and not a published release.
- The `.deb` files are not submitted to the Debian or Ubuntu archives, and
  are not part of Omarchy. A Launchpad PPA is planned; it takes a source
  upload, not these binaries (see Versions below).
- The tag-triggered Ubuntu (22.04, 24.04, and since 2026-11-04 26.04 as
  `ccx-cli-ubuntu-2604`), Windows, and macOS release workflows ship the same
  four binaries in their archives.
- Flatpak is out of scope. A blockchain node needs network access and a large
  persistent data directory, so a sandboxed package needs its own permission
  design. Omarchy installs ordinary Arch packages, so the Arch package is the
  path there.

## Package contents

Package name: `conceal-core` on both systems.

| Path | Contents |
| --- | --- |
| `/usr/bin/conceal-daemon` | node daemon |
| `/usr/bin/conceal-wallet` | command-line wallet |
| `/usr/bin/conceal-walletd` | payment gateway service |
| `/usr/bin/conceal-optimizer` | wallet optimizer tool |
| `/usr/share/licenses/conceal-core/` (Arch) | `LICENSE` (MIT), `LICENSE.miniupnpc` (BSD-3-Clause), `LICENSE.parallel_hashmap` (Apache-2.0) |
| `/usr/share/doc/conceal-core/copyright` (Debian) | MIT, LGPL-3.0-or-later, BSD-3-Clause, and Apache-2.0, in Debian copyright format |

Renamed in 6.7.6 from `conceald`, `concealwallet`, `walletd`, and
`optimizer` ([`docs/binary-naming.md`](binary-naming.md), DEC-0002). The old
names are not installed, not even as symlinks; `test-install.sh` fails if
they are. Earlier "proposed rename / transitional alias" text here is
superseded.

The binaries write no log under `/usr/bin`. Default logs are
`conceal-daemon.log`, `conceal-wallet.log`, and `conceal-walletd.log` in the
data directory (`~/.conceal`, `~/.conceal/testnet` with `--testnet`);
`--log-file` overrides it (a bare file name still goes in the data
directory). `conceal-optimizer` logs to the console.

`/usr/bin` is where package-managed commands go; `/usr/local/bin` is for
software installed outside the package manager. Upstream
[`src/CMakeLists.txt`](../src/CMakeLists.txt) has no `install()` rules for
the four binaries, so each package copies them and sets mode `755`.

miniupnpc and parallel-hashmap are compiled into the binaries, which is why
their licenses ship too.

32 files under `src/` are LGPL-3.0-or-later: CryptoNote/Bytecoin, Karbo,
and the optimizer code, including `src/Platform/Linux/System/*`, `src/Rpc/*`,
`src/Common/*`, and `src/Wallet/WalletSerializationV2.*`. They are compiled
into every binary; `conceal-optimizer --help` prints a GNU license notice.

- **Arch:** `license=('MIT' 'LGPL-3.0-or-later' 'BSD-3-Clause' 'Apache-2.0')`.
  The LGPL text comes from Arch's shared SPDX license directory, so it is not
  copied.
- **Debian:** `copyright` lists the LGPL files in their own stanza. It points
  to `/usr/share/common-licenses/LGPL-3` and `GPL-3`.

This LGPL entry corrects the first version of these packages (2026-10-29),
which declared only MIT, BSD-3-Clause, and Apache-2.0.

Nothing else is installed. Installing, upgrading, or removing a package does
not start or enable a service, create or import a wallet, download blockchain
data, open firewall ports, enable mining, or change user configuration. There
is no Arch `.install` script and no Debian maintainer script.

## Packaging files

```text
package/
  test-install.sh       install and removal check (disposable containers only)
  arch/
    PKGBUILD
    README.md
  deb/
    control
    rules
    copyright
    changelog
    source/format
    README.md
```

The Debian build copies `package/deb` to `debian/` at the source root, because
`dpkg-buildpackage` requires that path. `/debian/` is in `.gitignore`.
Local build steps are in [`package/arch/README.md`](../package/arch/README.md)
and [`package/deb/README.md`](../package/deb/README.md).

The Debian files follow the
[Debian Packaging Tutorial](https://www.debian.org/doc/manuals/packaging-tutorial/packaging-tutorial)
(Lucas Nussbaum, version 0.30): `dh` with `debhelper-compat (= 13)` and the
CMake build system, built with `dpkg-buildpackage`, checked with `lintian`.
The `.deb` archive is never assembled by hand.

## Build settings

Both packages build the exact commit the workflow checked out (`github.sha`)
with full git history, so CMake's `git log -1` stamps the real commit into
`--version` instead of the hardcoded `COMMIT` in
[`CMakeLists.txt`](../CMakeLists.txt).

```text
-DCMAKE_BUILD_TYPE=Release -DSTATIC=OFF -DARCH=x86-64 -DBUILD_TESTS=OFF
```

- Two compile jobs. The Debian jobs also add the 6 GB swap file the Ubuntu
  release jobs use. The Arch job runs in a container, where swap cannot be
  added; it relies on the runner's memory.
- `STATIC=OFF`: the binaries link the distribution's shared Boost, libstdc++,
  and glibc. The in-tree libraries and miniupnpc are static archives, as
  upstream declares them. The binaries are not fully static; `ldd` output is
  saved with each build.
- CPU baseline: generic x86-64 with AES-NI. CMake passes `-march=x86-64` and
  `-maes`. `NO_AES` stays off, so a CPU without AES-NI is not supported.
  `-march=native` is never used.
- Arch uses its standard `makepkg.conf` flags (`-O2`, `_FORTIFY_SOURCE=3`,
  stack clash and CET protection). The PKGBUILD removes any `-march=` from
  `CFLAGS` and `CXXFLAGS`, so a host-specific `makepkg.conf` cannot change the
  CPU baseline. `options=('!lto' '!debug')` keeps peak build memory down and
  skips the separate debug package.
- Debian uses its standard hardening and turns LTO off
  (`DEB_BUILD_MAINT_OPTIONS="hardening=+all optimize=-lto"`), with
  `DEB_BUILD_OPTIONS="parallel=2 noddebs"`. Ubuntu's default `dpkg-buildflags`
  add `-flto=auto -ffat-lto-objects`. With LTO, the 2026-10-29 builds on
  22.04 and 24.04 failed to link, with undefined references to
  `cn_slow_hash<...>::explode_scratchpad_3()` and related members
  (`src/crypto/cn_slow_hash_hard_intel.cpp`). Without LTO, both link. Arch has
  LTO off for the same reason, plus lower peak memory.
- CMake's own `-O3 -DNDEBUG` Release flags are added after the distribution
  flags, as in the existing release builds.

On Arch, CMake 4.4.5 accepts `cmake_minimum_required(VERSION 3.5)` with a
deprecation warning (2026-10-29). Ubuntu 26.04 / Boost 1.90 does not ship
`boost_systemConfig.cmake` or `libboost-system*` (header-only; stub removed
around Boost 1.89). As of 2026-11-05, top-level `CMakeLists.txt` requests
Boost components without `system` on all platforms. *(Supersedes the earlier
note that Linux `find_package(... system ...)` needed no change.)*

## Dependencies

With `STATIC=OFF`, Linux builds request these Boost components:
`filesystem`, `thread`, `date_time`, `chrono`, `regex`, `serialization`, and
`program_options`. `system` is not requested (header-only since Boost 1.69).
Only some of the listed components are still separate shared libraries. The
rest are header-only in current Boost. The binaries also need `libresolv`,
`libstdc++`, `libgcc_s`, and `libc`. There is no OpenSSL, zlib, or database
dependency.

### Arch

- `depends=('boost-libs' 'glibc' 'libgcc' 'libstdc++')`
- `makedepends=('boost' 'cmake' 'git')`, plus `base-devel`

Arch splits `gcc-libs` into `libgcc` and `libstdc++`. `namcap` reports
exactly those four runtime packages on the 2026-10-29 build, which linked
`libboost_filesystem` and `libboost_program_options` from Boost 1.92.
`pacman -U` installs missing dependencies from the configured repositories.

### Debian and Ubuntu

The build installs the prerequisite set from [README.md](../README.md) and
[`tools/deploy.sh`](../tools/deploy.sh), plus the compilers named in
`check.yml` and the packaging tools:

```bash
sudo apt-get install -y build-essential python3-dev gcc g++ git cmake libboost-all-dev debhelper lintian
```

- `Build-Depends`: `debhelper-compat (= 13), cmake, git, libboost-all-dev, python3-dev`.
  `build-essential`, `gcc`, and `g++` are implied in a Debian build and are
  not listed.
- `Depends: ${shlibs:Depends}, ${misc:Depends}`. `dh_shlibdeps` writes the
  real runtime packages, such as `libboost-filesystem1.83.0`, `libstdc++6`,
  `libc6`, and `libgcc-s1`. Those package names include the Boost version, so
  the Ubuntu 22.04 and 24.04 packages have different dependencies and cannot
  be swapped. `python3` is not a runtime dependency.

`Depends` generated on the 2026-10-29 local builds:

| Release | Toolchain | Generated `Depends` |
| --- | --- | --- |
| 22.04 | GCC 11.4, CMake 3.22.1 (apt; the GitHub runner's preinstalled 3.31.6 takes precedence there), Boost 1.74 | `libboost-filesystem1.74.0, libboost-program-options1.74.0, libc6 (>= 2.34), libgcc-s1 (>= 3.0), libstdc++6 (>= 12)` |
| 24.04 | GCC 13.3, CMake 3.28.3, Boost 1.83 | `libboost-filesystem1.83.0, libboost-program-options1.83.0, libc6 (>= 2.38), libgcc-s1 (>= 3.0), libstdc++6 (>= 13.1)` |
| 26.04 | TODO: record from the first `debian.yml` run (job log prints compiler, CMake, Boost) | TODO: from that run's `dpkg-deb --info` |

`lintian` reports no errors. Its warnings:

- `no-manual-page` for each of the four binaries. Upstream ships no man pages.
- Info-level `spelling-error-in-binary` notes for misspellings in upstream
  strings, such as `Succesfully` and `ouput`. They are left as-is, because
  packaging does not change program output.

Install with `sudo apt install ./conceal-core_*.deb`, which installs missing
dependencies. `sudo dpkg -i` does not, and leaves the package unconfigured
until `sudo apt-get -f install` runs.

## Versions, upgrade, uninstall

The upstream version is CMake's `VERSION`, or the tag when the built commit
has an exact `X.Y.Z` or `vX.Y.Z` tag. The commit is recorded in the
artifact's `README.txt` and in `--version` output, not in the package
version, so versions stay ordered.

| Build | Arch | Debian |
| --- | --- | --- |
| Manual test build | `6.7.6-0` | `6.7.6-0~<codename>1` |
| Tag release package | `6.7.6-1` | `6.7.6-1~<codename>1` |

`<codename>` comes from `/etc/os-release`: `jammy` (22.04), `noble` (24.04),
`resolute` (26.04). A 22.04 release file is
`conceal-core_6.7.6-1~jammy1_amd64.deb`.

- Arch test builds use `pkgrel=0`, so the file is
  `conceal-core-6.7.6-0-x86_64.pkg.tar.zst`. The date stamp is gone
  (2026-11-04): AUR and Omarchy do not use one, and their recipes stay at
  `pkgrel=1` (`package/arch/README.md`). Two test builds share `6.7.6-0`;
  `pacman -U` reinstalls that version. `pkgrel=1` upgrades `pkgrel=0`.
- Debian test builds carry no date. Two test builds of the same upstream
  version share `6.7.6-0~<codename>1`: install the second with
  `sudo apt install --reinstall ./conceal-core_*_amd64.deb`.
- Revision `1` sorts above revision `0`, so `6.7.6-1~jammy1` upgrades
  `6.7.6-0~jammy1`. Checked with `dpkg --compare-versions` on 2026-11-04:
  `6.7.6-0~jammy1` < `6.7.6-1~jammy1` < `6.7.6-1~jammy2`, and
  `6.7.6-1~jammy1` < `6.7.6-1~noble1` < `6.7.6-1~resolute1`.
- `+ubuntu22.04` (used earlier the same day) is superseded. `+` sorts after a
  plain revision, so that package would look newer than an Ubuntu archive
  package `6.7.6-1` and block it. `~` sorts before, which is what a PPA
  wants. Launchpad does not require a date in the version.

### Launchpad PPA (planned, not implemented)

The version above is the PPA form. The upload is not built yet.

- A PPA accepts a source package (`.dsc` via `dput`), then Launchpad compiles
  the binary. The `.deb` attached to the GitHub Release cannot be uploaded.
- One version per Ubuntu series: `6.7.6-1~jammy1`, `6.7.6-1~noble1`,
  `6.7.6-1~resolute1`. The changelog distribution is that codename, which the
  workflow already writes.
- The final digit is the upload count. Launchpad rejects a repeated version,
  so a rebuild of the same upstream version must become `~jammy2`. The
  workflow always writes `1`.
- Source format stays `3.0 (native)`. `dpkg-source` 1.21.1 builds
  `6.7.6-1~jammy1` in that format (checked 2026-11-04). `3.0 (quilt)` needs an
  upstream tarball that this workflow does not produce.
- Upgrade replaces the files in `/usr/bin` only.
- Uninstall removes the packaged files only. Wallets and blockchain data under
  the default data directory, `~/.conceal`, are never touched by installation,
  upgrade, or removal. Delete them yourself only if you mean to; that
  directory can hold wallet keys.

```bash
# Arch / Omarchy
sudo pacman -U ./conceal-core-*.pkg.tar.zst   # install or upgrade
sudo pacman -R conceal-core                   # uninstall

# Ubuntu
sudo apt install ./conceal-core_*_amd64.deb   # install or upgrade
sudo apt remove conceal-core                  # uninstall
```

## Checksums

Each artifact contains `SHA256SUMS` with the package filename and its
SHA256. Verify before installing:

```bash
sha256sum -c SHA256SUMS
```

MD5 is not used. On Arch, the `git bundle` source also has a real
`sha256sums` entry that `makepkg` verifies, never `SKIP`.

## Workflows

Every container job runs on an `ubuntu-24.04` host. It is pinned because
`ubuntu-latest` moves to Ubuntu 26.04 between 2026-10-19 and 2026-11-19
([GitHub changelog](https://github.blog/changelog/2026-09-17-ubuntu-26-generally-available-and-latest-migration/)).

### Arch Linux package

1. `build`, in `archlinux/archlinux:base-devel` pinned by digest, after
   `pacman -Syu`. This is an isolated Arch container, not a `devtools` clean
   chroot; nested `systemd-nspawn` is not a reliable fit for GitHub-hosted
   container jobs.
   - Root installs build dependencies, writes the `git bundle` of the
     commit, and fills `pkgver`, `pkgrel`, `_commit`, and `sha256sums` in a
     copy of the PKGBUILD.
   - `makepkg` runs as the unprivileged `builder` user.
   - Checks: `bash -n`, `namcap` on the PKGBUILD and package (any `E:` fails
     the job), file list, mode `755`, `ldd`, and license files. The output,
     compiler, CMake, Boost, and compile flags are printed in the job log.
2. `test`, in a fresh `archlinux/archlinux:base` container. `base-devel`
   cannot be used here, because its `gdb` already pulls in `boost-libs`.
   It verifies `SHA256SUMS`, then runs
   [`package/test-install.sh`](../package/test-install.sh).

### Debian package

1. `build` on GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04`, and
   `ubuntu-26.04` runners. The
   build runs as the runner user; `sudo` is used only for swap and `apt-get`.
   `dpkg-buildpackage -b -us -uc` builds the binary package. Then
   `lintian --fail-on error`, `dpkg-deb --contents`, and `ldd` check it. The
   results and toolchain versions go to the job log.
2. `test` in fresh `ubuntu:22.04`, `ubuntu:24.04`, and `ubuntu:26.04`
   containers pinned by digest (26.04 digest pulled 2026-11-04). Each verifies `SHA256SUMS` and runs `package/test-install.sh` on
   its own release's package.

### What `test-install.sh` checks

1. Boost runtime libraries are absent before install.
2. Install pulls them in, all four binaries are in `/usr/bin` with mode
   `755`, `ldd` finds every library, and none of the old names
   (`conceald`, `concealwallet`, `walletd`, `optimizer`) exists.
3. `conceal-daemon --version`, `conceal-wallet --version`, and
   `conceal-walletd --version` print the upstream version and the built
   commit. `conceal-optimizer --help` prints its options. Each runs under a
   20-second timeout with a temporary `HOME` and working directory, which
   must stay empty. These paths exit before any wallet, network, or mining
   code. `conceal-wallet --version` exits with status 1 after printing; the
   test checks the output, not the exit status.
4. Log location: `conceal-walletd` with a missing container and a temporary
   `HOME` must write `$HOME/.conceal/conceal-walletd.log`, create no
   container, and leave no `*.log` in `/usr/bin`.
5. Removal deletes the four binaries and leaves a marker file in
   `~/.conceal`.

The first version also ran an upgrade to a repackaged higher revision. It
passed on Arch, Ubuntu 22.04, and Ubuntu 24.04 (2026-10-29). It was then
removed, so CI builds only what users download. Upgrades are still checked
by hand in step 5 of the developer test.

### Artifacts

Each artifact holds only what a user needs:

| Artifact | Contents |
| --- | --- |
| `conceal-core-arch-x86_64` | `.pkg.tar.zst`, `SHA256SUMS`, `README.txt` |
| `conceal-core-deb-ubuntu-2204`, `conceal-core-deb-ubuntu-2404`, `conceal-core-deb-ubuntu-2604` | `.deb`, `SHA256SUMS`, `README.txt` |
| `*-build-logs`, `*-test-logs` | build and install logs, uploaded only when the job fails |

`README.txt` gives the version, the commit, and the verify, install, check,
and uninstall commands for that package. The namcap/lintian, `ldd`,
file-list, and toolchain details are in the run's job log, not in the
artifact.

## Testing plan

The workflow checks run in containers. They do not replace a test on a real
machine.

### 1. Trigger

1. Open **Actions** in GitHub.
2. Select **Arch Linux package** or **Debian package**.
3. Choose **Run workflow**, select the branch or tag, and run it.
4. When it finishes, download the artifact from the run summary:
   `conceal-core-arch-x86_64`, `conceal-core-deb-ubuntu-2204`,
   `conceal-core-deb-ubuntu-2404`, or `conceal-core-deb-ubuntu-2604`.

With the GitHub CLI:

```bash
gh workflow run archlinux.yml --ref <branch-or-tag>
gh workflow run debian.yml --ref <branch-or-tag>
gh run list --workflow archlinux.yml --limit 1
gh run download <run-id> --name conceal-core-arch-x86_64
```

### 2. Developer test on the target OS

| Package | Machines |
| --- | --- |
| `.pkg.tar.zst` | Arch Linux x86_64, then Omarchy x86_64 |
| `~jammy1` `.deb` | Ubuntu 22.04 x86_64 |
| `~noble1` `.deb` | Ubuntu 24.04 x86_64 |
| `~resolute1` `.deb` | Ubuntu 26.04 x86_64 |

Record the OS, CPU, and the commit from `README.txt`, then:

1. In the artifact directory, run `sha256sum -c SHA256SUMS`.
2. Install with `sudo pacman -U ./conceal-core-*.pkg.tar.zst` or
   `sudo apt install ./conceal-core_*_amd64.deb`. Confirm the package
   manager lists and installs the missing dependencies.
3. Check `ls -l /usr/bin/conceal-daemon /usr/bin/conceal-wallet /usr/bin/conceal-walletd /usr/bin/conceal-optimizer`,
   and that `ls /usr/bin/conceald /usr/bin/concealwallet /usr/bin/walletd /usr/bin/optimizer`
   finds none of them.
4. Run `conceal-daemon --version`, `conceal-wallet --version`,
   `conceal-walletd --version`, and `conceal-optimizer --help`. The version
   must show the commit from `README.txt`. Do not create a wallet, start a
   sync, or mine.
5. Upgrade: install a tag release package over the test build (Arch
   `6.7.6-1` over `6.7.6-0`, Ubuntu `6.7.6-1~<codename>1` over
   `6.7.6-0~<codename>1`). Confirm `pacman -Q conceal-core` or
   `dpkg -s conceal-core` shows the release version. The binaries must still
   run.
6. Remove with `sudo pacman -R conceal-core` or `sudo apt remove conceal-core`.
   Confirm the binaries are gone and `~/.conceal`, if present, is unchanged.

Write pass or fail for each machine next to the workflow run. A passing test
shows the package installs, upgrades, and uninstalls cleanly. It does not
show that the node syncs, that wallets work correctly, or that Omarchy will
list the package.

## After the developer test

1. Tag automation: **implemented 2026-11-04, not yet run** (see Channels). A
   tag push builds revision `1` and attaches the `.pkg.tar.zst`, the three
   `.deb` files, and their renamed `SHA256SUMS` / `README.txt` to that tag's
   GitHub Release. The manual trigger stays for test builds.
2. Omarchy: submit a recipe to
   [omacom/omarchy-pkgs](https://github.com/omacom/omarchy-pkgs), as
   described in [`package/arch/README.md`](../package/arch/README.md).
   Omarchy builds from a pinned upstream tag, so this does not wait for tag
   automation or release artifacts. The recipe targets `6.7.6`, the first
   tag with the `conceal-*` names, and Omarchy's upstream watch proposes
   later versions. Users then
   install it with `omarchy pkg add conceal-core` or Install > Package in the
   Omarchy menu.

## Known limitations

- x86_64 only. No ARM package yet.
- A CPU without AES-NI cannot run these binaries.
- Arch is rolling. A package built today links today's Boost soname
  (`1.92.0`). After Arch moves to a newer Boost, an old artifact stops
  resolving `libboost_*`; rebuild instead of forcing it.
- Each `.deb` matches only its own Ubuntu release. Other Debian-based
  distributions are untested.
- The Arch build runs in a container, not a clean chroot.
- The 22.04 `.deb` builds on the GitHub-hosted `ubuntu-22.04` runner. GitHub
  started deprecating it on 2026-09-17 and stops supporting it on 2027-04-17.
  Before that date, move the 22.04 build into an `ubuntu:22.04` container or
  drop the 22.04 package.
