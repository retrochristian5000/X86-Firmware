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
    state->arg_count = 0;
    state->arg_index = 0;
    state->args[0] = 0;
    state->args[1] = 0;
    state->args[2] = 0;
    state->args[3] = 0;
    state->action = 0;
    state->speaker = 0;
    state->dma_id_value = 0xaa;
    state->dma_id_mask = 0x96;
    state->test_register = 0;
    state->response[1] = 0;
    reply(state, 0xaa);
}

static void complete(CT1741Command *state)
{
    uint8_t cmd = state->parameter;
    uint8_t a = state->args[0];
    uint8_t b = state->args[1];
    uint8_t c = state->args[2];

    switch (cmd) {
    case 0xe0:
        reply(state, (uint8_t)~a);
        break;
    case 0xe2:
        /* Undocumented DMA identification: cumulative modulo-256 sum
         * with a rotating XOR mask. See README compatibility reference. */
        state->dma_id_value += (uint8_t)(a ^ state->dma_id_mask);
        state->dma_id_mask = (uint8_t)((state->dma_id_mask >> 2) |
                                      (state->dma_id_mask << 6));
        state->args[0] = state->dma_id_value;
        state->action = CT1741_ACTION_DMA_ID;
        break;
    case 0xe4:
        state->test_register = a;
        break;
    case 0x40:
        state->action = CT1741_ACTION_TIME_CONST;
        break;
    case 0x41:
        state->action = CT1741_ACTION_RATE;
        break;
    case 0x48:
        state->action = CT1741_ACTION_BLOCK;
        break;
    case 0x14:
        state->args[2] = b;
        state->args[1] = a;
        state->args[0] = cmd;
        state->action = CT1741_ACTION_LEGACY_START;
        break;
    default:
        if ((cmd & 0xf0) == 0xb0 || (cmd & 0xf0) == 0xc0) {
            /* Playback only; reject ADC, reserved bit and mode bits. */
            if (!(cmd & 9) && !(a & 0xcf)) {
                state->args[0] = cmd;
                state->args[1] = a;
                state->args[2] = b;
                state->args[3] = c;
                state->action = CT1741_ACTION_START;
            }
        }
        break;
    }
    state->parameter = 0;
    state->arg_index = 0;
}

uint8_t ct1741_accept(CT1741Command *state, uint8_t value)
{
    if (state->response_index < state->response_count || state->action)
        return 0;

    if (state->parameter) {
        state->args[state->arg_index++] = value;
        if (state->arg_index == state->arg_count)
            complete(state);
        return 1;
    }

    state->args[0] = 0;
    state->args[1] = 0;
    state->args[2] = 0;
    state->args[3] = 0;
    state->arg_count = 0;
    switch (value) {
    case 0xe0: case 0xe4: case 0xe2: case 0x40: case 0x10:
    case 0x04: case 0x0f: case 0xf9:
        state->arg_count = 1;
        break;
    case 0x14: case 0x41: case 0x42: case 0x48: case 0x05: case 0x0e:
    case 0x74: case 0x75: case 0x76: case 0x77: case 0x80:
    case 0x24:
        state->arg_count = 2;
        break;
    case 0xe1:
        reply(state, DSP_MAJOR);
        state->response[1] = DSP_MINOR;
        state->response_count = 2;
        break;
    case 0xe8:
        reply(state, state->test_register);
        break;
    case 0x1c: case 0x90: case 0x91:
        state->args[0] = value;
        state->action = CT1741_ACTION_LEGACY_START;
        break;
    case 0xd0: case 0xd4: case 0xd5: case 0xd6: case 0xd9: case 0xda:
        state->args[0] = value;
        state->action = CT1741_ACTION_DMA_CONTROL;
        break;
    case 0xd1: case 0xd3:
        state->speaker = value == 0xd1;
        state->args[0] = state->speaker;
        state->action = CT1741_ACTION_SPEAKER;
        break;
    case 0xd8:
        reply(state, state->speaker ? 0xff : 0);
        break;
    case 0xf2: case 0xf3:
        state->args[0] = value == 0xf3;
        state->action = CT1741_ACTION_IRQ;
        break;
    default:
        if ((value & 0xf0) == 0xb0 || (value & 0xf0) == 0xc0)
            state->arg_count = 3;
        break;
    }
    if (state->arg_count)
        state->parameter = value;
    return 1;
}

uint8_t ct1741_take_action(CT1741Command *state)
{
    uint8_t action = state->action;
    state->action = 0;
    return action;
}

uint8_t ct1741_take(CT1741Command *state, uint8_t *value)
{
    if (state->response_index == state->response_count)
        return 0;

    *value = state->response[state->response_index++];
    return 1;
}
