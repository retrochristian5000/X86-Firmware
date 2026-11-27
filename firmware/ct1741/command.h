/* SPDX-License-Identifier: LGPL-3.0-or-later */
#ifndef CT1741_COMMAND_H
#define CT1741_COMMAND_H

#include <stdint.h>

typedef struct {
    uint8_t parameter;
    uint8_t arg_count;
    uint8_t arg_index;
    uint8_t args[4];
    uint8_t action;
    uint8_t speaker;
    uint8_t dma_id_value;
    uint8_t dma_id_mask;
    uint8_t test_register;
    uint8_t response[2];
    uint8_t response_count;
    uint8_t response_index;
} CT1741Command;

void ct1741_reset(CT1741Command *state);
/* Accept one byte only when the preceding response has drained. */
uint8_t ct1741_accept(CT1741Command *state, uint8_t value);
/* Return one response byte if available. */
uint8_t ct1741_take(CT1741Command *state, uint8_t *value);

/* Internal experimental firmware-to-hardware ABI v1. The firmware
 * publishes a completed action once; the transport writes its arguments
 * before the commit byte. These are not guest ISA registers. */
#define CT1741_ACTION_RATE 1
#define CT1741_ACTION_TIME_CONST 2
#define CT1741_ACTION_BLOCK 3
#define CT1741_ACTION_START 4
#define CT1741_ACTION_LEGACY_START 5
#define CT1741_ACTION_DMA_CONTROL 6
#define CT1741_ACTION_SPEAKER 7
#define CT1741_ACTION_IRQ 8
#define CT1741_ACTION_DMA_ID 9
#define CT1741_ACTION_DIRECT_DAC 10

uint8_t ct1741_take_action(CT1741Command *state);

#endif
