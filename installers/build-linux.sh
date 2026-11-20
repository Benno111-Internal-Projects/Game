#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "Usage: $0 <desktop-build-directory> <output.run>" >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_dir=$1
output=$2
[[ -x "$build_dir/benno111-engene" && -d "$build_dir/assets" && -d "$build_dir/shaders" ]]
mkdir -p -- "$(dirname -- "$output")"
stage=$(mktemp -d)
temporary=$(mktemp -- "${output}.XXXXXX")
trap 'rm -rf -- "$stage"; rm -f -- "$temporary"' EXIT

cp -R -- "$build_dir/benno111-engene" "$build_dir/assets" "$build_dir/shaders" "$stage/"
cp -- "$script_dir/../LICENSE" "$stage/LICENSE"
cat -- "$script_dir/linux-installer.sh" > "$temporary"
tar -czf - -C "$stage" benno111-engene assets shaders LICENSE >> "$temporary"
chmod 755 "$temporary"
mv -f -- "$temporary" "$output"
