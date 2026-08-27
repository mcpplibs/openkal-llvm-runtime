#!/usr/bin/env bash
#
# THE ECOSYSTEM AS WRITTEN ON A BRANCH, RATHER THAN AS PUBLISHED.
#
# This package declares openkal-musl by version, which is what a published
# manifest must say. A change that spans the two repositories cannot be tested
# that way: the version named here does not exist in the index until the other
# half is released, and the run fails with
#
#     E_NOT_FOUND: package 'openkal-musl@<version>' not found in the synced index
#
# --- which reads as a mistake in this manifest and is nothing of the kind. The
# remedy is to put the working trees in place of the versions, for the whole
# graph rather than for its first edge.
#
# ⚠️ WHY THIS IS A SCRIPT AND NOT A STEP. It was a step, in one job of two, and
# the other job resolved from the index and failed exactly as above the moment
# the versions moved. Two copies of a procedure that must agree are two copies
# that will not; one file called twice cannot drift.
#
# ⚠️ THE SUBSTITUTED PATHS ARE RELATIVE, AND DELIBERATELY SO.
#
# An absolute path names a directory of one machine. A manifest carrying one has
# been committed in this ecosystem and published, and every consumer resolving
# it was handed a path that exists nowhere. Relative paths cannot express that
# mistake. They are also the only form that works unchanged on all three hosts:
# `pwd` under MSYS reports `/d/a/...`, which is not a path the engine resolves,
# so an absolute form would need a Windows-only conversion here.
#
# ⚠️ `sed -i` IS NOT PORTABLE. BSD sed, which is macOS's, reads the argument
# after -i as a backup suffix; the same command that edits a file on Linux
# consumes the next expression on macOS. In-place editing goes through a
# temporary file below for that reason.
set -euo pipefail

branch="${1:-}"
[ -n "$branch" ] || { echo "usage: ${0##*/} <branch>" >&2; exit 2; }

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

# Clones a repository of this ecosystem, preferring the branch under test when
# that repository has one of the same name.
fetch() {
    local repo="$1" dir="$2"
    rm -rf "$dir"
    git clone --quiet "https://github.com/mcpplibs/$repo.git" "$dir"
    if git -C "$dir" rev-parse --verify --quiet "origin/$branch" > /dev/null; then
        git -C "$dir" checkout --quiet "origin/$branch"
        printf '  %-22s %s %s\n' "$repo" "$branch" "$(git -C "$dir" rev-parse --short HEAD)"
    else
        printf '  %-22s default branch %s (it has no %s)\n' \
            "$repo" "$(git -C "$dir" rev-parse --short HEAD)" "$branch"
    fi
}

# In-place editing that both GNU and BSD sed perform identically.
edit() {
    local file="$1" expr="$2"
    sed -E "$expr" "$file" > "$file.substituted"
    mv "$file.substituted" "$file"
}

echo "the stack under test:"
fetch openkal-musl .musl
fetch openkal      .spec

manifests=(mcpp.toml .musl/mcpp.toml)

# ⚠️ EVERY BACKEND THE C LIBRARY NAMES, DISCOVERED RATHER THAN LISTED.
#
# openkal-musl names a backend per target --- linux, macos, windows, opensbi ---
# each conditional on the target it serves. Their versions all move with a change
# that spans these repositories, so each one left unsubstituted fails the same
# way, one link further down:
#
#     E_NOT_FOUND: package 'openkal-linux@0.6.0'    (the host build)
#     E_NOT_FOUND: package 'openkal-opensbi@0.2.0'  (the bare-metal one)
#
# The first was fixed by naming it, and the second appeared. A list written by
# hand is a list that is one entry short, so the set is read out of the manifest.
for backend in $(grep -oE '^openkal-[a-z]+ = \{ version' .musl/mcpp.toml | cut -d' ' -f1); do
    fetch "$backend" ".$backend"

    # The backend reaches the specification too, by whatever form its own
    # manifest uses. From <root>/.<backend>/ the specification is ../.spec.
    edit ".$backend/mcpp.toml" 's|^openkal = .*$|openkal = { path = "../.spec" }|'

    edit .musl/mcpp.toml \
        "s|^$backend = \\{ version = \"[^\"]*\"(.*)\$|$backend = { path = \"../.$backend\"\\1|"
    grep -q "path = \"../.$backend\"" .musl/mcpp.toml \
        || { echo "::error::$backend was not substituted"; exit 1; }

    manifests+=(".$backend/mcpp.toml")
done

edit .musl/mcpp.toml 's|^openkal = .*$|openkal = { path = "../.spec" }|'
edit mcpp.toml       's|^openkal-musl = .*$|openkal-musl = { path = "./.musl" }|'

# The substitution is asserted rather than assumed. One that matched nothing
# would leave the manifest naming a version, the resolver would fetch a
# published C library, and the run would report on that one while appearing to
# report on this branch.
grep -q 'path = "./.musl"'  mcpp.toml \
    || { echo "::error::the C library substitution matched nothing"; exit 1; }
grep -q 'path = "../.spec"' .musl/mcpp.toml \
    || { echo "::error::the specification substitution matched nothing"; exit 1; }

# ⭐ THE CHECK THAT WOULD HAVE CAUGHT EVERY FAILURE ABOVE AT ITS FIRST OCCURRENCE:
# nothing anywhere in the substituted graph still names a version. The three
# defects this file records were each found by a build failing one link further
# down than the last; this asks the whole graph at once.
if grep -nE '^openkal[a-z-]* = ("|\{ version)' "${manifests[@]}"; then
    echo "::error::something in the graph still names a version rather than a tree"
    exit 1
fi

echo "the whole stack names working trees; nothing in it names a version"
