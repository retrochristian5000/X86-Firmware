// SPDX-License-Identifier: LGPL-3.0-or-later
// Guard the TPM CRB response-boundary checks against regressions.
#include "hw/tpm-crb-size.h"

_Static_assert(sizeof(struct tpm_rsp_header) == 10,
               "TPM response header size changed");

int
main(void)
{
    static const struct {
        u32 capacity;
        u32 declared;
        int valid;
    } cases[] = {
        { 0, 10, 0 },
        { 9, 10, 0 },
        { 10, 9, 0 },
        { 10, 10, 1 },
        { 10, 11, 0 },
        { 64, 0, 0 },
        { 64, 6, 0 },
        { 64, 9, 0 },
        { 64, 10, 1 },
        { 64, 64, 1 },
        { 64, 65, 0 },
        { 4096, 4096, 1 },
        { 4096, 4097, 0 },
        { 0xffffffffU, 0xffffffffU, 1 },
    };

    for (u32 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int valid = tpm_crb_response_size_valid(
            cases[i].capacity, cases[i].declared);
        if (valid != cases[i].valid)
            return i + 1;
    }
    return 0;
}
