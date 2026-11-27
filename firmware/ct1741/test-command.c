/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include <assert.h>
#include "command.h"

static void expect(CT1741Command *state, uint8_t expected)
{
    uint8_t value = 0;
    assert(ct1741_take(state, &value));
    assert(value == expected);
}

int main(void)
{
    CT1741Command state;
    uint8_t value = 0;

    ct1741_reset(&state);
    assert(!ct1741_accept(&state, 0xe1));
    expect(&state, 0xaa);
    assert(!ct1741_take(&state, &value));

    assert(ct1741_accept(&state, 0xe1));
    expect(&state, 4);
    assert(!ct1741_accept(&state, 0xe4));
    expect(&state, 5);

    for (unsigned int i = 0; i < 256; i++) {
        assert(ct1741_accept(&state, 0xe0));
        assert(ct1741_accept(&state, (uint8_t)i));
        expect(&state, (uint8_t)(i ^ 0xff));
    }

    assert(ct1741_accept(&state, 0xe4));
    assert(ct1741_accept(&state, 0xe1)); /* Parameter, not command. */
    assert(ct1741_accept(&state, 0xe8));
    expect(&state, 0xe1);

    assert(ct1741_accept(&state, 0xe0));
    ct1741_reset(&state);
    expect(&state, 0xaa);
    assert(ct1741_accept(&state, 0xe8));
    expect(&state, 0);
    assert(!ct1741_take(&state, &value));

    assert(ct1741_accept(&state, 0xe1));
    expect(&state, 4);
    ct1741_reset(&state);
    expect(&state, 0xaa);
    assert(!ct1741_take(&state, &value));
    return 0;
}
