// The threading support header, plus the two PLATFORM names libc++abi looks for
// at this point and that openkal answers differently.
//
// ⭐ WHY HERE. `cxa_guard_impl.h` includes this unconditionally and then asks
//
//     #if defined(__APPLE__) && _LIBCPP_HAS_THREAD_API_PTHREAD
//       ... pthread_mach_thread_np(...)  → mach_port_t
//     #elif defined(SYS_gettid) && _LIBCPP_HAS_THREAD_API_PTHREAD
//       ... syscall(SYS_gettid)
//     #else
//       constexpr uint32_t (*PlatformThreadID)() = nullptr;
//
// ⚠️ ALL THREE BRANCHES ARE FINE FOR openkal EXCEPT THE FIRST, WHICH IS THE ONE
// THAT MATCHES. The second routes through musl's system-call shim, which this
// ecosystem answers with `kal_task_*`; the third is a legal answer that the
// guard implementations already handle. The first names Apple's Mach kernel,
// and there is no Mach here — the platform is `openkal-macos`, and `__APPLE__`
// is a statement about the OBJECT FORMAT.
//
// ⚠️ AND IT CANNOT BE SWITCHED OFF. `_LIBCPP_HAS_THREAD_API_PTHREAD` is ours to
// set, and clearing it would remove the whole threading layer to route around
// one identity function. `_LIBCXXABI_USE_FUTEX` selects a different
// IMPLEMENTATION but does not stop this function from being DEFINED, so it
// still has to compile.
//
// ⇒ So the two names are supplied, as the platform, exactly as
// `AvailabilityMacros.h` and `mach-o/dyld.h` beside this file are. The rule the
// overlay follows throughout: replace the PLATFORM, never the library.
#ifndef OPENKAL_LIBCXX_THREAD_SUPPORT_OVERLAY
#define OPENKAL_LIBCXX_THREAD_SUPPORT_OVERLAY

#include_next <__thread/support.h>

#if defined(__APPLE__) && !defined(OPENKAL_NO_MACH_SHIM)
#  include <cstdint>
#  include <pthread.h>

// Apple's own width, which `cxa_guard_impl.h` static_asserts against.
typedef std::uint32_t mach_port_t;

// A value that is the same for one execution context and different between two.
//
// ⚠️ THAT IS THE WHOLE CONTRACT AT THIS CALL SITE. The identity is used to
// notice that a guarded initialisation has re-entered itself — it is compared
// for equality and never for anything else, never published, and never handed
// to the system. Apple's answer is a Mach port name because Mach is what
// identifies a thread there; here the descriptor's own address serves, and it
// is unique for the same reason a descriptor is.
inline mach_port_t pthread_mach_thread_np(pthread_t __p) {
    return static_cast<mach_port_t>(reinterpret_cast<std::uintptr_t>(__p));
}
#endif

#endif  // OPENKAL_LIBCXX_THREAD_SUPPORT_OVERLAY
