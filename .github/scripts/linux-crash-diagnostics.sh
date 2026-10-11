#!/usr/bin/env bash
set -euo pipefail

mode=${1:?Expected sanitized or unsanitized}
case "$mode" in
    sanitized|unsanitized) ;;
    *) echo "Unknown test mode: $mode" >&2; exit 1 ;;
esac

destination="diagnostics/$mode"
mkdir -p "$destination/cores"

# Preserve the exact binaries and objects before another test mode rebuilds them.
tar -czf "$destination/build.tar.gz" Bin build
{
    git rev-parse HEAD
    uname -a
    "${CI_COMPILER:?Expected CI_COMPILER}" --version
    gdb --version
    printf 'ASAN_OPTIONS=%s\n' "${ASAN_OPTIONS-}"
} > "$destination/environment.txt" 2>&1

shopt -s nullglob
cores=("$RUNNER_TEMP/cores/$mode/"*)
if ((${#cores[@]} == 0)); then
    echo 'No core dumps were produced; see tests.log for sanitizer reports or build failures.' |
        tee "$destination/no-cores.txt"
fi

for core in "${cores[@]}"; do
    name=${core##*/}
    # core_pattern is %p.%E; %E encodes the full executable path with ! for /.
    encoded_executable=${name#*.}
    executable=${encoded_executable//\!/\/}
    trace="$destination/$name.backtrace.txt"
    {
        printf 'Executable: %s\nCore: %s\n' "$executable" "$core"
        if [[ -f "$executable" ]]; then
            timeout 120s gdb --batch --nx "$executable" "$core" \
                -ex 'set pagination off' \
                -ex 'thread apply all bt full' \
                -ex 'info registers' \
                -ex 'x/16i $pc' \
                -ex 'info sharedlibrary' || true
            ldd "$executable" || true
        else
            echo 'Executable no longer exists at the path recorded in the core filename.'
        fi
    } > "$trace" 2>&1
    cat "$trace"
    mv "$core" "$destination/cores/"
done
