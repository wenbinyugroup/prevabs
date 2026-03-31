#!/usr/bin/env bash
# Build the Windows MSVC executable from WSL by delegating to PowerShell.
#
# Usage:
#   ./tools/build-wsl.sh full    # clean build from scratch
#   ./tools/build-wsl.sh fast    # incremental rebuild (default)
#   ./tools/build-wsl.sh clean   # remove build directory only
#
# Requires: powershell.exe accessible from WSL (standard on Windows 10/11)

set -euo pipefail

MODE="${1:-fast}"

case "$MODE" in
  full|fast|clean) ;;
  *)
    echo "Usage: $0 [full|fast|clean]" >&2
    exit 1
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

WIN_REPO_ROOT="$(wslpath -w "$REPO_ROOT")"
WIN_SCRIPT="$(wslpath -w "$SCRIPT_DIR/build-msvc.ps1")"

echo "=== Building Windows .exe from WSL (mode: $MODE) ==="
echo "    Repo:   $WIN_REPO_ROOT"
echo "    Script: $WIN_SCRIPT"

powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -Command "Set-Location '$WIN_REPO_ROOT'; & '$WIN_SCRIPT' -Mode $MODE"
