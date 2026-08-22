//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
// Abstract interface to shared reader/writer log, hiding platform and
// configuration differences.
//
//===----------------------------------------------------------------------===//

#ifndef __RWMUTEX_HPP__
#define __RWMUTEX_HPP__

// ─── openkal ─── BEGIN
//
// The platform surface of this file is a read-write lock, and upstream picks it
// by operating system: SRW locks on `_WIN32`, pthreads everywhere else. openkal
// makes that question have one answer — `pthread_rwlock_*` is what is beneath on
// every target, because musl is, and musl's rwlock is a futex, and the futex is
// `kal_task_wait` / `kal_task_wake`. The same route `libcxx/src/atomic.cpp`
// takes, and the same reason it takes it.
//
// ⚠️ THE INCLUDE IS OUTSIDE THE THREADS CHECK, WHICH IS WHY THIS IS A PATCH AND
// NOT A FLAG. Upstream reaches `<windows.h>` on `_WIN32` BEFORE asking whether
// this build has threads at all — so `_LIBUNWIND_HAS_NO_THREADS`, which would
// otherwise make this file need nothing, does not prevent the include.
#if defined(_WIN32) && !defined(OPENKAL)
#include <windows.h>
#elif !defined(_LIBUNWIND_HAS_NO_THREADS)
// ─── openkal ─── END
#include <pthread.h>
#if defined(__ELF__) && defined(_LIBUNWIND_LINK_PTHREAD_LIB)
#pragma comment(lib, "pthread")
#endif
#endif

namespace libunwind {

#if defined(_LIBUNWIND_HAS_NO_THREADS)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return true; }
  bool unlock_shared() { return true; }
  bool lock() { return true; }
  bool unlock() { return true; }
};

// ─── openkal ─── BEGIN
// ⚠️ THE SECOND OF TWO. The same fact — "this is PE, so use SRW locks" — is
// stated twice in this file, once to pick the include and once to pick the
// class. Withdrawing only the first leaves the class referring to `SRWLOCK`
// with nothing declaring it.
#elif defined(_WIN32) && !defined(OPENKAL)
// ─── openkal ─── END

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() {
    AcquireSRWLockShared(&_lock);
    return true;
  }
  bool unlock_shared() {
    ReleaseSRWLockShared(&_lock);
    return true;
  }
  bool lock() {
    AcquireSRWLockExclusive(&_lock);
    return true;
  }
  bool unlock() {
    ReleaseSRWLockExclusive(&_lock);
    return true;
  }

private:
  SRWLOCK _lock = SRWLOCK_INIT;
};

#elif !defined(LIBUNWIND_USE_WEAK_PTHREAD)

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() { return pthread_rwlock_rdlock(&_lock) == 0;  }
  bool unlock_shared() { return pthread_rwlock_unlock(&_lock) == 0; }
  bool lock() { return pthread_rwlock_wrlock(&_lock) == 0; }
  bool unlock() { return pthread_rwlock_unlock(&_lock) == 0; }

private:
  pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};

#else

extern "C" int __attribute__((weak))
pthread_create(pthread_t *thread, const pthread_attr_t *attr,
               void *(*start_routine)(void *), void *arg);
extern "C" int __attribute__((weak))
pthread_rwlock_rdlock(pthread_rwlock_t *lock);
extern "C" int __attribute__((weak))
pthread_rwlock_wrlock(pthread_rwlock_t *lock);
extern "C" int __attribute__((weak))
pthread_rwlock_unlock(pthread_rwlock_t *lock);

// Calls to the locking functions are gated on pthread_create, and not the
// functions themselves, because the data structure should only be locked if
// another thread has been created. This is what similar libraries do.

class _LIBUNWIND_HIDDEN RWMutex {
public:
  bool lock_shared() {
    return !pthread_create || (pthread_rwlock_rdlock(&_lock) == 0);
  }
  bool unlock_shared() {
    return !pthread_create || (pthread_rwlock_unlock(&_lock) == 0);
  }
  bool lock() {
    return !pthread_create || (pthread_rwlock_wrlock(&_lock) == 0);
  }
  bool unlock() {
    return !pthread_create || (pthread_rwlock_unlock(&_lock) == 0);
  }

private:
  pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};

#endif

} // namespace libunwind

#endif // __RWMUTEX_HPP__
