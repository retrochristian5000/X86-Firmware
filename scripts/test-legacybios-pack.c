// SPDX-License-Identifier: LGPL-3.0-or-later
// Check that the EFI wire format is packed without changing its includer.
#include <stddef.h>
#include <stdint.h>

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;

// Simulate a caller which already has a non-default packing policy.
#pragma pack(push, 2)
#include "std/LegacyBios.h"

_Static_assert(_Alignof(EFI_COMPATIBILITY16_TABLE) == 1,
               "CSM compatibility table must remain byte-packed");

struct caller_packing {
    UINT8 tag;
    UINT32 value;
};
_Static_assert(offsetof(struct caller_packing, value) == 2,
               "LegacyBios.h leaked its packing into the caller");

#pragma pack(pop)

struct default_packing {
    UINT8 tag;
    UINT32 value;
};
_Static_assert(offsetof(struct default_packing, value) == 4,
               "The original ABI packing was not restored");
