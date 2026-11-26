// Legacy programmable attribute map policy.
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __SHADOW_UMB_H
#define __SHADOW_UMB_H

#include "types.h"

#define SHADOW_UMB_BLOCK 0x4000u
#define SHADOW_READONLY  1
#define SHADOW_READWRITE 3

// Each PAM nibble describes one 16 KiB segment. Preserve writable
// runtime data even if a segment also contains immutable firmware;
// the allocator's dummy option-ROM signature marks that shared area.
static inline u8
shadow_umb_attr(u32 start, u32 rom_end, u32 runtime_start,
                u32 readonly_start, int upper_alloc, int writable_unused)
{
    u32 end = start + SHADOW_UMB_BLOCK;

    if (upper_alloc && runtime_start < readonly_start &&
        start < readonly_start && end > runtime_start)
        return SHADOW_READWRITE;

    // Only wholly unused blocks may become writable by policy.
    if (writable_unused && start >= rom_end && end <= runtime_start &&
        end <= readonly_start)
        return SHADOW_READWRITE;

    return SHADOW_READONLY;
}

static inline u8
shadow_umb_pam(u32 start, u32 rom_end, u32 runtime_start,
               u32 readonly_start, int upper_alloc, int writable_unused)
{
    return shadow_umb_attr(start, rom_end, runtime_start, readonly_start,
                           upper_alloc, writable_unused)
        | (shadow_umb_attr(start + SHADOW_UMB_BLOCK, rom_end, runtime_start,
                           readonly_start, upper_alloc, writable_unused) << 4);
}

#endif
