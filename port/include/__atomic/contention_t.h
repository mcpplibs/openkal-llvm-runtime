// The width `std::atomic`'s contention counter has — which is the width the
// platform's wait primitive takes, and openkal's is four bytes.
//
// ⭐ THE SAME QUESTION AS EVERY OTHER OVERLAY HERE, AND IT IS THE ONE THAT
// DECIDES WHICH `__platform_wait_on_address<N>` GETS INSTANTIATED.
//
// Upstream picks by operating system — `int32_t` on Linux, `int64_t` on Apple,
// FreeBSD and Windows — because each of those has a wait primitive of that
// width. It is not a taste: `atomic.cpp` instantiates the templates at
// `sizeof(__cxx_contention_t)` and the platform block `static_assert`s the size
// it can serve.
//
// openkal's is `kal_task_wait(const kal_u32*, kal_u32, kal_u64)` — four bytes,
// on every target, because that is what the specification says the suspension
// primitive takes. So the answer here follows the C library, exactly as the
// locale backend and the CRT selection do.
//
// ⚠️ Measured 2026-08-23, after `atomic.cpp`'s platform block was replaced but
// before this file existed:
//
//     static assertion failed due to requirement '8UL == 4':
//         Can only wait on 4 bytes value
//
// — the host build passed and the other two targets did not, because on Linux
// upstream had already chosen four and elsewhere it had chosen eight. The
// failure named the assertion rather than the type that produced the eight.
//
// ⚠️ AND THIS IS AN ABI DECISION. `__cxx_contention_t` appears in the mangled
// names of the four exported entry points, so a program and a libc++ that
// disagree about it do not link. That is the right failure — and it is why this
// belongs in a header both sides read rather than in a flag one side passes.
#ifndef OPENKAL_LIBCXX_CONTENTION_T_OVERLAY
#define OPENKAL_LIBCXX_CONTENTION_T_OVERLAY

#include <__config>

// ⚠️ THE CONDITION IS THE C LIBRARY ALONE. It used to also require
// `_LIBCPP_ABI_ATOMIC_WAIT_NATIVE_BY_SIZE`, mirroring upstream's own structure
// — and measured, the overlay was reached and took the other branch, because
// that macro is not defined at the point this header is first pulled in. The
// fact this file states is true either way: openkal's wait primitive is four
// bytes wide whether or not the ABI selects per size.
#if _LIBCPP_HAS_MUSL_LIBC

#  include <__atomic/support.h>
#  include <cstdint>

_LIBCPP_BEGIN_NAMESPACE_STD
using __cxx_contention_t _LIBCPP_NODEBUG   = int32_t;
using __cxx_atomic_contention_t _LIBCPP_NODEBUG = __cxx_atomic_impl<__cxx_contention_t>;
_LIBCPP_END_NAMESPACE_STD

#else
#  include_next <__atomic/contention_t.h>
#endif

#endif  // OPENKAL_LIBCXX_CONTENTION_T_OVERLAY
