//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "abort_message.h"
#include "cxxabi.h"
#include <__thread/support.h>
#ifndef _LIBCXXABI_HAS_NO_THREADS
#if defined(__ELF__) && defined(_LIBCXXABI_LINK_PTHREAD_LIB)
#pragma comment(lib, "pthread")
#endif
#endif

#include <stdlib.h>

namespace __cxxabiv1 {

  using Dtor = void(*)(void*);

  extern "C"
#ifndef HAVE___CXA_THREAD_ATEXIT_IMPL
  // A weak symbol is used to detect this function's presence in the C library
  // at runtime, even if libc++ is built against an older libc
  _LIBCXXABI_WEAK
#endif
  int __cxa_thread_atexit_impl(Dtor, void*, void*);

#ifndef HAVE___CXA_THREAD_ATEXIT_IMPL

namespace {
  // This implementation is used if the C library does not provide
  // __cxa_thread_atexit_impl() for us.  It has a number of limitations that are
  // difficult to impossible to address without ..._impl():
  //
  // - dso_symbol is ignored.  This means that a shared library may be unloaded
  //   (via dlclose()) before its thread_local destructors have run.
  //
  // - thread_local destructors for the main thread are run by the destructor of
  //   a static object.  This is later than expected; they should run before the
  //   destructors of any objects with static storage duration.
  //
  // - thread_local destructors on non-main threads run on the first iteration
  //   through the __libccpp_tls_key destructors.
  //   std::notify_all_at_thread_exit() and similar functions must be careful to
  //   wait until the second iteration to provide their intended ordering
  //   guarantees.
  //
  // Another limitation, though one shared with ..._impl(), is that any
  // thread_locals that are first initialized after non-thread_local global
  // destructors begin to run will not be destroyed.  [basic.start.term] states
  // that all thread_local destructors are sequenced before the destruction of
  // objects with static storage duration, resulting in a contradiction if a
  // thread_local is constructed after that point.  Thus we consider such
  // programs ill-formed, and don't bother to run those destructors.  (If the
  // program terminates abnormally after such a thread_local is constructed,
  // the destructor is not expected to run and thus there is no contradiction.
  // So construction still has to work.)

  struct DtorList {
    Dtor dtor;
    void* obj;
    DtorList* next;
  };

  // THE LIST LIVES IN THE TLS KEY'S OWN VALUE, NOT IN A `__thread` VARIABLE.
  //
  // Upstream keeps `__thread DtorList* dtors` here and passes the key a dummy
  // value purely to arm this destructor. That is correct wherever the compiler
  // emits native thread-local storage, and WRONG wherever it emits EMULATED
  // TLS --- which is every PE target this package builds, since the platform's
  // own `_tls_index` machinery is not available over openkal and the runtime
  // is built with `-femulated-tls`.
  //
  // MEASURED, on `x86_64-windows-gnu` under wine. Registration and the key
  // destructor run on the same thread and see different storage:
  //
  //     registered dtor, dtors=0x7ffffe994680, &dtors=0x7ffffe9946a8
  //     run_dtors called, dtors=0,             &dtors=0x7ffffe9946c8
  //
  // emutls keeps its per-thread blocks behind a pthread key of its own, and
  // that key's destructor had already released this thread's block. A read
  // after that point allocates a fresh, ZEROED one --- a different address
  // every time, as the two calls above show. So `run_dtors` walked an empty
  // list and every `thread_local` destructor was silently skipped: the link
  // succeeded, the program ran, and nothing happened.
  //
  // KEY DESTRUCTOR ORDER IS WHY AN EARLIER PROBE EXONERATED emutls. musl
  // iterates keys in creation order, so a probe whose own key is created
  // BEFORE the first thread-local access sees emutls still alive in its
  // destructor and reads the expected value. Here the order is the other way
  // round, and a probe cannot report an ordering it was built to avoid.
  //
  // The fix needs no new mechanism: the key destructor is already handed the
  // key's value, and a list kept there cannot be affected by any other key's
  // teardown. `dtors_alive` goes with it --- a non-null value IS the list.
  std::__libcpp_tls_key dtors_key;

  void run_dtors(void* p) {
    auto head = static_cast<DtorList*>(p);
    // Cleared BEFORE the walk, so a destructor that itself constructs a
    // `thread_local` starts a fresh list rather than appending to the one
    // being walked. POSIX may then call this destructor again for that list,
    // which is the behaviour upstream's `dtors_alive` reset also allowed.
    std::__libcpp_tls_set(dtors_key, nullptr);
    while (head) {
      auto next = head->next;
      head->dtor(head->obj);
      ::free(head);
      head = next;
    }
  }

  struct DtorsManager {
    DtorsManager() {
      // There is intentionally no matching std::__libcpp_tls_delete call, as
      // __cxa_thread_atexit() may be called arbitrarily late (for example, from
      // global destructors or atexit() handlers).
      if (std::__libcpp_tls_create(&dtors_key, run_dtors) != 0) {
        __abort_message("std::__libcpp_tls_create() failed in __cxa_thread_atexit()");
      }
    }

    ~DtorsManager() {
      // std::__libcpp_tls_key destructors do not run on threads that call exit()
      // (including when the main thread returns from main()), so we explicitly
      // call the destructor here.  This runs at exit time (potentially earlier
      // if libc++abi is dlclose()'d).  Any thread_locals initialized after this
      // point will not be destroyed.
      run_dtors(std::__libcpp_tls_get(dtors_key));
    }
  };

  // THE FALLBACK'S BODY, NAMED so that the two callers are one implementation
  // rather than two copies that drift apart.
  int register_dtor(Dtor dtor, void* obj) {
    // Initialize the dtors std::__libcpp_tls_key (uses __cxa_guard_*() for
    // one-time initialization and __cxa_atexit() for destruction)
    static DtorsManager manager;

    auto head = static_cast<DtorList*>(::malloc(sizeof(DtorList)));
    if (!head) {
      return -1;
    }

    head->dtor = dtor;
    head->obj = obj;
    head->next = static_cast<DtorList*>(std::__libcpp_tls_get(dtors_key));
    if (std::__libcpp_tls_set(dtors_key, head) != 0) {
      ::free(head);
      return -1;
    }

    return 0;
  }
} // namespace

#endif // HAVE___CXA_THREAD_ATEXIT_IMPL

// THE GUARD ASKS WHICH OPERATING SYSTEM, AND THE QUESTION IS WHETHER ANOTHER
// C++ RUNTIME IS IN THE IMAGE.
//
// Upstream exports `__cxa_thread_atexit` on Linux and Fuchsia only, because
// everywhere else somebody else already does --- on an ordinary MinGW target
// it is `libmingw32.a`, which defines exactly one such symbol. openkal
// replaces the C library AND its runtime, so both sides assumed the other
// would supply it and neither did: `ld.lld: error: undefined symbol:
// __cxa_thread_atexit`, measured on two members of the index's compatibility
// suite (doctest, spdlog) on `x86_64-windows-gnu`.
//
// `OPENKAL_TARGET_WINDOWS` IS THIS PACKAGE'S OWN DEFINE, not the engine's
// `__MCPP_TARGET_WINDOWS__`, and the difference is whose compile reads the
// file: this one is a source of this package and is never installed, so a
// package-private define reaches it. The same distinction chose the define in
// `compiler-rt/lib/builtins/int_lib.h`.
//
// THE FALLBACK BELOW WAS ALREADY COMPLETE; only the export was missing. What
// was NOT complete is where it kept its list --- see `run_dtors` above.
#if defined(__linux__) || defined(__Fuchsia__) \
    || defined(OPENKAL_TARGET_WINDOWS) || defined(__APPLE__)
extern "C" {

  _LIBCXXABI_FUNC_VIS int __cxa_thread_atexit(Dtor dtor, void* obj, void* dso_symbol) throw() {
#ifdef HAVE___CXA_THREAD_ATEXIT_IMPL
    return __cxa_thread_atexit_impl(dtor, obj, dso_symbol);
#elif defined(__APPLE__)
    // NO WEAK PROBE ON MACH-O, AND THE REASON IS THE OBJECT FORMAT.
    //
    // The `else` branch below declares `__cxa_thread_atexit_impl` weak and
    // tests it against null. That is an ELF idiom: an unresolved weak symbol
    // there IS zero. ld64 does not do that in a static link, so the probe
    // itself becomes the error --- measured building `examples/cxx` for
    // `aarch64-macos`:
    //
    //     ld64.lld: error: undefined symbol: __cxa_thread_atexit_impl
    //
    // The test is also pointless here: what would define that symbol is
    // libSystem, and libSystem is what this stack replaces.
    (void)dso_symbol;
    return register_dtor(dtor, obj);
#else
    if (__cxa_thread_atexit_impl) {
      return __cxa_thread_atexit_impl(dtor, obj, dso_symbol);
    } else {
      return register_dtor(dtor, obj);
    }
#endif // HAVE___CXA_THREAD_ATEXIT_IMPL
  }

#if defined(__APPLE__)
  // APPLE ASKS FOR THE SAME THING UNDER A DIFFERENT NAME, and supplies it from
  // libSystem --- which is exactly what openkal replaces.
  //
  // clang lowers a `thread_local` with a non-trivial destructor to a call to
  // `_tlv_atexit` on Mach-O and to `__cxa_thread_atexit` elsewhere, so the
  // Windows gap this file was opened for has an Apple twin, invisible until
  // something in the examples declared such a variable:
  //
  //     ld64.lld: error: undefined symbol: _tlv_atexit
  //
  // The two differ only in the dso handle, which the fallback ignores. There
  // is nothing to implement a second time.
  _LIBCXXABI_FUNC_VIS void _tlv_atexit(Dtor dtor, void* obj) {
    __cxa_thread_atexit(dtor, obj, nullptr);
  }
#endif

} // extern "C"
#endif // defined(__linux__) || defined(__Fuchsia__)
} // namespace __cxxabiv1
