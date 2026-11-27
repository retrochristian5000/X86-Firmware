// SPDX-License-Identifier: LGPL-3.0-or-later
#include <assert.h>
#include <stdint.h>
typedef uint8_t u8;
#define PNP_NODE_STORAGE
#include "../src/pnp-fixed-nodes.h"

static void check_node(const u8 *node, unsigned int size, unsigned int handle)
{
    assert(node[0] == size && node[1] == 0 && node[2] == handle);
    assert(node[10] == 3 && node[11] == 0);
    unsigned int pos = 12;
    for (unsigned int block = 0; block < 3; block++) {
        for (;;) {
            assert(pos < size);
            unsigned int tag = node[pos++];
            unsigned int len = tag & 7;
            assert(!(tag & 0x80) && pos + len <= size);
            pos += len;
            if (tag == 0x79)
                break;
            assert(block == 0);
        }
    }
    assert(pos == size);
}

int main(void)
{
    check_node(PnpPicNode, sizeof(PnpPicNode), 2);
    check_node(PnpDmaNode, sizeof(PnpDmaNode), 3);
    check_node(PnpPitNode, sizeof(PnpPitNode), 4);
    check_node(PnpRtcNode, sizeof(PnpRtcNode), 5);
    assert(PnpDmaNode[60] == 0x2a && PnpDmaNode[61] == 0x10);
    for (unsigned int bits = 0; bits < 256; bits++) {
        unsigned int mask = pnp_core_node_mask(bits);
        assert(mask == (3U | (bits <= 15 ? bits << 2 : 0)));
        unsigned int seen = 1;
        for (unsigned int handle = pnp_next_node(mask, 0); handle != 0xff;
             handle = pnp_next_node(mask, handle)) {
            assert(handle < PNP_HANDLE_LIMIT);
            seen |= 1U << handle;
        }
        assert(seen == mask);
    }
    return 0;
}
