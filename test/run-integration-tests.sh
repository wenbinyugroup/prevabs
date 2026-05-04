#!/bin/bash
#
# Run or clean PreVABS integration tests via CTest.
#
# Usage:
#   ./test/run-integration-tests.sh        # Run all tests
#   ./test/run-integration-tests.sh t1    # Run only t1_strip tests
#   ./test/run-integration-tests.sh clean # Clean all generated outputs
#   ./test/run-integration-tests.sh clean t6  # Clean only t6_circle

MODE="run"
FILTER=""

if [[ $# -ge 1 ]]; then
    if [[ "$1" == "clean" ]]; then
        MODE="clean"
        FILTER="${2:-}"
    else
        FILTER="$1"
    fi
fi

repoRoot="$(cd "$(dirname "$0")/.." && pwd)"
integrationDir="$repoRoot/test/integration"
buildDir="$integrationDir/build_linux_64"
prevabs="$repoRoot/bin/prevabs"

# ── clean ────────────────────────────────────────────────────────────────────
if [[ "$MODE" == "clean" ]]; then
    for dir in "$integrationDir"/t*/; do
        [[ -d "$dir" ]] || continue
        dirName=$(basename "$dir")

        if [[ -n "$FILTER" && ! "$dirName" =~ ^"$FILTER" ]]; then
            continue
        fi

        indexFile="$dir/INDEX.txt"
        [[ -f "$indexFile" ]] || continue

        keep=("INDEX.txt")
        while IFS= read -r line; do
            if [[ "$line" =~ ^-\ +(.+)$ ]]; then
                keep+=("${BASH_REMATCH[1]}")
            fi
        done < "$indexFile"

        echo "Cleaning $dirName..."
        for f in "$dir"*; do
            [[ -f "$f" ]] || continue
            fileName=$(basename "$f")
            skip=false
            for k in "${keep[@]}"; do
                if [[ "$fileName" == "$k" ]]; then
                    skip=true
                    break
                fi
            done
            if [[ "$skip" == "false" ]]; then
                echo "  Del: $fileName"
                rm -f "$f"
            fi
        done
    done
    exit 0
fi

# ── locate cmake/ctest ───────────────────────────────────────────────────────
for cmd in cmake ctest; do
    if ! command -v "$cmd" &> /dev/null; then
        echo "Error: $cmd not found. Install CMake." >&2
        exit 1
    fi
done

# ── run ─────────────────────────────────────────────────────────────────────
if [[ ! -x "$prevabs" ]]; then
    echo "Error: prevabs not found at $prevabs" >&2
    echo "Build the project first." >&2
    exit 1
fi

# Always re-configure so INDEX.txt changes are picked up immediately.
rm -rf "$buildDir"
mkdir -p "$buildDir"

cmakeArgs=("-S" "$integrationDir" "-B" "$buildDir" "-DPREVABS:FILEPATH=$prevabs")

if command -v ninja &> /dev/null; then
    cmakeArgs+=("-G" "Ninja" "-DCMAKE_MAKE_PROGRAM=ninja")
fi

cmake "${cmakeArgs[@]}"
if [[ $? -ne 0 ]]; then
    exit $?
fi

ctestArgs=("--test-dir" "$buildDir" "--output-on-failure")
if ! command -v ninja &> /dev/null; then
    ctestArgs+=("-C" "Release")
fi
if [[ -n "$FILTER" ]]; then
    ctestArgs+=("-R" "$FILTER")
fi

ctest "${ctestArgs[@]}"
exit $?