// SPDX-License-Identifier: LGPL-3.0-or-later
// Enforce the hardware's CRB response-buffer capacity.
#ifndef __HW_TPM_CRB_SIZE_H
#define __HW_TPM_CRB_SIZE_H

#include "std/tcg.h" // struct tpm_rsp_header

static inline int
tpm_crb_response_size_valid(u32 capacity, u32 declared)
{
    return declared >= sizeof(struct tpm_rsp_header)
        && declared <= capacity;
}

#endif // __HW_TPM_CRB_SIZE_H
