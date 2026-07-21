#!/bin/bash
set -euo pipefail

build_dir="${1:-/private/tmp/fishpond-release-make}"
version="${2:-0.1.0}"
app_path="$build_dir/app/FishpondApp_artefacts/Release/Fishpond.app"
python_framework="/opt/homebrew/opt/python@3.12/Frameworks/Python.framework"
release_dir="$PWD/dist"
staging_dir="$release_dir/Fishpond-macos-$version"
output_zip="$release_dir/Fishpond-macos-$version.zip"

test -d "$app_path"
test -d "$python_framework"

rm -rf "$staging_dir" "$output_zip"
mkdir -p "$release_dir"
ditto "$app_path" "$staging_dir/Fishpond.app"
mkdir -p "$staging_dir/Fishpond.app/Contents/Frameworks"
ditto "$python_framework" "$staging_dir/Fishpond.app/Contents/Frameworks/Python.framework"

binary="$staging_dir/Fishpond.app/Contents/MacOS/Fishpond"
install_name_tool -change \
  "$python_framework/Versions/3.12/Python" \
  "@rpath/Python.framework/Versions/3.12/Python" \
  "$binary"
install_name_tool -add_rpath \
  "@executable_path/../Frameworks" \
  "$binary"

ditto -c -k --sequesterRsrc --keepParent "$staging_dir/Fishpond.app" "$output_zip"
rm -rf "$staging_dir"

echo "$output_zip"
