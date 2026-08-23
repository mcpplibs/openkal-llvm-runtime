// ⭐⭐ libc++ 的平台后端按 OS 宏选,而 openkal 的答案按「配置的是哪个 C 库」。
//
// THE OVERLAY, NOT AN EDIT. openkal-musl's `port/` carries every difference
// from vendored musl in one directory that shadows it on the include path, and
// this is that arrangement for libc++ — the vendored tree stays byte-identical
// to upstream, and the whole of the difference is this file.
//
// WHAT UPSTREAM DOES, AND WHY IT IS RIGHT EVERYWHERE ELSE
//
// `__locale_dir/locale_base_api.h` selects a backend by asking which operating
// system this is, and on each answer it assumes that system's C library:
//
//     #if defined(__APPLE__)      → support/apple.h
//     #elif defined(__linux__)    → support/linux.h
//     …
//
// That holds wherever libc++ is normally built, because the OS and its C
// library travel together.
//
// WHY THEY DO NOT TRAVEL TOGETHER HERE
//
// openkal's arrangement is that the C library is a PACKAGE — openkal-musl — and
// the platform beneath it is an implementation of a 48-function interface. A
// program for Apple's object format built this way has `__APPLE__` defined,
// because that is a statement about the FORMAT and the ABI, and has musl
// underneath it. Upstream's first question then gets the right answer to the
// wrong question:
//
//     bsd_like.h:203: no member named 'asprintf_l' in the global namespace
//
// — Apple's locale extensions, which musl does not have and never claimed to.
//
// ⚠️ WHY THIS IS NOT EXPRESSIBLE AS A `__config_site` SWITCH, WHICH WAS TRIED
// FIRST. `__config_site` is where this package states every other decision
// (which C library, whether there is a filesystem, whether there is a random
// device), and it is the right place for anything libc++ offers a knob for.
// libc++ offers none here: the chain is a fixed `#if defined(__APPLE__)` with
// no override, and the only related knob — `_LIBCPP_HAS_LOCALIZATION 0` —
// removes `<locale>` entirely rather than choosing a different backend.
// Removing a working facility to route around a question of taste is a worse
// trade than shadowing one header.
//
// ⇒ `_LIBCPP_HAS_MUSL_LIBC` comes from this package's own `__config_site`, so
// it is a CONFIGURED FACT rather than a guess, and it is asked first. The
// backend it selects is the one a musl build already uses on the other object
// format — nothing new is written, the existing answer is merely reachable.
//
// Measured 2026-08-23: with this in place, libc++'s `std` module precompiles
// for `arm64-apple-macos14.0` on a Linux host.
#ifndef OPENKAL_LIBCXX_LOCALE_BASE_API_OVERLAY
#define OPENKAL_LIBCXX_LOCALE_BASE_API_OVERLAY

#include <__config>

// ⭐ THE PREDICATE IS THE C LIBRARY, FULL STOP — NOT "the C library, on Apple".
//
// It was `&& defined(__APPLE__)` at first, because Apple was the target that
// exposed it. Measured 2026-08-23 on the next one: `x86_64-windows-gnu` over
// openkal takes `_LIBCPP_MSVCRT_LIKE` → `support/windows.h` and stops on
// `_locale_t`, which is Microsoft's C runtime, for a program whose C library
// is musl. Same shape, second object format.
//
// ⇒ Narrowing the rule to one platform would have meant writing it again per
// platform. The question upstream asks is which C library is beneath; this
// answers it, and every target where the answer is musl now reaches the same
// backend — which on ELF is the one it already reached.
#if _LIBCPP_HAS_LOCALIZATION && _LIBCPP_HAS_MUSL_LIBC
#  include <__locale_dir/support/linux.h>
#else
#  include_next <__locale_dir/locale_base_api.h>
#endif

#endif  // OPENKAL_LIBCXX_LOCALE_BASE_API_OVERLAY
