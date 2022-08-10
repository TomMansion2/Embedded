#ifndef COMPUTATIONALNODE_H
#define COMPUTATIONALNODE_H

#include "contiki.h"
#include "sys/etimer.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"

#include "routing.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define HELLO 0
#define ROOT_HELLO 1
#define SENSOR_UPDATE 2
#define VALVE_CONTROL 3

#define N_NODES 5
#define SLOPE_THRESHOLD 1
#define COMP_TIMEOUT 5

typedef struct {
    int id;
    uint8_t values[30];
    uint8_t index, length, timeout;
} child_t;

// Callback function that handles all incoming messages.
void callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest);

// Broadcasts the HELLO message to all neighbours.
// The message contains the list of this node's parents.
void broadcastSensor();

// Initializer for the parents array.
void initParents();

// Specific message type handlers used in the callback function.
/* ------------------------------------------------------------- */
void processValveControl(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest);

void processSensorUpdate(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest);

void processRootHello(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest);

void processHello(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest);
/* ------------------------------------------------------------- */

// Resets the parents array.
void resetParent();

// Initializer for the children data structure which holds all information about children this comp node is responsible for.
void initChildren();

// Returns the index of the first available slot in our children array.
// Returns -1 if there is no free slot.
int freeSpaceIndex();

// This function is called to handle incoming SENSOR_UPDATE messages. 
// It is responsible for maintaining the last 30 sensor values we received from a node and computes the slope of the least squares fit if needed.
// Returns :
// 0 if we can't handle the data for node id. The calling function then knows to forward their SENSOR_UPDATE messages.
// -1 if we are handling the data but the valve of node id doesn't need to be opened.
// id if we determine that node id needs to open its valve.
int updateChildrenList(uint8_t value, int id);

// Finds the index of node id in the children array.
// Returns -1 if the node is not in the array.
int nodeInChildren(uint8_t id);

// Computes the slope of the least squares fit on the last 30 values sent by node id.
double leastSquares(uint8_t id);

// Debugging function to print the children array.
void printChildrenList();

// Decrements the timeout of each child we are responsible for and removes them from the children array if they've timed out.
void updateChildrenTimeouts();

// Removes the child at index i from the children array.
// /!\ i is not the id of the node but it's index in the child array.
void removeChild(int i);

#endif