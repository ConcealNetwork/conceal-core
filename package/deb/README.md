# Debian package (Ubuntu 22.04, 24.04, and 26.04)

Requirements, dependencies, upgrade and removal rules, the workflow, and the
developer test plan live in [docs/packaging.md](../../docs/packaging.md).
This file covers only what is specific to this directory.

The `.deb` is distributed through GitHub only. It is not submitted to the
Debian or Ubuntu archives, and it is not part of the Omarchy submission.

## Files

These are the `debian/` files from the
[Debian Packaging Tutorial](https://www.debian.org/doc/manuals/packaging-tutorial/packaging-tutorial):

- `control`: package metadata, build dependencies, and
  `${shlibs:Depends}` for runtime libraries.
- `rules`: `dh` with the CMake build system. It copies the four binaries
  (`conceal-daemon`, `conceal-wallet`, `conceal-walletd`, `conceal-optimizer`)
  to `/usr/bin` because upstream CMake has no install rules for them.
- `copyright`: machine-readable copyright for the project (MIT and
  LGPL-3.0-or-later files) and for the bundled miniupnpc and parallel-hashmap
  code.
- `changelog`: template entry. The workflow replaces the version with the
  build version.
- `source/format`: `3.0 (native)`.

The repository has no top-level `debian/` directory. `dpkg-buildpackage`
expects one, so the build copies this directory to `debian/` first and leaves
this README out.

## Build locally on Ubuntu

From a checkout of the commit you want, on the Ubuntu release you are
targeting:

```bash
sudo apt-get update
sudo apt-get install -y build-essential python3-dev gcc g++ git cmake libboost-all-dev debhelper lintian

. /etc/os-release
pkgver=$(sed -n 's/^set(VERSION "\(.*\)")$/\1/p' CMakeLists.txt)
version="$pkgver-0~${VERSION_CODENAME}1"
rm -rf debian && cp -r package/deb debian && rm debian/README.md
sed -i "1s/^conceal-core ([^)]*) [^;]*;/conceal-core ($version) $VERSION_CODENAME;/" debian/changelog
DEB_BUILD_OPTIONS="parallel=2 noddebs" dpkg-buildpackage -b -us -uc
```

`pkgver` is the `VERSION` in `CMakeLists.txt`, as in the workflow, so a local
build on Ubuntu 22.04 produces `conceal-core_<pkgver>-0~jammy1_amd64.deb`
(24.04 is `noble`, 26.04 is `resolute`). Revision `0` marks a test build; tag
releases use `1`. The `~<codename>1` suffix is the Launchpad PPA form
(docs/packaging.md). The package is written to the parent directory. Install it with
`sudo apt install ../conceal-core_*_amd64.deb`. Remove the generated `debian/`
and `build/` directories afterwards; they are not tracked.
