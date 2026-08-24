// Generated. One of the 130 instantiations of compiler-rt's `aarch64/lse.S`.
//
// Upstream's CMake compiles that one file 130 times with different defines
// (6 patterns x 5 sizes x 5 models, minus the non-cas 16-byte cases). A
// manifest expressing its sources as a glob cannot pass per-file defines, so
// each combination becomes a file that states its own and includes the shared
// body. The body is upstream's and unmodified.
#define L_ldclr
#define SIZE 2
#define MODEL 2
// ⚠️ `assembly.h` FIRST, BY A PATH RELATIVE TO THIS FILE.
//
// `lse.S` opens with `#include "assembly.h"`, which upstream resolves by
// compiling it with `-I` pointing at `builtins/`. This file lives elsewhere, so
// that include would not resolve and the assembler would meet `HIDDEN(...)` as
// an instruction: `error: unrecognized instruction mnemonic`. Including the
// header here, by a path relative to this file, needs no flag at all — and a
// flag is what would have to be repeated by anything else that ever includes
// this directory.
#include "../../llvm/compiler-rt/lib/builtins/assembly.h"
#include "../../llvm/compiler-rt/lib/builtins/aarch64/lse.S"
