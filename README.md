# openkal-llvm-runtime

LLVM's C++ runtime — libc++, libc++abi and libunwind — configured for
[openkal-musl](https://github.com/mcpplibs/openkal-musl) rather than for a host
C library.

```toml
[dependencies]
openkal-llvm-runtime = "0.1.0"
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

`llvm-generated/generic/__config_site` is libc++'s configure product. Two
positions in it carry the whole of what makes this package different from the
one a toolchain ships:

```c
#define _LIBCPP_HAS_MUSL_LIBC     1   /* the C library beneath is musl's */
#define _LIBCPP_HAS_RANDOM_DEVICE 0   /* openkal has no source of entropy */
```

The first was measured rather than assumed. With it at `0` — the value a
toolchain configured for glibc ships — a translation unit that includes
`<vector>` fails with twenty errors, of which the first names the cause:

```
__locale:439: error: unknown rune table for this platform
              -- do you mean to define _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE?
```

With it at `1`, none. The second follows from openkal reporting `ENOSYS` for
entropy: `std::random_device` is not built, and a program that names it is told
by the linker.

## What a program gets

`examples/cxx` asserts it, and every assertion is written so that it can fail:

- containers and algorithms — `std::vector` sorted by `std::sort`
- strings — built and searched
- values returned through several frames
- **an exception thrown across three frames and caught**
- **a destructor run while the stack is unwound**

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

## Not supported

- **the sanitizers.** compiler-rt's sanitizers depend on a host's internals —
  memory layout, interceptors, symbolisation — and are outside this package.
- **`std::random_device`**, for the reason above.

## Licence

The port is Apache-2.0. The vendored sources under `llvm/` are Apache-2.0 with
LLVM exceptions and are unchanged; `llvm/LICENSE.TXT` is theirs.
