#!/usr/bin/env bash
# Install and remove a conceal-core package, then check that the package
# manager pulled the runtime libraries and left user data alone.
#
# Runs as root and installs packages system-wide, so use it only in a
# disposable container (the packaging workflows do). For a manual test on a
# real machine, follow the checklist in docs/packaging.md instead.
#
# Usage: test-install.sh <package> <commit-sha>
# The package type comes from the extension: .pkg.tar.zst or .deb.
set -euo pipefail

pkg=$(realpath "$1")
commit=$2
bins=(conceal-daemon conceal-wallet conceal-walletd conceal-optimizer)
legacy=(conceald concealwallet walletd optimizer)

case "$pkg" in
  *.pkg.tar.zst)
    install_pkg() { pacman -U --noconfirm "$1"; }
    remove_pkg() { pacman -R --noconfirm conceal-core; }
    installed_version() { pacman -Q conceal-core | cut -d' ' -f2; }
    file_version() { pacman -Qp "$1" | cut -d' ' -f2; }
    ;;
  *.deb)
    export DEBIAN_FRONTEND=noninteractive
    install_pkg() { apt-get install -y --no-install-recommends "$1"; }
    remove_pkg() { apt-get remove -y conceal-core; }
    installed_version() { dpkg-query -W -f='${Version}' conceal-core; }
    file_version() { dpkg-deb -f "$1" Version; }
    ;;
  *)
    echo "unknown package type: $pkg" >&2
    exit 2
    ;;
esac

fail() { echo "FAIL: $*" >&2; exit 1; }
step() { echo; echo "== $*"; }

has_boost_runtime() { ldconfig -p | grep -q 'libboost_program_options\.so'; }

check_files() {
  local bin
  for bin in "${bins[@]}"; do
    [[ -f /usr/bin/$bin && ! -L /usr/bin/$bin ]] || fail "/usr/bin/$bin missing or not a real file"
    [[ $(stat -c %a "/usr/bin/$bin") == 755 ]] || fail "/usr/bin/$bin mode is not 755"
    if ldd "/usr/bin/$bin" | grep -q 'not found'; then
      ldd "/usr/bin/$bin"
      fail "/usr/bin/$bin has unresolved libraries"
    fi
  done
  for bin in "${legacy[@]}"; do
    [[ ! -e /usr/bin/$bin && ! -L /usr/bin/$bin ]] || fail "/usr/bin/$bin: the package must not install legacy names"
  done
  echo "four binaries present, mode 755, all libraries resolved; no legacy names"
}

# Version and help paths only: no wallet, no sync, no mining. The temporary
# HOME and working directory must stay empty afterwards.
smoke() {
  local version=$1 scratch out bin
  scratch=$(mktemp -d)
  for bin in conceal-daemon conceal-wallet conceal-walletd; do
    out=$(cd "$scratch" && HOME=$scratch timeout 20 "$bin" --version 2>&1) || [[ $? -lt 124 ]] \
      || fail "$bin --version timed out or crashed"
    echo "$bin: $out"
    grep -qF "v${version%%-*}-${commit:0:7}" <<< "$out" \
      || fail "$bin --version does not report ${version%%-*} at commit ${commit:0:7}"
  done
  out=$(cd "$scratch" && HOME=$scratch timeout 20 conceal-optimizer --help 2>&1) || [[ $? -lt 124 ]] \
    || fail "conceal-optimizer --help timed out or crashed"
  grep -q -- '--help' <<< "$out" || fail "conceal-optimizer --help printed no option list"
  echo "conceal-optimizer: help printed"
  [[ -z $(ls -A "$scratch") ]] || { ls -la "$scratch"; fail "version/help wrote files"; }
  rm -rf "$scratch"
}

# The default log goes to the data directory, not beside /usr/bin. walletd
# opens its log, then stops because the container does not exist; it only
# tries localhost RPC and creates no wallet.
check_log_location() {
  local scratch
  scratch=$(mktemp -d)
  (cd "$scratch" && HOME=$scratch timeout 20 conceal-walletd \
    --container-file="$scratch/missing.wallet" --container-password=unused --address) >/dev/null 2>&1 || true
  [[ -f $scratch/.conceal/conceal-walletd.log ]] || { ls -laR "$scratch"; fail "conceal-walletd.log not in the data directory"; }
  [[ ! -e $scratch/missing.wallet ]] || fail "walletd created a container"
  ! compgen -G '/usr/bin/*.log' > /dev/null || fail "a log file was written to /usr/bin"
  echo "conceal-walletd log written to \$HOME/.conceal, nothing in /usr/bin"
  rm -rf "$scratch"
}

step "before install"
! has_boost_runtime || fail "Boost runtime is already installed; dependency pull cannot be checked"
mkdir -p "$HOME/.conceal"
echo keep > "$HOME/.conceal/package-test-marker"
echo "no Boost runtime present; marker written to $HOME/.conceal"

step "install $(basename "$pkg")"
install_pkg "$pkg"
has_boost_runtime || fail "Boost runtime was not installed as a dependency"
[[ $(installed_version) == "$(file_version "$pkg")" ]] || fail "installed version mismatch"
check_files
smoke "$(installed_version)"
check_log_location

step "remove"
remove_pkg
for bin in "${bins[@]}"; do
  [[ ! -e /usr/bin/$bin ]] || fail "/usr/bin/$bin still present after removal"
done
[[ -f $HOME/.conceal/package-test-marker ]] || fail "removal deleted user data"
echo "binaries removed; $HOME/.conceal kept"

step "PASS"
