/* SPDX-License-Identifier: LGPL-3.0-or-later */
#ifndef CT1741_COMMAND_H
#define CT1741_COMMAND_H

#include <stdint.h>

typedef struct {
    uint8_t parameter;
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

#endif
