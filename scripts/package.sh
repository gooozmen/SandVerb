#!/usr/bin/env bash
# Упаковывает готовую сборку SandVerb.vst3 в архив релиза для Linux или macOS:
# папка плагина + install.sh + краткая инструкция, рядом контрольная сумма SHA-256.
#
#   scripts/package.sh linux|macos [папка-сборки]     (по умолчанию ./build)
#
# Результат: dist/SandVerb-<версия>-<система>.tar.gz (Linux) или .zip (macOS).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
os="${1:-}"
build="${2:-$ROOT/build}"

case "$os" in
    linux) label="linux-$(uname -m)" ;;
    macos) label="macos-universal" ;;
    *)     echo "Использование: $0 linux|macos [папка-сборки]" >&2; exit 2 ;;
esac

version="$(sed -n 's/^project(SandVerb VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$version" ] || { echo "Не удалось прочитать версию из CMakeLists.txt" >&2; exit 1; }

bundle="$(find "$build/SandVerb_artefacts" -type d -path '*/VST3/SandVerb.vst3' 2>/dev/null | head -1)"
[ -d "$bundle" ] || { echo "Не найден собранный SandVerb.vst3 в $build (сначала соберите проект)." >&2; exit 1; }

name="SandVerb-$version-$label"
stage="$ROOT/dist/$name"
rm -rf "$stage"
mkdir -p "$stage"

cp -R "$bundle" "$stage/SandVerb.vst3"
cp "$ROOT/installers/install.sh" "$stage/install.sh"
chmod +x "$stage/install.sh"

cat > "$stage/INSTALL.txt" <<TXT
SandVerb $version ($label)

Установка для текущего пользователя:
    ./install.sh

Установка для всех пользователей (нужен sudo):
    ./install.sh --system

Удаление:
    ./install.sh --uninstall           (или с --system, если ставили так)

Скрипт копирует SandVerb.vst3 в:
TXT
if [ "$os" = "linux" ]; then
    printf '    ~/.vst3   (для всех пользователей: /usr/lib/vst3)\n' >> "$stage/INSTALL.txt"
else
    printf '    ~/Library/Audio/Plug-Ins/VST3   (для всех пользователей: /Library/Audio/Plug-Ins/VST3)\n' >> "$stage/INSTALL.txt"
fi
printf '\nПосле установки перезапустите DAW или пересканируйте плагины.\n' >> "$stage/INSTALL.txt"

cd "$ROOT/dist"
if [ "$os" = "linux" ]; then
    archive="$name.tar.gz"
    tar -czf "$archive" "$name"
else
    archive="$name.zip"
    rm -f "$archive"
    zip -qr "$archive" "$name"
fi

if command -v sha256sum >/dev/null 2>&1; then sha256sum "$archive" > "$archive.sha256"
else shasum -a 256 "$archive" > "$archive.sha256"; fi

echo "Готово: $ROOT/dist/$archive"
cat "$archive.sha256"
