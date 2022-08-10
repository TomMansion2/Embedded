#include "sensorNode.h"

PROCESS(sensor_node, "Sensor Node");
PROCESS(valve_process, "Valve Process");
AUTOSTART_PROCESSES(&sensor_node, &valve_process);

static struct etimer sensorTimer;
static struct etimer valveTimer;
linkaddr_t parent = {{0,0,0,0,0,0,0,0}};
uint8_t parents[256];
uint8_t parentsLength;
uint8_t parentStrength;
uint8_t parentTimeout;
uint8_t myID;
bool valve = false;
// NOTE : linkaddr_node_addr.u8[0] contains the ID of the mote, we can use it as its address.

PROCESS_THREAD(sensor_node, ev, data) {
    PROCESS_BEGIN();
    LOG_INFO("Sensor Node\n");

    // Initialisation of the data structures.
    initParents();
    initRouting();
    myID = linkaddr_node_addr.u8[0];
    nullnet_set_input_callback(callback);

    etimer_set(&sensorTimer, 60* CLOCK_SECOND);

    while(1) {
        // Update the timeout counters for the parent and the children in the routing table.
        if(parentsLength != 0) {
            if(--parentTimeout == 0) 
                resetParent();
        }
        updateTimeouts();

        // Broadcast the HELLO message.
        broadcastSensor();

        // Send the sensor data to our parent if we have one.
        uint8_t sensorData = (uint8_t) rand() % 100;
        sendSensorData(sensorData);

        // Repeat every minute.
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sensorTimer));
        etimer_restart(&sensorTimer);
    }
    PROCESS_END();
}

PROCESS_THREAD(valve_process, ev, data) {
    PROCESS_BEGIN();
    LOG_INFO("Valve Process\n");

    static struct etimer check; 
    etimer_set(&check, 60 * CLOCK_SECOND);

    while(1) {
        if(valve) { // Valve needs to be opened because we got a VALVE_CONTROL msg from the server
            LOG_INFO("opening the valve\n");
            etimer_set(&valveTimer, 600 * CLOCK_SECOND); // Start the 10 minute timer for the valve.
            leds_on(LEDS_BLUE);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&valveTimer));
            leds_off(LEDS_BLUE);
            valve = false;
            etimer_restart(&valveTimer);
        }
        // If we don't use a timer and check periodically, the other process just doesn't run.
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&check));
        etimer_restart(&check);
    }

    PROCESS_END();
}

void callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    uint8_t received[len];
	memcpy(&received, data, len);

    linkaddr_t source = {{0,0,0,0,0,0,0,0}};
    memcpy(&source, src, sizeof(linkaddr_t));

    switch(received[0]) {
        case HELLO:
            processHello(received, len, &source, dest);
            break;

        case ROOT_HELLO:
            processRootHello(received, len, &source, dest);
            break;

        case SENSOR_UPDATE:
            processSensorUpdate(received, len, &source, dest);
            break;

        case VALVE_CONTROL:
            processValveControl(received, len, &source, dest);
            break;
    }

    // LOG_INFO("Received %d bytes with signal strength %u from \t%u\n", len, (uint8_t) packetbuf_attr(PACKETBUF_ATTR_RSSI), src->u8[0]);

    // for(int i = 1; i < len; i++) {
    //     LOG_INFO("Payload %d : \t%u\n", i, received[i]);
    // }

    // debug();
}

void broadcastSensor() {
    // Create a buffer for the HELLO message. It contains the HELLO value in the first byte and then an ID for each parent node in the following bytes.
    uint8_t msg[parentsLength + 1];
    msg[0] = HELLO;
    memcpy(msg + 1, parents, parentsLength);

    nullnet_buf = msg;
    nullnet_len = parentsLength + 1;
    NETSTACK_NETWORK.output(NULL);
}

void processHello(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest) {
    if(len == 1) return; // The broadcast message sender doesn't have any parents so not interesting.

    uint8_t signalStrength = packetbuf_attr(PACKETBUF_ATTR_RSSI);

    // We must first check that the message sender is not our child.
    for(int i = 1; i < len; i++) {
        if(received[i] == myID)
            return; // We are already the parent of the sender.
    }

    if(parentsLength == 0) { // We don't have a parent yet so attach to this node who is connected to the border router
        memcpy(&parent, src, 8 * sizeof(uint8_t));
        parentsLength = len;
        memcpy(parents, received + 1, parentsLength - 1);
        parents[parentsLength - 1] = src->u8[0];
        parentStrength = signalStrength;
        parentTimeout = TIMEOUT;
        return;
    }
    // We already have a parent so we must measure the signal strength to decide if we switch or not.

    // Either the sender is already our parent or it has a better signal strength than our parent so we take him as our new parent.
    // Update signal strength, parents list and parent timeout.
    if(linkaddr_cmp(&parent, src) || signalStrength > parentStrength) { 
        memcpy(&parent, src, 8 * sizeof(uint8_t));
        parentStrength = signalStrength;
        parentTimeout = TIMEOUT;
        parentsLength = len; // Our parent's parent list.
        memcpy(parents, received + 1, parentsLength - 1);
        parents[parentsLength - 1] = src->u8[0];
        memset(parents + parentsLength, 0, 256 - parentsLength); // Fill the rest of our parents list with unused values in case the list is shortened.
    }
}

void processRootHello(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest) {
    uint8_t signalStrength = packetbuf_attr(PACKETBUF_ATTR_RSSI);

    if(parentsLength == 0) { // We don't have a parent yet so connect to the border router
        memcpy(&parent, src, 8 * sizeof(uint8_t));
        parentsLength = 1;
        memcpy(parents, src, (uint8_t) 1);
        parentStrength = signalStrength;
        parentTimeout = TIMEOUT;
        return;
    }

    if(linkaddr_cmp(&parent, src)) { // The border router is already our parent, update the signal strength and the timeout.
        parentStrength = signalStrength;
        parentTimeout = TIMEOUT;
        return;
    }

    if(signalStrength > parentStrength) { // The border router is not our parent and has a better signal strength than our parent so we take him as our new parent.
        memcpy(&parent, src, 8 * sizeof(uint8_t));
        parentsLength = 1;
        parentStrength = signalStrength;
        memcpy(parents, src, (uint8_t) 1);
        memset(parents + parentsLength, 0, 256 - parentsLength);
    }
}

void processSensorUpdate(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest) {
    // Step 1: Update routing table.
    updateNode(received[2], src); // received[2] always contains the ID of the node that sent the value.
    // Step 2: Forward sensor update message.
    nullnet_buf = received;
    nullnet_len = len;

    NETSTACK_NETWORK.output(&parent);
}

void processValveControl(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest) {
    if(received[1] != myID) { // Simply forward the message
        linkaddr_t firstNodeOnPath;
        getDirectChild(received[1], &firstNodeOnPath);

        nullnet_buf = received;
        nullnet_len = 2;

        NETSTACK_NETWORK.output(&firstNodeOnPath);
        return;
    }
    LOG_INFO("Set the valve to open\n");
    valve = true;
}

void sendSensorData(uint8_t data) {
    if(parentsLength == 0) return;

    uint8_t msg[3];
    msg[0] = SENSOR_UPDATE;
    msg[1] = data;
    msg[2] = myID;

    nullnet_buf = msg;
    nullnet_len = 3 * sizeof(uint8_t);

    NETSTACK_NETWORK.output(&parent);
}

void initParents() {
    memset(parents, 0, 256); // Since the parents array stores the IDs of the motes in Contiki which start from 1, we use 0 as an unused value.
    parentsLength = 0;
}

void resetParent() {
    parentsLength = 0;
    parentStrength = 0;
    memset(parents, 0, 256);
}

void debug() {
    // Parents length
    printf("Parents length : %d\n", parentsLength);
    // Parents list
    for(int i = 0; i < parentsLength; i++) {
        printf("Parent %d : %d\n", i, parents[i]);
    }
    // Parent signal strength
    printf("Parent Signal Strength : %d\n", parentStrength);
    // Parent timeout
    printf("Parent timeout : %u\n", parentTimeout);
    // Routing table TODO
    printf("Routing table : \n");
    printRoutingTable();
}
