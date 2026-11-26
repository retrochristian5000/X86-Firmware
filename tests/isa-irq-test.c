// Host-side regression for the production PIIX IRQ policy.
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <assert.h>
#include "../src/fw/isa-irq.h"

int main(void)
{
    assert(piix_reserved_irqs(ISA_IRQ_MASK_MISSING, 1U << 10) == (1U << 10));
    assert(piix_reserved_irqs(1U << 5, 1U << 10) == ((1U << 5) | (1U << 10)));
    assert(piix_pirq_options(ISA_IRQ_MASK_MISSING, 0) == PIIX_PIRQ_LEGACY_BITMAP);
    assert(piix_pirq_options(0, 0) == PIIX_PIRQ_MANAGED_BITMAP);
    for (unsigned int mask = 0; mask <= 0xffffU; mask++) {
        unsigned int options = piix_pirq_options(mask, 0);
        assert(options == (PIIX_PIRQ_MANAGED_BITMAP & ~mask));
        for (unsigned int preferred = 10; preferred <= 11; preferred++) {
            unsigned int route = piix_pirq_select(preferred, mask);
            if (route == 0x80)
                assert((mask & 0xc00U) == 0xc00U);
            else {
                assert(route == 10 || route == 11);
                assert(options & (1U << route));
                if (!(mask & (1U << preferred)))
                    assert(route == preferred);
            }
        }
    }
    return 0;
}
