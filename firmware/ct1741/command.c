/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "command.h"

#define DSP_MAJOR 4
#define DSP_MINOR 5

static void reply(CT1741Command *state, uint8_t value)
{
    state->response[0] = value;
    state->response_count = 1;
    state->response_index = 0;
}

void ct1741_reset(CT1741Command *state)
{
    state->parameter = 0;
    state->test_register = 0;
    state->response[1] = 0;
    reply(state, 0xaa);
}

uint8_t ct1741_accept(CT1741Command *state, uint8_t value)
{
    if (state->response_index < state->response_count)
        return 0;

    if (state->parameter) {
        if (state->parameter == 0xe0)
            reply(state, (uint8_t)~value);
        else
            state->test_register = value;
        state->parameter = 0;
        return 1;
    }

    switch (value) {
    case 0xe0:
    case 0xe4:
        state->parameter = value;
        break;
    case 0xe1:
        reply(state, DSP_MAJOR);
        state->response[1] = DSP_MINOR;
        state->response_count = 2;
        break;
    case 0xe8:
        reply(state, state->test_register);
        break;
    default:
        /* Unsupported commands have no implementation in this prototype. */
        break;
    }
    return 1;
}

uint8_t ct1741_take(CT1741Command *state, uint8_t *value)
{
    if (state->response_index == state->response_count)
        return 0;

    *value = state->response[state->response_index++];
    return 1;
}
