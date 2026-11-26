// Experimental fixed motherboard device-node inventory.
#ifndef __PNPNODES_H
#define __PNPNODES_H

#include "types.h"

int pnp_nodes_setup(void);
u16 pnp_get_node_count(u16 *args);
u16 pnp_get_node(u16 *args);
u16 pnp_set_node(u16 *args);

#endif
