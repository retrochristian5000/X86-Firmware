// SPDX-License-Identifier: LGPL-3.0-or-later
// Verify firmware metadata alignment and the POST relocation contract.
#include "util.h"
#include "std/bda.h"
#include "std/disk.h"

// These tables live at architectural F-segment offsets, not at addresses
// selected by the compiler's preferred alignment.
_Static_assert(sizeof(struct bios_config_table_s) == 10,
               "BIOS configuration table size must stay fixed");
_Static_assert(sizeof(struct floppy_dbt_s) == 11,
               "Floppy diskette parameter table size must stay fixed");
_Static_assert(__alignof__(BIOS_CONFIG_TABLE) == 1,
               "BIOS configuration table must stay byte-aligned");
_Static_assert(__alignof__(diskette_param_table) == 1,
               "Floppy parameter table must stay byte-aligned");

// Both GCC and Clang must see the non-returning attribute at the call site.
#if defined(__GNUC__) && !defined(__clang__)
_Static_assert(__builtin_has_attribute(reloc_preinit, noreturn),
               "reloc_preinit must be declared noreturn");
#elif defined(__clang__)
int
util_noreturn_clang_probe(void *init, void *arg)
{
    reloc_preinit(init, arg);
}
#endif
