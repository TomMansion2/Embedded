#ifndef SENSORNODE_H
#define SENSORNODE_H

#include "contiki.h"
#include "sys/etimer.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "dev/leds.h"
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

// Sends a unicast transmission with the value data to this node's direct parent.
void sendSensorData(uint8_t data);

// Resets the parents array.
void resetParent();

// Print all information on this node
void debug(); 

#endif