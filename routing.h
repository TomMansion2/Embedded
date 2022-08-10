#ifndef ROUTING_H
#define ROUTING_H

#define TIMEOUT 2

#include <stdio.h>
#include <string.h>

#include "net/nullnet/nullnet.h"

typedef struct {
    linkaddr_t intermediateNode;
    uint8_t timeout;
    bool reachable;
} node_t;

// Initializer for the routing table data structure.
void initRouting();

// Updates a node's address and timeout in the routing table.
// Here, the node's address means the address of the node through which we can reach node 'node'.
void updateNode(uint8_t node, linkaddr_t* intermediateNode);

// Copies into dest the address of the child we must go through to reach id.
void getDirectChild(uint8_t id, linkaddr_t* dest); 

// Decrements all timeouts in the routing table and removes nodes from the table if they've timed out.
void updateTimeouts();

// Debug function to print a node's routing table.
void printRoutingTable();

#endif