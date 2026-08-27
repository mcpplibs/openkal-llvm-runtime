#!/usr/bin/env bash
#
# THE mcpp THIS RUN IS TO BE JUDGED BY, PUT ON PATH.
#
# Two things are wanted of the same procedure, and which one applies is decided
# by MCPP_SOURCE_REF rather than by which job is asking:
#
#   unset  --- the released engine the pin names, which is what an ordinary run
#              of this repository tests.
#   set    --- the engine built from that reference, which is how a change to
#              mcpp is validated against this ecosystem before it is merged.
#
# ⚠️ THE PIN MAY NAME A RELEASE THIS RUN IS VALIDATING, and so may not exist. In
# that case the bootstrap takes whatever the index has; it is only the compiler
# that compiles the compiler, and the build it produces is what goes on PATH.
#
# ⚠️ WHY THIS IS A SCRIPT AND NOT A STEP. It was a step in one job of this
# workflow and absent from another, so that job installed a released engine and
# built manifests written for an unreleased one. The engine accepts a manifest
# key it does not know without failing, so the mismatch does not announce
# itself: the build proceeds and the semantics the key asks for are absent.
set -euo pipefail

: "${MCPP_VERSION:?the pin this repository tests against}"

if [ -n "${MCPP_SOURCE_REF:-}" ]; then
    xlings update > /dev/null 2>&1 || true
    xlings install mcpp -y -g
else
    for attempt in 1 2 3 4 5 6; do
        xlings update > /dev/null 2>&1 || true
        if xlings install "mcpp@$MCPP_VERSION" -y -g; then break; fi
        if [ "$attempt" = 6 ]; then
            echo "::error::mcpp@$MCPP_VERSION never appeared in the index"
            exit 1
        fi
        sleep 60
    done
fi
mcpp --version
mcpp self config --mirror GLOBAL

[ -n "${MCPP_SOURCE_REF:-}" ] || exit 0

# CROSS-VALIDATION: BUILD THE mcpp UNDER REVIEW AND USE THAT ONE.
#
# The released mcpp installed above is the bootstrap that compiles it; mcpp
# builds itself and there is no other compiler for it here.
src="${RUNNER_TEMP:-/tmp}/mcpp-src"
[ -d "$src" ] || git clone --quiet --depth 1 \
    --branch "$MCPP_SOURCE_REF" \
    https://github.com/mcpp-community/mcpp.git "$src"

# The clone's own workspace pin must not decide which mcpp builds it. The
# `.xlings.json` at mcpp's root pins the mcpp that compiles mcpp and does not
# move when mcpp is released, so a build inside the checkout obeys it and tries
# to install a version the index may no longer carry. What is wanted is the
# source compiled by the mcpp installed above, which is what removing it leaves.
rm -f "$src/.xlings.json"

# ⚠️ THE PRODUCT OF THIS BUILD IS IDENTIFIED BY ABSENCE, NOT BY RECENCY.
#
# `target/` holds one directory per configuration and accumulates them, and a
# restored cache writes every timestamp to the moment of extraction. Both
# `find | head -1` and `ls -t | head -1` have selected a stale binary here; the
# one chosen had an interpreter naming a payload that no longer existed, and the
# job failed with `cannot execute: required file not found`, which reads as a
# broken commit and is nothing of the kind. Deleting first makes what remains
# the product of this build, and the count is asserted rather than assumed.
find "$src/target" -type f \( -name mcpp -o -name mcpp.exe \) -delete 2>/dev/null || true
( cd "$src" && mcpp build --release )

built=$(find "$src/target" -type f \( -name mcpp -o -name mcpp.exe \))
count=$(printf '%s\n' "$built" | grep -c . || true)
if [ "$count" != 1 ]; then
    echo "::error::expected exactly one binary from $MCPP_SOURCE_REF, found $count"
    printf '%s\n' "$built" | sed 's/^/        /'
    exit 1
fi

echo "$(cd "$(dirname "$built")" && pwd)" >> "$GITHUB_PATH"
echo "under review: $("$built" --version)  (from $MCPP_SOURCE_REF)"

# ⚠️ THE APPEND ABOVE IS NOT EVIDENCE THAT THE APPEND TOOK EFFECT.
#
# `GITHUB_PATH` governs the steps that follow, so nothing observable in this one
# can distinguish a directory that wins from a directory that is ignored, and a
# build with the wrong engine looks exactly like a build with the right one
# until something depends on the difference. This is not a hypothetical form of
# doubt on every host: `pwd` under MSYS reports `/d/a/...`, and whether the
# runner accepts that spelling is a property of the runner rather than of this
# script.
#
# The version is therefore carried forward so that a following step can compare
# it against what PATH resolves. That step is the criterion; this line only
# supplies the value it needs.
echo "MCPP_UNDER_REVIEW=$("$built" --version | awk '{print $2}')" >> "$GITHUB_ENV"
