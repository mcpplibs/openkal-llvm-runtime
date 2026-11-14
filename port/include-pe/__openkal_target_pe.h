/* The PE container's mark, for a header this package installs.
 *
 * This directory is on the include line of the PE rows only (mcpp.toml,
 * `cfg(object-format = "pe")`), Windows's and UEFI's alike, so a consumer's
 * compile on one of them finds this file and every other compile does not. An
 * installed header asks `__has_include(<__openkal_target_pe.h>)`: the manifest
 * states the container, and no system is named in the header. */
#ifndef OPENKAL_TARGET_PE
#define OPENKAL_TARGET_PE 1
#endif
