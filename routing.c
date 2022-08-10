#include "routing.h"

// The routing table for one node. It stores the addresses of direct children through which we can reach other children and the timeout for each (distant) child.
// routingTable[i].intermediateNode = address of parent of node i.
node_t routingTable[256]; 

void initRouting() {
    for(int i = 0; i < 256; i++) {
        routingTable[i].intermediateNode = linkaddr_null;
        routingTable[i].timeout = TIMEOUT;
        routingTable[i].reachable = false;
    }
}

void updateNode(uint8_t node, linkaddr_t* intermediateNode) {
    memcpy(&routingTable[node].intermediateNode, intermediateNode, 8 * sizeof(uint8_t));
    routingTable[node].timeout = TIMEOUT;
    routingTable[node].reachable = true;
}

void updateTimeouts() {
    for (int i = 0; i < 256; i++) {
        if(!routingTable[i].reachable) continue; // This node is not in the routing table, do nothing.

        if(--routingTable[i].timeout == 0) { // This node hasn't sent an update in TIMEOUT iterations, delete it from the routing table.
            routingTable[i].reachable = false; 
        }
    }
}

void getDirectChild(uint8_t id, linkaddr_t* dest) {
    memcpy(dest, &routingTable[id].intermediateNode, 8 * sizeof(uint8_t));
}

void printRoutingTable() {
    for(int i = 0; i < 256; i++) {
        if(routingTable[i].reachable) {
            printf("Node %d reachable via %u\n", i, routingTable[i].intermediateNode.u8[0]);
        }
    }
}
