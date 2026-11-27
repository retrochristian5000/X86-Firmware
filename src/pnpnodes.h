// Experimental fixed motherboard device-node inventory.
#ifndef __PNPNODES_H
#define __PNPNODES_H

#include "types.h"

int pnp_nodes_setup(int acpi_present);
u16 pnp_get_node_count(u16 *args);
u16 pnp_get_node(u16 *args);
u16 pnp_set_node(u16 *args);
u16 pnp_get_isa_config(u16 *args);

#endif
