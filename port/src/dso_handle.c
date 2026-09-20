/* `__dso_handle`, which a dynamic loader supplies and this arrangement has
 * none of --- the same fact `AddressSpace.hpp`'s `__ImageBase` read and
 * `RWMutex.hpp`'s pthread route are already about, stated once more for the
 * one symbol neither of them reaches.
 *
 * The C++ ABI passes this to `__cxa_atexit` as the third argument, so that an
 * unloaded shared object's destructors can be told apart from every other
 * one's. A statically linked openkal program is one module and never unloads
 * anything short of exiting, and openkal-musl's own `__cxa_atexit`
 * (musl/src/exit/atexit.c) does not read the argument at all --- so any
 * stable address is a correct answer, and this is the same one
 * `compiler-rt/lib/builtins/crtbegin.c` gives on every format that reaches
 * it, taken alone rather than with the rest of that file: the rest drives
 * `.init_array`, which `okm_start.c` already does, and driving it twice is
 * not a slower program but a different one (see `AddressSpace.hpp` for the
 * fuller account of that).
 *
 * WINDOWS ONLY, AND FOR A REASON THAT IS mcpp'S RATHER THAN THIS PACKAGE'S.
 * On ELF and on Mach-O, clang gives every translation unit that needs one a
 * PRIVATE `__dso_handle` of its own --- measured with `nm`, a local symbol,
 * never an external reference, so nothing here is missing on those formats.
 * On this target only, it is an external reference. Measured: compiling the
 * same translation unit once with `--target=x86_64-w64-windows-gnu` alone
 * and once with the second `--target=x86_64-pc-cygwin` mcpp's `[c-abi]`
 * realization adds (see mcpp.toml, `[target.'cfg(windows)'.build]`), only
 * the second leaves `__dso_handle` undefined. The substitution that states
 * `__CYGWIN__` for the preprocessor also changes what clang assumes about
 * the runtime beneath --- a real Cygwin has a real loader, and this build
 * does not --- and supplying the symbol is the correct response to that,
 * not a workaround for it: the C++ ABI always required something to supply
 * this, and on every other format something already did. */
#if defined(OPENKAL_TARGET_WINDOWS)
__attribute__((visibility("hidden"))) void *__dso_handle = &__dso_handle;
#endif
