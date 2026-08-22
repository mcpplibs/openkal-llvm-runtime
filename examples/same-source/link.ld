/* Where the image goes, and what a C++ program needs the layout to contain.
 *
 * OpenSBI occupies 0x80000000 upward and hands control to the next stage at
 * 0x80200000, which is why this address and not the start of RAM.
 *
 * ⚠️ FIRMWARE JUMPS TO THE LOWEST LOADED ADDRESS AND NOT TO THE ENTRY THE IMAGE
 * RECORDS. Measured 2026-08-23, after an attempt to put the ELF header inside
 * the first loaded segment so that `__ehdr_start` would be usable: the entry
 * moved to 0x80200270, OpenSBI still announced `Next Address 0x80200000`, and
 * the machine hung executing the header as instructions. So the first thing at
 * the load address has to be the first instruction, and everything that would
 * otherwise be read out of the program headers is named here instead.
 *
 * ⚠️ FOUR THINGS BEYOND THE MINIMUM, AND EACH IS SOMETHING A C++ PROGRAM HAS
 * THAT A C ONE DOES NOT.
 *
 *   The initialiser arrays. Every static object with a constructor puts a
 *   pointer in .init_array, and the C library walks it between symbols the
 *   LINKER normally provides. A script that does not name them gets an empty
 *   range and every such object stays unconstructed --- which does not fail,
 *   it MISBEHAVES, and that is worse.
 *
 *   The unwind tables, and the two symbols that bound them. libunwind built
 *   with _LIBUNWIND_IS_BAREMETAL looks them up by name instead of walking
 *   program headers; the fragment below is the one its AddressSpace.hpp
 *   documents, reproduced rather than invented.
 *
 *   The thread-local segment, and three symbols describing it. The startup
 *   object in openkal-opensbi builds one context's storage from them. Without
 *   it the register that names thread-local storage holds whatever reset left,
 *   and the first `throw` faults inside `__cxa_get_globals` --- a message that
 *   names an exception function and says nothing about a thread pointer.
 *
 *   A stack big enough for the standard library. 16 KiB is enough for a program
 *   that prints a string; formatting through <format> and sorting a vector is
 *   not that program.
 */
ENTRY(_start)
SECTIONS {
    . = 0x80200000;

    .text : { *(.text.entry) *(.text .text.*) }

    .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) }

    /* Verbatim from libunwind's AddressSpace.hpp, which is where the names come
     * from. `--eh-frame-hdr` on the link line is what makes the index exist at
     * all; without it the section is empty and the two `_hdr_` symbols are the
     * zero the conditional below produces, which libunwind reads as "no index"
     * and falls back to scanning. */
    .eh_frame : {
        __eh_frame_start = .;
        KEEP(*(.eh_frame))
        __eh_frame_end = .;
    }
    .eh_frame_hdr : { KEEP(*(.eh_frame_hdr)) }
    __eh_frame_hdr_start = SIZEOF(.eh_frame_hdr) > 0 ? ADDR(.eh_frame_hdr) : 0;
    __eh_frame_hdr_end   = SIZEOF(.eh_frame_hdr) > 0 ? . : 0;

    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    }
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }
    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }

    /* The thread-local segment, and its three measurements.
     *
     * They are named here rather than read from PT_TLS for the reason at the
     * top of this file: the program headers are not reachable at run time on
     * this target. The startup object takes the initialised bytes from
     * `__tls_start`, copies `__tls_filesz` of them, and zeroes out to
     * `__tls_memsz` — which is what a loader does. */
    .tdata : { __tls_start = .; *(.tdata .tdata.*) }
    .tbss  : { *(.tbss .tbss.*) *(.tcommon) }
    __tls_filesz = SIZEOF(.tdata);
    __tls_memsz  = SIZEOF(.tdata) + SIZEOF(.tbss);

    .data : { *(.data .data.*) *(.sdata .sdata.*) }

    .bss : {
        __bss_start = .;
        *(.sbss .sbss.*) *(.bss .bss.*) *(COMMON)
        __bss_end = .;
    }

    /* 256 KiB of stack. See the note above. */
    . = ALIGN(16); . = . + 0x40000; __stack_top = .;

    /* ⭐ AND THE HEAP, WHICH FOR THIS PROGRAM CANNOT BE THE IMPLEMENTATION'S
     * DEFAULT.
     *
     * openkal-opensbi carries a 64 KiB static region for a program that
     * allocates a little. A program carrying a C library and a C++ standard
     * library allocates during its own initialisation, before `main`, and does
     * not survive that figure — measured, and what it looks like is a program
     * that starts and prints nothing, because the allocator runs out before
     * there is a stream to report it on.
     *
     * So the region is stated here, where the stack's size already is, and it
     * costs nothing in the image: it is past the end and QEMU's `virt` has
     * 128 MiB. 16 MiB is comfortably more than this program needs and small
     * enough to fit a machine with far less. */
    . = ALIGN(16); __heap_start = .; . = . + 0x1000000; __heap_end = .;
}
