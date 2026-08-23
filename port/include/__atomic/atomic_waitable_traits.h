// Which widths the platform's wait primitive serves natively.
//
// ⭐ THE COMPANION TO `__atomic/contention_t.h` BESIDE THIS FILE, AND BOTH ARE
// NEEDED — WHICH IS THE FINDING.
//
// Upstream answers by operating system: 4 on Linux, 4 and 8 on Apple, 8 on
// FreeBSD and Windows. `atomic.cpp` instantiates its templates at exactly these
// widths, and its platform block `static_assert`s the one it can serve.
//
// openkal's suspension primitive is `kal_task_wait(const kal_u32*, …)` — four
// bytes, on every target, because that is what the specification says it takes.
//
// ⚠️ SETTING THE CONTENTION TYPE ALONE DID NOT WORK, and the way it failed is
// worth keeping. Measured 2026-08-23: after `__cxx_contention_t` was made
// `int32_t` here, the same assertion came back unchanged —
//
//     static assertion failed due to requirement '8UL == 4'
//
// — because when `_LIBCPP_ABI_ATOMIC_WAIT_NATIVE_BY_SIZE` is on, the
// instantiation list is THIS macro and not `sizeof(__cxx_contention_t)`. Two
// headers state one fact, and fixing the one a reader finds first leaves the
// error identical. (This is the second time in this port that a fact reached
// its destination through two channels; the link line was the first.)
#ifndef OPENKAL_LIBCXX_ATOMIC_WAITABLE_TRAITS_OVERLAY
#define OPENKAL_LIBCXX_ATOMIC_WAITABLE_TRAITS_OVERLAY

#include <__config>

#if _LIBCPP_HAS_MUSL_LIBC
// The width openkal serves, stated before upstream's chain can answer by OS.
#  define _LIBCPP_NATIVE_PLATFORM_WAIT_SIZES(_APPLY) _APPLY(4)
#endif

#include_next <__atomic/atomic_waitable_traits.h>

#endif  // OPENKAL_LIBCXX_ATOMIC_WAITABLE_TRAITS_OVERLAY
