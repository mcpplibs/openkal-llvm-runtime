# openkal-llvm-runtime

LLVM's C++ runtime — libc++, libc++abi and libunwind — configured for
[openkal-musl](https://github.com/mcpplibs/openkal-musl) rather than for a host
C library.

```toml
[dependencies]
openkal-llvm-runtime = "0.4.0"
```

A C++ standard library is not portable in the way a program is. It is
*configured* for one C library and compiled against that library's headers, and
a build that merely finds the headers is not the same thing. The criterion is
the one openkal keeps returning to: **whether an implementation has been
configured for this target**, not whether its headers can be found.

## What is vendored, and what is not

| | |
| --- | --- |
| **vendored** | `libcxx`, `libcxxabi`, `libunwind`, and `compiler-rt`'s builtins, at the revision the toolchain itself was built from (`llvm/UPSTREAM-REV`) |
| **not vendored, not changed** | the compiler and the linker |

The compiler takes a target triple and emits for it; openkal changes no
architecture and no object format, so it needs nothing. What has to be
configured is what travels with the program, and that is what is here.

Building it takes minutes rather than hours, because these are the pieces LLVM
itself builds separately from the compiler.

## The one thing the configuration decides

`llvm-generated/generic/__config_site` is libc++'s configure product, and every
position in it is a **claim about the environment beneath**:

```c
#define _LIBCPP_HAS_MUSL_LIBC     1   /* the C library beneath is musl's   */
#define _LIBCPP_HAS_RANDOM_DEVICE 1   /* openkal.random reaches a source   */
#define _LIBCPP_HAS_FILESYSTEM    1   /* openkal.fs, and its sources built */
#define _LIBCPP_HAS_TERMINAL      1   /* isatty answers, rather than lying */
```

The first was measured rather than assumed. With it at `0` — the value a
toolchain configured for glibc ships — a translation unit that includes
`<vector>` fails with twenty errors, of which the first names the cause:

```
__locale:439: error: unknown rune table for this platform
              -- do you mean to define _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE?
```

With it at `1`, none.

⚠️ **A claim that drifts from what the port provides fails neither the build nor
the link.** It produces a program that takes a path the environment cannot
support, and reports nothing. `_LIBCPP_HAS_RANDOM_DEVICE` was `0` until openkal
gained `openkal.random`; `_LIBCPP_HAS_TERMINAL` was `1` while every `isatty`
over the port beneath returned `0` — for a real terminal as readily as for a
pipe — so `std::print` never took its terminal path and nothing failed. The
remedy there was to repair the port rather than withdraw the claim, and the
workflow now reconciles the two rather than checking once and assuming
afterwards.

## Two configurations, and why there are two

`llvm-generated/` carries one directory per configuration, which is the shape
`musl-generated/` already has and for the same reason: these are configure
products, and a package with no configure step commits them.

| | `generic` | `freestanding` |
| --- | --- | --- |
| threads | libc++ recognises the system and finds pthread itself | stated, because an unrecognised system makes libc++ stop with `No thread API` rather than guess |
| filesystem | yes | no — and the sources that implement it are then not compiled, which is what libc++'s own build does with the same switch |
| terminal | yes | no |

A target with no operating system also needs two switches a hosted one answers
by recognising the system: the unwinder's use of `dladdr` (there is no dynamic
loader, and the type its interface names does not exist), and `_GNU_SOURCE`
(libc++abi reaches `syscall` for a thread identity, and musl declares that name
only under it — LLVM's own runtimes build defines it for the same reason).

**Measured 2026-08-22**: `mcpp build --target riscv64-none-elf` produces 1431
objects, `ELF 64-bit LSB relocatable, UCB RISC-V`, with `__cxa_throw` and
`__cxa_begin_catch` defined.

⚠️ `import std;` on such a target is still refused, and not by this package:

```
error: `import std;` is not available on 'riscv64-none-elf'
       --- a freestanding target has no hosted standard library.
```

That message states a premise this package makes false. Lifting it is a change
to the build tool, which asks whether the *target* is freestanding rather than
whether a hosted standard library is *present*.

## What a program gets

`examples/cxx` asserts it, and every assertion is written so that it can fail:

- containers and algorithms — `std::vector` sorted by `std::sort`
- strings — built and searched
- values returned through several frames
- **an exception thrown across three frames and caught**
- **a destructor run while the stack is unwound**
- `hidden`, `weak` and `weak_alias` as the program's own identifiers — the C++
  half of `mcpplibs/openkal-musl#13`, where `hidden` was given C *linkage*
  rather than emptied and so failed differently from the C case
- `std::filesystem` — a directory created, a file written, enumerated, copied,
  its size reported, and the whole removed
- **`std::filesystem::permissions` and `create_symlink` refused, not ignored** —
  openkal carries a boolean `writable` rather than a mode word, and has no
  operation that creates a link. A probe checking only the supported operations
  would pass just as well for a port that silently accepted these two
- `std::random_device` — three draws that **differ**, which a source stuck at a
  constant would not satisfy and "a number was produced" would

`examples/import-std` asserts the other half: `import std;` — the module, not the
headers — with `std::ranges::sort` and `std::println`.

## ⚠️ The observation a build cannot make

The last two assertions are the package. Everything else here a runtime that was
linked but never worked would also satisfy.

This is not a precaution. While this package was written, the example printed its
first line and then

```
libc++abi: terminating due to uncaught exception of type std::runtime_error
```

with `_Unwind_Backtrace` walking **zero frames**, and with the compile, the link,
and every path that does not throw green. Nothing in that report named the
cause, and the cause was two layers down: openkal-musl's replaced
`__libc_start_main` does not read the auxiliary vector, so `dl_iterate_phdr`
answered with one object having **no program headers** — and the unwinder,
looking there for `PT_GNU_EH_FRAME`, concluded the program had no frame
descriptions rather than that it had not been told. openkal-musl now answers
that enquiry from `__ehdr_start`.

A package that had only ever been built would have shipped that.

## Four targets, one source

`examples/same-source` is built for four machines from one `src/main.cpp` with
nothing edited between them, and it prints the same four lines on each:

| target | format | how it is run |
|---|---|---|
| `x86_64-linux-gnu` | ELF | directly |
| `x86_64-windows-gnu` | PE32+ | on a Windows machine |
| `aarch64-macos` | Mach-O | on an arm64 Mac |
| `riscv64-none-elf` | ELF | QEMU with real OpenSBI firmware |

The fourth is the one that cannot pass by accident. A hosted target has a C
library, a C++ runtime and an unwinder already installed, so a program that
reaches one of them by mistake still works. There is nothing to reach on the
fourth: the C library is openkal-musl, the standard library and the unwinder are
this package's, and beneath them is firmware whose whole interface is `ecall`.

⚠️ **And the artefacts are run on the real machines, not inspected.** Continuous
integration builds all three hosted targets on Linux and executes each on the
system it was built for; `mcpp`'s own `openkal-cross` workflow builds them from
three hosts and runs all nine combinations. Every difference this package had to
find on Mach-O and PE — the loader-bootstrapped thread-local, the unwinder's
search for its own tables, the personality routine, the visibility of weak
definitions — **links successfully and fails at run time**.

## Not supported

- **the sanitizers.** compiler-rt's sanitizers depend on a host's internals —
  memory layout, interceptors, symbolisation — and are outside this package.
- **`std::random_device`**, for the reason above.

## Licence

The port is Apache-2.0. The vendored sources under `llvm/` are Apache-2.0 with
LLVM exceptions; `llvm/LICENSE.TXT` is theirs.

⚠️ They are **almost** unchanged, and the exceptions are enumerated rather than
described. `llvm/PATCHES.md` lists every one — five regions in four files, each
between `// ─── openkal ─── BEGIN` and `// ─── openkal ─── END`, countable with

```sh
grep -rn "openkal ─── BEGIN" llvm/
```

Each replacement fits one sentence: *upstream asks here which operating system
this is, and the real answer is openkal's `<interface>`.* A change that does not
fit it is not made. The larger half of that document is the list of places that
did **not** need changing — twelve of the nineteen `#include <windows.h>` sites
resolve themselves once the predicate is answered correctly, because libc++'s
POSIX branch was already reaching openkal through musl.
