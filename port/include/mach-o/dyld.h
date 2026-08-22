// The dynamic loader's enquiry interface — declared, and answering that there
// is nothing loaded.
//
// ⚠️ THIS ONE IS NOT A COMPILE-TIME STUB. Two places call these at RUN time:
//
//   libcxx/src/include/refstring.h   — asks whether a string literal lies in a
//                                      read-only segment, so that a copy can be
//                                      skipped;
//   libunwind/src/UnwindCursor.hpp   — asks which image a program counter is
//                                      in, for the compact-unwind path.
//
// openkal has no dynamic loader. A program built this way is one image, loaded
// by whatever started it, and there is nothing to enumerate — so the honest
// answer is zero images, and both callers already handle it: refstring falls
// back to copying, and the unwinder falls back to the DWARF tables it is
// configured to use here anyway.
//
// ⚠️ Answering zero is NOT the same as leaving the symbols undefined. Undefined
// would be a link error naming Apple's loader in a program that never wanted
// one; zero is the state of a program that is not dynamically loaded, and it is
// true rather than a simulation.
#ifndef OPENKAL_MACH_O_DYLD_H
#define OPENKAL_MACH_O_DYLD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mach_header;

static inline uint32_t _dyld_image_count(void) { return 0; }
static inline const struct mach_header* _dyld_get_image_header(uint32_t) { return 0; }
static inline intptr_t _dyld_get_image_vmaddr_slide(uint32_t) { return 0; }
static inline const char* _dyld_get_image_name(uint32_t) { return 0; }

#ifdef __cplusplus
}
#endif

#endif
