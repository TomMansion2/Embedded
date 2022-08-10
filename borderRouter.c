#include "borderRouter.h"

PROCESS(border_router, "Border Router");
PROCESS(listener, "Listener for incoming server commands");
AUTOSTART_PROCESSES(&border_router, &listener);

static struct etimer timer;

PROCESS_THREAD(border_router, ev, data) {
    PROCESS_BEGIN();
    printf("Border Router\n");
    etimer_set(&timer, 60* CLOCK_SECOND);
    initRouting();

    nullnet_set_input_callback(callback);

    while(1) {
        broadcastRouter();   
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        etimer_restart(&timer);
    }
    
    PROCESS_END();
}

PROCESS_THREAD(listener, ev, data) {
    PROCESS_BEGIN();
    serial_line_init();
    uart0_set_input(serial_line_input_byte);

    nullnet_set_input_callback(callback);

    while(1) {
        // Listen to incoming messages from the server and send them back to their dests.
        PROCESS_YIELD();
        if(ev==serial_line_event_message){
            // We received a VALVE_CONTROL msg so forward it to the appropriate child.
   	        // LOG_INFO("received line: %s\n", (char*) data);
            linkaddr_t firstNodeOnPath;
            getDirectChild(atoi(data), &firstNodeOnPath);

            uint8_t msg[2];
            msg[0] = VALVE_CONTROL;
            msg[1] = atoi(data);

            // LOG_INFO("Sending VALVE_CONTROL msg to %u via %u\n", msg[1], firstNodeOnPath.u8[0]);

            nullnet_buf = msg;
            nullnet_len = 2;

            NETSTACK_NETWORK.output(&firstNodeOnPath);
        }
    }

    PROCESS_END();
}

void broadcastRouter() {
    updateTimeouts();     

    uint8_t msg[2];
    msg[0] = ROOT_HELLO;
    msg[1] = linkaddr_node_addr.u8[0];

    nullnet_buf = msg;
    nullnet_len = 2;

    NETSTACK_NETWORK.output(NULL);
}

void callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    uint8_t received[len];
	memcpy(&received, data, len);

    if(received[0] != SENSOR_UPDATE) return; // The border router only processes sensor update messages.

    linkaddr_t source = {{0,0,0,0,0,0,0,0}};
    memcpy(&source, src, sizeof(linkaddr_t));

    // Step 1: Update routing table.
    updateNode(received[2], &source); // received[2] always contains the ID of the node that sent the value.
    // Step 2: Forward data to server. We use printf to write data on serial line output.
    printf("%u;%u\n", received[1], received[2]);
}