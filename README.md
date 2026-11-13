# openkal-llvm-runtime

LLVM's C++ runtime — libc++, libc++abi and libunwind — configured for
[openkal-musl](https://github.com/mcpplibs/openkal-musl) rather than for a host
C library.

```toml
[dependencies]
openkal-llvm-runtime = "0.20.2"
```

> **Engine floor (mcpp 2026.11.2.1):** 0.17.0 is built over openkal-musl 0.26.0,
> which embeds its allocator through a dependency edge's `embed` and states that
> floor itself. A Windows program needs mcpp 2026.11.2.2, from which an embedded
> dependency's link libraries are its embedder's; under 2026.11.2.1 its link
> names the Win32 libraries of mimalloc's own operating-system layer.

> **Engine floor (mcpp 2026.9.20.1):** this version of this package pins
> `openkal-musl 0.19.0` and inherits its `[c-abi]` declaration. The engine
> assembles the verification probe's command line in one place that REFUSES
> to run it without a target selection (`cenv_probe::assemble_argv`), and
> `cenv::realise` forces `-fno-short-wchar` on freestanding wchar. Older
> engines silently misbuild this package on freestanding cross-builds
> (`riscv64-none-elf` and the rest): their probe ran with no `--target=` at
> all, so the compiler answered for the machine it was running on.
>
> **The floor named here was 2026.9.18.3 and that was wrong.** It credited
> that release with stripping host macros (`-U_WIN32` and the MinGW pair) on
> Windows hosts before reading the predefined macros. The strip was real and
> it protected nothing: there was no target substitution in the freestanding
> argv for it to compensate for, and deleting the host's predefines deleted
> the one piece of evidence that would have said so. A measurement that
> removes its own disagreement reports an agreement it never established.
>
> Upgrade: `xlings install mcpp --force`.

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
#define _GNU_SOURCE               1   /* musl's GNU interfaces, on every target */
```

The last line is not a libc++ switch, and it is here for the same reason as the
others. This configuration's locale support calls `strtof_l`, `strtod_l`,
`strtold_l` and `vasprintf`, which musl declares only under `_GNU_SOURCE`.
Clang predefines `_GNU_SOURCE` for C++ on Linux targets and on no other, so a
translation unit that includes a standard header — which is what every module
wrapper of a header library does — built for Linux and failed for macOS and
Windows with `no member named 'strtof_l' in the global namespace`. Stating it
in the configuration gives every target what the Linux target already had.

The first was measured rather than assumed. With it at `0` — the value a
toolchain configured for glibc ships — a translation unit that includes
`<vector>` fails with twenty errors, of which the first names the cause:

```
__locale:439: error: unknown rune table for this platform
              -- do you mean to define _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE?
```

With it at `1`, none.

**A claim that drifts from what the port provides fails neither the build nor
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

`import std;` on such a target is still refused, and not by this package:

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

## The observation a build cannot make

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

**And the artefacts are run on the real machines, not inspected.** Continuous
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

They are **almost** unchanged, and the exceptions are enumerated rather than
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

## Inside an ok image (0.16.0)

For the target `<arch>-openkal` (openkal SPEC clause 12) the runtime is built
into an **ok image** that `okrun` runs on Linux, macOS and Windows of its
architecture. It takes the Linux selection --- the same C library above
openkal, the same object format and calling convention --- and depends on
`openkal-musl` 0.22.0 without its default `image-builtins` feature, since this
package supplies the compiler runtime itself and a build has one.
`examples/image-cxx` (an exception, a thread with its own thread-local
variable, the containers, formatting) is built once on Linux for both
architectures and run through okrun on four systems in CI.

## Over openkal-musl 0.27.2 (0.18.3)

openkal-musl 0.27.2 selects openkal-ld 0.4.2 for an openkal image. The published
dependency graph therefore uses the same loader version as the platform.
The compiler runtime sources are unchanged.

## Over openkal-musl 0.27.1 (0.18.2)

openkal-musl 0.27.1, over openkal-macos 0.21.3: a hosted program on macOS is given the directories
it was granted. Nothing in this package changes.

## Built with branch protection (0.18.1)

openkal-platform 0.23.0 compiles every unit on aarch64 with `-mbranch-protection=pac-ret+bti`.
compiler-rt's outline atomics (`llvm-generated/outline-atomics`, one file for each instantiation
of `aarch64/lse.S`) include compiler-rt's `assembly.h` by their own path, and `lse.S` then asks
for `"assembly.h"`, which this package's include path answers with libunwind's header of the same
name: it redefined `GNU_PROPERTY` as a number, and the property note compiler-rt writes after the
functions failed to assemble (`3(0xc0000000, 1 | 2 | 0)`: "unexpected token at start of
statement"). Without branch protection the note is empty and the redefinition went unseen. Each
file now states libunwind's guard before it includes `lse.S`, and the objects carry the note an
image's loader reads before it enforces the marks (`aarch64 feature: BTI, PAC`).

## Over openkal-musl 0.27.0 (0.18.0)

openkal-musl 0.27.0, the C library openkal-platform 0.23.0 names: `/dev/null` on every system,
and the system's null device at a started program's position (openkal 0.22); `realpath` answered
by the environment for the whole name, and an image's `dlopen` at its base (openkal 0.23); its
image loader is openkal-image 0.4.0's; a program that gives on the directories it holds, with their
rights; and a snapshot of an image's initialisers, which runs the C++ run-time's own --- the
standard streams, the locale --- once in a boot (openkal-platform 0.23.0's CI starts from one a
program whose initialisers build strings, maps and vectors and whose `main` then throws, starts
threads and destroys their thread-local objects). On PE under clang the C library requires the compiler runtime
this package builds (`mcpp:compiler-runtime=compiler-rt`), which a graph holding this package
supplies. Nothing else changes.

## Over openkal-musl 0.26.1 (0.17.1)

openkal-musl 0.26.1, whose image loader is openkal-image 0.3.1's, the version group openkal-platform
0.21.1 names; nothing else changes.

## Over openkal-musl 0.26.0, and measured (0.17.0)

The C library beneath is openkal-musl 0.26.0: its allocator is mimalloc,
embedded, its anonymous memory is `openkal.region`, its string functions use the
processor's vectors, and a positional read takes no lock. This package depends
on it without the default features (the compiler runtime is this package's) and
names the allocator itself (`malloc-mimalloc`), since 0.26.0 selects exactly one.

**A `thread_local`'s destructor reads its variable on PE and Mach-O.** There a
thread's variables are compiler-rt's emutls blocks, freed by a key created before
the key that runs `thread_local` destructors, and musl runs key destructors in
key order: the destructor read freed memory, which mimalloc overwrites. emutls
now frees the blocks in musl's last round (`llvm/PATCHES.md`, 第七处 第四层).

**What a front end does with the runtime, timed against the system's own**
(`bench/`, the design's L2). Five workloads in the shapes clangd's warm start
spends its time in, written with the standard library alone: an option table
searched by prefix for every argument of 481 command lines (as OptTable parses a
compile command), a map from path strings filled, searched and emptied (as
StringMap), an open-addressing map from pointers (as DenseMap), variable-width
fields written and read back (as a module's bitstream), and posting lists of
three-letter keys built and intersected (as a symbol index). The same source is
built with the system's C++ library, above this runtime, and as an ok image;
`tools/bench-compare.sh` divides, and continuous integration gates at 1.15 times
the system's time. The system's library is built by Clang, as the runtime is,
so that what differs is the runtime (0.17.1): on Linux arm64 the index workload
over libstdc++ took 220 ns per symbol built by GCC 13 and 261 built by Clang,
and the image 237; the build by the system's own compiler is reported beside
the gate. Measured on Linux x86_64 while it was written, the image against the
system's libstdc++ and glibc:

| workload | system | image | ratio |
|---|---|---|---|
| options (per command line) | 3991 ns | 3612 ns | 0.91 |
| stringmap (per element) | 107 ns | 73 ns | 0.68 |
| densemap (per element) | 31 ns | 29 ns | 0.96 |
| bitstream (per field) | 12.4 ns | 12.4 ns | 1.00 |
| index (per symbol) | 157 ns | 139 ns | 0.89 |


## Over openkal-musl 0.27.3 (0.18.4)

openkal-musl 0.27.3 selects openkal-linux 0.24.1 on Linux and openkal-ld 0.4.3
for an image. Linux confinement requires Landlock ABI 3 to enforce read-only
grants against raw file truncation. The runtime implementation is unchanged.

## i686 (0.20.2)

Built for `i686-linux-musl` above openkal-musl and openkal-linux on i386
(openkal 0.25.1, clause 5.5), the selection of compiler-rt differs from the
sixty-four bit one in two files. The x87 routines (`*xf*.c`, `*xc3.c`, the 80-bit
`long double`) are kept, as on x86_64: the type is real on i386 too. And
`atomic.c` is compiled: a sixty-four bit atomic on an object the compiler cannot
prove eight-byte aligned --- an `int64_t` field of a structure, which i386 aligns
to four --- is the call `__atomic_load` rather than an instruction, and the
embedded allocator's statistics make such loads (measured: `undefined symbol:
__atomic_load`). It takes its own locks and calls nothing of the system. Measured
locally above the family's i386 branches: a C++23 program with `import std`, an
exception, four `std::jthread`s, `std::format` of sixty-four bit values and a
file stream ran, and the openkal conformance suite held every observation it
made (407, none did not, the five not observed being x86_64's five).

## OpenKal 0.25 dependency group

Runtime 0.20.1 builds for FreeBSD above openkal-musl (`x86_64-freebsd-musl`,
`aarch64-freebsd-musl`): the selection of compiler-rt is Linux's and the ok
image's, and `clear_cache.c` is kept, since nothing beneath a program of those
rows supplies `__clear_cache`. The rows need openkal-musl 0.29.1, whose
FreeBSD port offers `elf_aux_info`, which compiler-rt's aarch64 `cpu_model`
calls there. The OpenKal conformance suite built so holds every observation in
a FreeBSD virtual machine.

Runtime 0.20.0 is built over openkal-musl `^0.29.0` and the OpenKal 0.25
implementations beneath it (Linux 0.26, macOS 0.23, Windows 0.21, OpenSBI 0.9).
The C++ runtime surface and the allocator selection are unchanged. Through the C
library, `std::filesystem::create_hard_link` gives a file a second name
(`link`, over `kal_fs_hard_link`), and a program named without a separator is
looked for by the system's conventions: on Windows PATH is split on `;` and the
`PATHEXT` suffixes are tried.

## OpenKal 0.24 dependency group

Runtime 0.19.2 keeps the volume where a name is rebuilt part by part. `X:` is a
root name only with the separator after it, so `lexically_normal` appended `D:` as
an ordinary name and the root directory replaced it (`D:/a/./b` became `/a/b`), and
so did `weakly_canonical`, which ends in it, and `path("D:/a") / "/b"`. On Windows a
program over the hosted arrangement then named every file beneath no volume: a
launcher could not start its runner (mcpp-community/mcpp#889). `lexically_normal`
now begins its result with the volume and its root directory as written, and
appending a rooted name beneath a volume keeps the volume (`D:/b`), as
[fs.path.append] states it for a system with root names. Both probes state the
three results; without the change two fail on Linux and under the Windows hosted
arrangement.

Runtime 0.19.1 reads a name beneath a volume, `X:/...`, as the C library beneath
does: `X:` is the root name and `/` the root directory, so the working directory on
a system with volumes is absolute and `absolute`, `canonical` and `relative` keep
it (openkal-llvm-runtime#64). `X:` without a separator stays an ordinary name.
Both probes (`examples/cxx`, `examples/image-cxx`) state the decomposition, the
append and the working-directory results; without the change four of them fail on
Linux and the working-directory results fail on Windows. The dependency on
openkal-musl is the compatible range `^0.28.0`.

Runtime 0.19.0 selects openkal-musl 0.28.0 and its matching OpenKal 0.24
implementations. The C++ runtime surface and allocator feature selection
remain unchanged.
