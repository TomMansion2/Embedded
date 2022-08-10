#ifndef BORDERROUTER_H
#define BORDERROUTER_H

#include "contiki.h"
#include "sys/etimer.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "dev/serial-line.h"
#include "cpu/msp430/dev/uart0.h"
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

// Broadcast the HELLO message to all neighbours. The message simply contains the id of the border router.
void broadcastRouter();

// Callback function for handling incoming messages.
void callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest);


#endif