#!/usr/bin/env bash
set -euo pipefail

product='benno111 engene'
id=benno111-engene
source_file=$(realpath -- "${BASH_SOURCE[0]}")
data_dir=$(realpath -m -- "${XDG_DATA_HOME:-$HOME/.local/share}")
install_dir="$data_dir/$id"
launcher="$data_dir/applications/$id.desktop"
mode=install
assume_yes=false
gui=false

if [[ ${source_file##*/} == uninstall ]]; then
    mode=uninstall
    install_dir=$(dirname -- "$source_file")
    data_dir=$(dirname -- "$install_dir")
fi

usage() {
    printf '%s\n' "Usage: $0 [--yes] [--uninstall]" \
        "Installs $product for the current user under XDG_DATA_HOME."
}

for option in "$@"; do
    case "$option" in
        --yes) assume_yes=true ;;
        --uninstall) mode=uninstall ;;
        --help|-h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

if ! $assume_yes && [[ ! -t 0 && -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]] && command -v zenity >/dev/null; then
    gui=true
fi

fail() {
    printf 'Error: %s\n' "$1" >&2
    if $gui; then zenity --error --title="$product Setup" --text="$1" || true; fi
    exit 1
}

confirm() {
    if $assume_yes; then return 0; fi
    if $gui; then
        zenity --question --title="$product Setup" --text="$1"
    elif [[ -t 0 ]]; then
        local answer
        printf '%s [y/N] ' "$1"
        read -r answer
        [[ $answer == [yY] || $answer == [yY][eE][sS] ]]
    else
        fail "Run this installer in a terminal or pass --yes."
    fi
}

finish() {
    printf '%s\n' "$1"
    if $gui; then zenity --info --title="$product Setup" --text="$1" || true; fi
}

owns_install() {
    [[ ! -L "$install_dir" && -f "$install_dir/.benno111-install" ]] &&
        grep -Fxq "$id" "$install_dir/.benno111-install"
}

owns_launcher() {
    [[ ! -L "$launcher" && -f "$launcher" ]] &&
        grep -Fxq 'X-Benno111-Managed=true' "$launcher" &&
        grep -Fxq "Exec=\"$install_dir/$id\"" "$launcher"
}

if [[ $mode == install ]]; then mkdir -p -- "$data_dir"; fi
[[ -d "$data_dir" ]] || fail "No managed installation found at $install_dir"
command -v flock >/dev/null || fail "The installer requires flock from util-linux."
exec {install_lock}<"$data_dir"
flock -n "$install_lock" || fail "Another installation or removal is already in progress."

if [[ $mode == uninstall ]]; then
    owns_install || fail "No managed installation found at $install_dir"
    if [[ -f "$install_dir/.launcher-path" ]]; then
        launcher=$(< "$install_dir/.launcher-path")
    fi
    confirm "Remove $product from $install_dir?" || exit 0
    if owns_launcher; then rm -- "$launcher"; fi
    rm -rf -- "$install_dir"
    finish "$product was uninstalled."
    exit 0
fi

if [[ -e "$install_dir" || -L "$install_dir" ]]; then
    owns_install || fail "The target already exists and was not created by this installer: $install_dir"
fi
if [[ -e "$launcher" || -L "$launcher" ]]; then
    owns_launcher || fail "The application launcher already exists and is not managed by this installer."
fi
case "$install_dir" in
    *\\*|*\"*|*\$*|*\`*|*%*|*$'\n'*|*$'\r'*) fail "The installation path contains characters unsupported by the launcher." ;;
esac

confirm "Install $product and add it to your applications menu? Install folder: $install_dir" || exit 0
payload_line=$(awk '/^__BENNO111_PAYLOAD__$/{print NR+1; exit}' "$source_file")
[[ -n $payload_line ]] || fail "This file contains no installation payload."

mkdir -p -- "$data_dir/applications"
stage=$(mktemp -d -- "$data_dir/.benno111-stage.XXXXXX")
backup=''
launcher_tmp=''
new_installed=false
complete=false

cleanup() {
    if ! $complete; then
        if $new_installed; then rm -rf -- "$install_dir"; fi
        if [[ -n $backup && -d "$backup/app" ]]; then
            mv -T -- "$backup/app" "$install_dir" || return
        fi
    fi
    rm -rf -- "$stage"
    if [[ -n $backup ]]; then rm -rf -- "$backup"; fi
    if [[ -n $launcher_tmp ]]; then rm -f -- "$launcher_tmp"; fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

tail -n "+$payload_line" -- "$source_file" | tar -xzf - --no-same-owner -C "$stage" ||
    fail "Could not extract the installer payload."
[[ -x "$stage/$id" && -f "$stage/LICENSE" && -d "$stage/assets" && -d "$stage/shaders" ]] ||
    fail "The installer payload is incomplete."
head -n "$((payload_line - 2))" -- "$source_file" > "$stage/uninstall"
chmod 755 "$stage/uninstall"
printf '%s\n' "$id" > "$stage/.benno111-install"
printf '%s\n' "$launcher" > "$stage/.launcher-path"

launcher_tmp=$(mktemp -- "$data_dir/applications/.benno111-launcher.XXXXXX")
cat > "$launcher_tmp" <<DESKTOP
[Desktop Entry]
Type=Application
Name=$product
Exec="$install_dir/$id"
Terminal=false
Categories=Game;
X-Benno111-Managed=true
DESKTOP
chmod 644 "$launcher_tmp"

if [[ -d "$install_dir" ]]; then
    backup=$(mktemp -d -- "$data_dir/.benno111-backup.XXXXXX")
    mv -T -- "$install_dir" "$backup/app"
fi
mv -T -- "$stage" "$install_dir"
new_installed=true
mv -T -- "$launcher_tmp" "$launcher"
complete=true
finish "$product is installed. Launch it from your applications menu."
exit 0
__BENNO111_PAYLOAD__
