# Arch Linux package

Requirements, dependencies, upgrade and removal rules, the workflow, and the
developer test plan live in [docs/packaging.md](../../docs/packaging.md).
This file covers only what is specific to this directory.

## Files

- `PKGBUILD`: source build of one exact commit. It installs `conceal-daemon`,
  `conceal-wallet`, `conceal-walletd`, and `conceal-optimizer` to `/usr/bin`,
  plus the license files to `/usr/share/licenses/conceal-core/`.
- `../test-install.sh`: install and removal check that the workflow
  runs in a disposable container. Do not run it on a machine you care about.

The committed `PKGBUILD` has placeholder values for `_commit` and
`sha256sums`. The workflow and the local steps below fill them in for the
commit being built. The source is a `git bundle` of that commit, so CMake can
read the commit ID for `--version`, and makepkg verifies the bundle's SHA256.

## Build locally on Arch or Omarchy

From a checkout of the commit you want, as a normal user:

```bash
sudo pacman -S --needed base-devel git cmake boost

commit=$(git rev-parse HEAD)
work=$(mktemp -d)
cp package/arch/PKGBUILD "$work/"
git bundle create "$work/conceal-core.bundle" HEAD
sum=$(sha256sum "$work/conceal-core.bundle" | cut -d' ' -f1)
sed -i \
  -e "s/^pkgrel=.*/pkgrel=0/" \
  -e "s/^_commit=.*/_commit=$commit/" \
  -e "s/^sha256sums=.*/sha256sums=('$sum')/" \
  "$work/PKGBUILD"
cd "$work" && makepkg
```

`makepkg` uses two compile jobs. On a machine with more memory, set
`CCX_BUILD_JOBS`, for example `CCX_BUILD_JOBS=8 makepkg`.

Install the result with `sudo pacman -U ./conceal-core-*.pkg.tar.zst`. This
local build uses `pkgrel=0`. The Omarchy and AUR recipe below keeps `pkgrel=1`.

## Omarchy submission

Do this only after the Arch and Omarchy rows of the test plan in
[docs/packaging.md](../../docs/packaging.md) pass.

Omarchy builds the package itself from a recipe in
[omacom/omarchy-pkgs](https://github.com/omacom/omarchy-pkgs). It does not use
this repository's workflow artifacts. A submission needs an existing upstream
release tag, not a GitHub Release with attached packages.

The submission adds `pkgbuilds/conceal-core/PKGBUILD` and
`pkgbuilds/conceal-core/.omarchy/package.json` to a fork of omarchy-pkgs. A
PR that changes only package files merges once the `result`, `self-tests`,
and `build-isolation` checks pass. AUR is not required.

### What you need

Omarchy itself is not required. Any Linux machine works with:

- `git`, `bash`, `jq`, `curl`, a `tar` that reads zstd, and `gh`
- Docker or Podman that your user can run, for `bin/build`

Two steps use Arch-only tools: `updpkgsums` (pacman-contrib) and the
upstream-watch check (`vercmp`, `bsdtar`). On a non-Arch machine, run them in
an `archlinux` container as shown below, or on the Omarchy VM. The final
`pacman -U` install test needs Arch or Omarchy.

### 1. Fork and branch

```bash
gh repo fork omacom/omarchy-pkgs --clone
cd omarchy-pkgs
git switch -c add-conceal-core
mkdir -p pkgbuilds/conceal-core/.omarchy
```

### 2. `pkgbuilds/conceal-core/PKGBUILD`

This is the PKGBUILD in this directory with the bundle replaced by the pinned
release tag, so there is no `_commit` or `prepare()`. When the upstream
version changes, update `pkgver` and the checksum; Omarchy's bot does this
once the watch below is merged. AUR uses this same `pkgver` and `pkgrel=1`.
Neither wants a date in the version.

```bash
# Maintainer: Conceal Network <ccx@conceal.network>

pkgname=conceal-core
pkgver=6.7.6
pkgrel=1
pkgdesc='Conceal Network command-line node and wallets (conceal-daemon, conceal-wallet, conceal-walletd, conceal-optimizer)'
arch=('x86_64')
url='https://github.com/ConcealNetwork/conceal-core'
license=('MIT' 'LGPL-3.0-or-later' 'BSD-3-Clause' 'Apache-2.0')
depends=('boost-libs' 'glibc' 'libgcc' 'libstdc++')
makedepends=('boost' 'cmake' 'git')
options=('!lto' '!debug')
source=("git+https://github.com/ConcealNetwork/conceal-core.git#tag=$pkgver")
sha256sums=('FILL-WITH-updpkgsums')

build() {
  # Packaged binaries must run on any x86-64 CPU with AES-NI, so a host -march
  # from makepkg.conf is dropped; CMake adds -march=x86-64 -maes itself.
  CFLAGS=$(sed -E 's/(^| )-march=[^ ]+//g' <<< "$CFLAGS")
  CXXFLAGS=$(sed -E 's/(^| )-march=[^ ]+//g' <<< "$CXXFLAGS")
  export CFLAGS CXXFLAGS

  cmake -S "$pkgname" -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTATIC=OFF \
    -DARCH=x86-64 \
    -DBUILD_TESTS=OFF \
    -Wno-dev
  cmake --build build --parallel "${CCX_BUILD_JOBS:-2}"
}

package() {
  local bin
  for bin in conceal-daemon conceal-wallet conceal-walletd conceal-optimizer; do
    install -Dm755 "build/src/$bin" "$pkgdir/usr/bin/$bin"
  done
  install -Dm644 "$pkgname/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
  install -Dm644 "$pkgname/external/miniupnpc/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE.miniupnpc"
  install -Dm644 "$pkgname/external/parallel_hashmap/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE.parallel_hashmap"
}
```

`arch=('x86_64')` only: the build needs AES-NI (`-maes`), so Omarchy's CI
does not build aarch64. This recipe targets the `6.7.6` tag, the first release
with the `conceal-*` binary names (docs/binary-naming.md). The `6.7.5` tag still
builds `conceald`, `concealwallet`, `walletd`, and `optimizer`, so a `6.7.5`
recipe must use the old names in its install loop.

### 3. `pkgbuilds/conceal-core/.omarchy/package.json`

```json
{
  "source": "local",
  "upstream": { "watch": { "github": "ConcealNetwork/conceal-core", "pattern": "(?P<version>[0-9]+(?:\\.[0-9]+)*)" } }
}
```

The `github` watch reads GitHub Releases. The pattern has no `v` because
upstream tags look like `6.7.6`, and it must match the whole tag.

### 4. Fill the checksum

On Arch or Omarchy:

```bash
cd pkgbuilds/conceal-core && updpkgsums && rm -rf conceal-core && cd -
```

On any other Linux, in a container. `makepkg` refuses to run as root, hence
the extra user:

```bash
cd pkgbuilds/conceal-core
docker run --rm -e HOST_IDS="$(id -u):$(id -g)" -v "$PWD":/pkg archlinux:latest bash -c \
  'pacman -Syu --noconfirm --needed pacman-contrib git >/dev/null && useradd -m b && chown -R b /pkg &&
   su b -c "cd /pkg && updpkgsums"; chown -R "$HOST_IDS" /pkg'
rm -rf conceal-core
cd -
```

`updpkgsums` leaves a clone named `conceal-core` in the package directory;
`rm -rf conceal-core` removes it. Before committing, `git status` must show
only `PKGBUILD` and `.omarchy/package.json`, and `sha256sums` must hold a
real hash, not `FILL-WITH-updpkgsums`.

### 5. Check and build

```bash
tests/pinned-sources.sh
bin/build --package conceal-core --dry-run
bin/build --package conceal-core
```

The package is written, unsigned, to `build-output/edge/x86_64/`. Copy it to
the Omarchy VM and run the developer test from
[docs/packaging.md](../../docs/packaging.md): `sha256sum`,
`sudo pacman -U`, and the `--version` checks. The commit must be the `6.7.6`
tag's commit, from `git rev-parse 6.7.6^{commit}`. Then remove it.

On Arch or Omarchy, also check the upstream watch:

```bash
python helpers/upstream-watch.py check pkgbuilds/conceal-core
```

Expect `"status": "skipped"` with `"reason": "already current"`.

### 6. Open the PR

```bash
git add pkgbuilds/conceal-core
git commit -m "Add conceal-core 6.7.6"
git push -u origin add-conceal-core
gh pr create --repo omacom/omarchy-pkgs --base master --fill --web
```

In the description, say what the package is: the Conceal Network node
(`conceal-daemon`), CLI wallet (`conceal-wallet`), payment gateway
(`conceal-walletd`), and optimizer (`conceal-optimizer`). Say
how it was tested: `bin/build`, then install, version checks, and removal
on Omarchy x86_64, plus a wallet connected to a remote node. Also note that
upstream CI builds and install-tests the same recipe on Arch. Keep the PR to
this one package.

Process confirmed from the omarchy-pkgs README on 2026-10-29.
