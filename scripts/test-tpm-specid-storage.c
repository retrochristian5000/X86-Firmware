// SPDX-License-Identifier: LGPL-3.0-or-later
// The TPM 2.0 Spec ID event needs trailing digest space, but nesting a
// flexible-array structure before a struct pad is a GNU-only extension.
#include "std/tcg.h"

union specid_event_storage {
    struct TCG_EfiSpecIdEventStruct hdr;
    u8 storage[sizeof(struct TCG_EfiSpecIdEventStruct) + 128];
};

_Static_assert(sizeof(union specid_event_storage)
               == sizeof(struct TCG_EfiSpecIdEventStruct) + 128,
               "Spec ID event backing storage changed");
_Static_assert(__alignof__(union specid_event_storage) == 1,
               "Spec ID event must remain byte-aligned");

int
main(void)
{
    union specid_event_storage event = {
        .hdr.signature = "Spec ID Event03",
        .hdr.specVersionMajor = 2,
    };
    // Exercise the flexible-array view through the backing union.
    event.hdr.digestSizes[0].algorithmId = 0x000b;
    event.hdr.digestSizes[0].digestSize = 32;
    event.hdr.numberOfAlgorithms = 1;

    u32 offset = __builtin_offsetof(struct TCG_EfiSpecIdEventStruct,
                                    digestSizes);
    return event.hdr.numberOfAlgorithms != 1
        || event.storage[offset] != 0x0b
        || event.storage[offset + 2] != 32;
}
