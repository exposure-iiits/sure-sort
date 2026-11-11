#!/bin/bash
# Run with bash; ZIP archives made on Windows do not preserve executable bits.
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
    echo 'A macOS build machine is required. See docs/MACOS.md for the cloud build.' >&2
    exit 1
fi
for tool in brew cmake ninja xcrun python3; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
xcrun --find clang++ >/dev/null

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
qt_root="$(brew --prefix qtbase)"
raw_root="$(brew --prefix libraw)"
architecture="$(uname -m)"
case "$architecture" in
    arm64|x86_64) ;;
    *) echo "Unsupported Mac architecture: $architecture" >&2; exit 1 ;;
esac
[[ -x "$qt_root/bin/macdeployqt" ]] || { echo 'The qtbase installation must contain bin/macdeployqt.' >&2; exit 1; }

build_dir="$project_root/build/macos-$architecture"
mkdir -p "$project_root/build" "$project_root/dist"
# Stage each package separately from previous builds and user session data.
work_dir="$(mktemp -d "$project_root/build/macos-package-$architecture.XXXXXX")"
stage="$work_dir/image"
mkdir -p "$stage"
cmake -S "$project_root" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    -DCMAKE_PREFIX_PATH="$qt_root;$raw_root" \
    -DCMAKE_OSX_ARCHITECTURES="$architecture"
cmake --build "$build_dir" --parallel 3
# Verified output removal tests require Windows locks; this check does not validate those paths.
ctest --test-dir "$build_dir" --output-on-failure -R '^folder_scanner_tests$'
cmake --install "$build_dir" --prefix "$stage"

app="$stage/Sure Sort.app"
binary="$app/Contents/MacOS/Sure Sort"
resources="$app/Contents/Resources"
mkdir -p "$resources/licenses"
cp "$project_root/THIRD_PARTY_NOTICES.md" "$resources/"
cp -R "$project_root/third_party/licenses" "$resources/licenses/project-notices"

# Preserve metadata and supplied license texts for actual Homebrew dependencies.
brew info --json=v2 qtbase libraw > "$resources/licenses/homebrew-direct-dependencies.json"
{ printf '%s\n' qtbase libraw; brew deps --formula --union qtbase libraw; } | sort -u > "$work_dir/formulae.txt"
shopt -s nullglob
while IFS= read -r formula; do
    prefix="$(brew --prefix "$formula")"
    target="$resources/licenses/${formula//\//_}"
    mkdir -p "$target"
    brew info --json=v2 "$formula" > "$target/formula.json"
    for notice in "$prefix"/LICENSE* "$prefix"/LICENCE* "$prefix"/COPYING* \
        "$prefix"/NOTICE* "$prefix"/COPYRIGHT* "$prefix/share/licenses"; do
        [[ ! -e "$notice" ]] || cp -R "$notice" "$target/"
    done
done < "$work_dir/formulae.txt"

# Reuse the existing artwork for the Finder and Dock icon.
iconset="$work_dir/suresort.iconset"
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$project_root/assets/suresort.png" --out "$iconset/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    sips -z "$double" "$double" "$project_root/assets/suresort.png" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$resources/suresort.icns"
/usr/libexec/PlistBuddy -c 'Set :CFBundleIconFile suresort.icns' "$app/Contents/Info.plist"

"$qt_root/bin/macdeployqt" "$app" -verbose=2
# Bundle LibRaw and its dependencies too, and reject unresolved libraries.
cmake "-DAPP_BUNDLE=$app" "-DSEARCH_DIRS=$qt_root/lib;$raw_root/lib;$(brew --prefix)/lib" \
    -P "$project_root/cmake/DeployMacOS.cmake"
cp "$project_root/docs/MACOS-PACKAGE.txt" "$resources/READ ME.txt"
cp "$project_root/docs/MACOS-PACKAGE.txt" "$stage/READ ME.txt"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app/Contents/Info.plist")"
python3 - "$binary" "$work_dir" "$version" <<'PY'
import os
from pathlib import Path
import subprocess
import sys

binary, work, version = sys.argv[1:]
env = {k: v for k, v in os.environ.items()
       if not k.startswith(('QT_', 'DYLD_'))}
env['QT_QPA_PLATFORM'] = 'cocoa'
env['PATH'] = '/usr/bin:/bin:/usr/sbin:/sbin'
result = subprocess.run([binary, '--version'], env=env, text=True,
                        capture_output=True, check=True, timeout=30)
if result.stdout.strip() != f'Sure Sort {version}':
    raise SystemExit(f'Unexpected bundled version: {result.stdout!r}')
session = Path(work) / 'smoke-session.sqlite'
subprocess.run([binary, '--session-db', str(session), '--smoke-test-ms', '2000'],
               env=env, check=True, timeout=30)
if not session.is_file() or session.stat().st_size == 0:
    raise SystemExit('Bundled SQLite launch check did not create a session.')
print(f'Bundled application startup and SQLite check passed: {version}')
PY

ln -s /Applications "$stage/Applications"
output="$project_root/dist/Sure-Sort-$version-macOS-$architecture-experimental.dmg"
if [[ -e "$output" ]]; then
    echo "Package already exists; move it aside before rebuilding: $output" >&2
    exit 1
fi
hdiutil create -volname "Sure Sort $version" -srcfolder "$stage" -format UDZO "$output"
hdiutil verify "$output"
(cd "$(dirname "$output")" && shasum -a 256 "$(basename "$output")" > "$(basename "$output").sha256")
printf 'Created experimental macOS package: %s\n' "$output"
