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
    assert(ct1741_accept(&state, 0x41));
    assert(ct1741_accept(&state, 0xac));
    assert(!ct1741_take_action(&state));
    assert(ct1741_accept(&state, 0x44));
    assert(!ct1741_accept(&state, 0xb6));
    assert(ct1741_take_action(&state) == CT1741_ACTION_RATE);
    assert(state.args[0] == 0xac && state.args[1] == 0x44);

    assert(ct1741_accept(&state, 0xb6));
    assert(ct1741_accept(&state, 0x30));
    assert(ct1741_accept(&state, 0x34));
    assert(!ct1741_take_action(&state));
    assert(ct1741_accept(&state, 0x12));
    assert(ct1741_take_action(&state) == CT1741_ACTION_START);
    assert(state.args[0] == 0xb6 && state.args[1] == 0x30);
    assert(state.args[2] == 0x34 && state.args[3] == 0x12);

    assert(ct1741_accept(&state, 0xc8)); /* ADC frame is consumed. */
    assert(ct1741_accept(&state, 0xe1));
    assert(ct1741_accept(&state, 0xe1));
    assert(ct1741_accept(&state, 0xe1));
    assert(!ct1741_take_action(&state));
    assert(ct1741_accept(&state, 0xd5));
    assert(ct1741_take_action(&state) == CT1741_ACTION_DMA_CONTROL);
    assert(state.args[0] == 0xd5);
    assert(ct1741_accept(&state, 0xd1));
    assert(ct1741_take_action(&state) == CT1741_ACTION_SPEAKER);
    assert(ct1741_accept(&state, 0xd8));
    expect(&state, 0xff);

    assert(ct1741_accept(&state, 0xc0));
    assert(ct1741_accept(&state, 0));
    ct1741_reset(&state);
    expect(&state, 0xaa);
    assert(!ct1741_take_action(&state));
    /* Independent known vectors: the mask repeats every four requests. */
    {
        const uint8_t inputs[] = { 0x00, 0x55, 0xff, 0x5a, 0x00 };
        const uint8_t outputs[] = { 0x40, 0x30, 0xc6, 0xc6, 0x5c };
        unsigned int i;

        for (i = 0; i < sizeof(inputs); i++) {
            assert(ct1741_accept(&state, 0xe2));
            assert(!ct1741_take_action(&state));
            assert(ct1741_accept(&state, inputs[i]));
            assert(ct1741_take_action(&state) == CT1741_ACTION_DMA_ID);
            assert(state.args[0] == outputs[i]);
            assert(!ct1741_take(&state, &value));
        }
    }
    ct1741_reset(&state);
    expect(&state, 0xaa);
    assert(ct1741_accept(&state, 0xe2));
    assert(ct1741_accept(&state, 0x00));
    assert(ct1741_take_action(&state) == CT1741_ACTION_DMA_ID);
    assert(state.args[0] == 0x40);
    return 0;
}
