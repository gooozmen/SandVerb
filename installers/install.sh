#!/usr/bin/env bash
# Установщик SandVerb для Linux и macOS: копирует SandVerb.vst3 в папку VST3-плагинов.
#
#   ./install.sh              для текущего пользователя
#   ./install.sh --system     для всех пользователей (запросит sudo)
#   ./install.sh --uninstall  удалить (с --system удалит системную копию)
#
# SandVerb.vst3 должен лежать рядом со скриптом.
set -euo pipefail

NAME="SandVerb.vst3"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

system=0
uninstall=0
for arg in "$@"; do
    case "$arg" in
        --system)    system=1 ;;
        --uninstall) uninstall=1 ;;
        -h|--help)   sed -n '2,8p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)           echo "Неизвестный параметр: $arg (см. --help)" >&2; exit 2 ;;
    esac
done

case "$(uname -s)" in
    Linux)  user_dir="$HOME/.vst3";                          system_dir="/usr/lib/vst3" ;;
    Darwin) user_dir="$HOME/Library/Audio/Plug-Ins/VST3";    system_dir="/Library/Audio/Plug-Ins/VST3" ;;
    *)      echo "Поддерживаются только Linux и macOS. Для Windows используйте установщик SandVerb-<версия>-Windows-Setup.exe." >&2; exit 1 ;;
esac

if [ "$system" -eq 1 ]; then dest="$system_dir"; else dest="$user_dir"; fi

sudo_cmd=""
if [ "$system" -eq 1 ] && [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || { echo "Для --system нужны права root (sudo не найден)." >&2; exit 1; }
    sudo_cmd="sudo"
fi

if [ "$uninstall" -eq 1 ]; then
    if [ -e "$dest/$NAME" ]; then
        $sudo_cmd rm -rf "${dest:?}/$NAME"
        echo "Удалено: $dest/$NAME"
    else
        echo "Не найдено: $dest/$NAME (уже удалён)."
    fi
    exit 0
fi

if [ ! -d "$HERE/$NAME" ]; then
    echo "Не найден $NAME рядом со скриптом ($HERE)." >&2
    exit 1
fi

$sudo_cmd mkdir -p "$dest"
$sudo_cmd rm -rf "${dest:?}/$NAME"
$sudo_cmd cp -R "$HERE/$NAME" "$dest/$NAME"

if [ "$(uname -s)" = "Darwin" ]; then
    # плагин не нотаризован: снимаем карантин, чтобы хост смог его загрузить
    $sudo_cmd xattr -dr com.apple.quarantine "$dest/$NAME" 2>/dev/null || true
fi

echo "Установлено: $dest/$NAME"
echo "Перезапустите DAW или пересканируйте плагины."
