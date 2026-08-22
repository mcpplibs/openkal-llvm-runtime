// A stub for a header openkal has no reason to have, providing exactly the one
// macro that is read.
//
// ⭐ THE SAME MOVE AS openkal-macos's `libSystem.tbd`, WHICH IS TWO NAMES.
//
// libunwind's public header asks how old a deployment target may be before its
// entry points become unavailable, and it asks Apple's SDK. There is no Apple
// SDK here: the platform is `openkal-macos`, an implementation of a
// 48-function interface, and the object format is Mach-O because that is what
// the machine loads — those are two different statements, and upstream reads
// the second as the first.
//
// The whole of what is read is below. It was obtained by grepping the vendored
// runtimes for every `AVAILABLE_*` / `__OSX_AVAILABLE*` / `__IPHONE_*` name, not
// by copying Apple's header, so a name that appears here appears because
// something asks for it.
//
//     libunwind.h:  AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER
//
// ⚠️ EMPTY IS THE ANSWER, NOT A PLACEHOLDER. The macro's job upstream is to
// attach an availability attribute, which describes when a symbol appeared in
// Apple's shipping libSystem. Nothing here comes from Apple's libSystem, so
// there is no version at which it appeared and nothing to attach.
#ifndef OPENKAL_AVAILABILITY_MACROS_H
#define OPENKAL_AVAILABILITY_MACROS_H

#define AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER /* always available here */

#endif
