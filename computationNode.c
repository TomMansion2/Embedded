#include "computationNode.h"

PROCESS(computationNode, "Computation Node");
AUTOSTART_PROCESSES(&computationNode);

static struct etimer timer;
linkaddr_t parent = {{0,0,0,0,0,0,0,0}};
uint8_t parents[256];
uint8_t parentsLength;
uint8_t parentStrength;
uint8_t parentTimeout;
uint8_t myID;

child_t children[N_NODES]; // The data structure for the children we provide data analysis for.

PROCESS_THREAD(computationNode, ev, data) {
    PROCESS_BEGIN();
    LOG_INFO("Computation Node\n");

    // Initialisation of the data structures.
    initParents();
    initRouting();
    initChildren();
    myID = linkaddr_node_addr.u8[0];
    nullnet_set_input_callback(callback);

    etimer_set(&timer, 60* CLOCK_SECOND);

    while(1) {
        // Update the timeout counters for the parent and the children in the routing table.
        if(parentsLength != 0) {
            if(--parentTimeout == 0) 
                resetParent();
        }
        updateTimeouts();
        updateChildrenTimeouts();

        // Broadcast the HELLO message.
        broadcastSensor();

        // Repeat every minute.
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        etimer_restart(&timer);
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
        parentTimeout = COMP_TIMEOUT;
        return;
    }
    // We already have a parent so we must measure the signal strength to decide if we switch or not.

    // Either the sender is already our parent or it has a better signal strength than our parent so we take him as our new parent.
    // Update signal strength, parents list and parent COMP_TIMEOUT.
    if(linkaddr_cmp(&parent, src) || signalStrength > parentStrength) { 
        memcpy(&parent, src, 8 * sizeof(uint8_t));
        parentStrength = signalStrength;
        parentTimeout = COMP_TIMEOUT;
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
        parentTimeout = COMP_TIMEOUT;
        return;
    }

    if(linkaddr_cmp(&parent, src)) { // The border router is already our parent, update the signal strength and the timeout.
        parentStrength = signalStrength;
        parentTimeout = COMP_TIMEOUT;
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

    // LOG_INFO("Getting sensor update msg from %u with value %u\n", received[2], received[1]);

    // Step 2: Check if we are responsible for that node's computations and handle the update.
    int id = updateChildrenList(received[1], received[2]);

    printChildrenList();
    
    if(id == 0) {
        // Step 3: If not, forward sensor update message.
        nullnet_buf = received;
        nullnet_len = len;

        NETSTACK_NETWORK.output(&parent);
    }
    else if(id > 0) {
        // Send a VALVE_CONTROL msg to the Node id.
        linkaddr_t firstNodeOnPath;
        getDirectChild(id, &firstNodeOnPath);

        uint8_t msg[2];
        msg[0] = VALVE_CONTROL;
        msg[1] = id;

        nullnet_buf = msg;
        nullnet_len = 2;
        NETSTACK_NETWORK.output(&firstNodeOnPath);
    }
    
}

void processValveControl(uint8_t* received, uint16_t len, linkaddr_t *src, const linkaddr_t *dest) {
    linkaddr_t firstNodeOnPath;
    getDirectChild(received[1], &firstNodeOnPath);

    nullnet_buf = received;
    nullnet_len = 2;

    NETSTACK_NETWORK.output(&firstNodeOnPath);
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

// Returns 0 if the node doesn't need to open its valve.
//         The id of the node we need to send a msg to if the least squares slope is above the threshold.
int updateChildrenList(uint8_t value, int id) {
    int index = nodeInChildren(id);
    if(index != -1) {
        // This node is already in our children list.
        // Add value to the array of values, update the curent index and length and reset the timeout.
        children[index].values[children[index].index + children[index].length % 30] = value;
        children[index].timeout = COMP_TIMEOUT;

        if(children[index].length < 30) {
            // Store the value and return a negative value.
            children[index].length++;
            return -1;
        }
        // We now have 30 values in our array so we can compute the least squares fit on them.
        // Increment the index because we just overwrote the oldest value.
        children[index].index = (children[index].index + 1) % 30;
        double slope = leastSquares(index);
        printf("Slope value for node %u: %f\n", id, slope);
        if(slope > (double) SLOPE_THRESHOLD) { // Send the valve control message to the relevant child.
            LOG_INFO("Node %u needs to open its valve\n", id);
            return id;
        }
        // The node should not open its valve so do nothing.
        return -1;
    }
    // The node is not in our children list.
    // Check if we have space for this new node.
    else {
        index = freeSpaceIndex();
        if(index < 0) // No free space so we can't handle this node. Forward the data.
            return 0;
        // There is a free spot for a node at index. Add this node to our children.
        children[index].id = id;
        children[index].values[0] = value;
        children[index].index = 0;
        children[index].length = 1;
        children[index].timeout = COMP_TIMEOUT;

        return -1; // Return a negative value to specify we don't need to forward the message as it's being handled by this comp node.
    }
}

int nodeInChildren(uint8_t id) {
    for(int i = 0; i < N_NODES; i++) {
        if(children[i].id == id) {
            return i;
        }
    }
    return -1;
}

int freeSpaceIndex() {
    for(int i = 0; i < N_NODES; i++) {
        if(children[i].id == -1)
            return i;
    }
    return -1;
}

// Initialize the children structure to contain only invalid ids.
void initChildren() {
    for(int i = 0; i < N_NODES; i++) {
        children[i].id = -1;
        children[i].index = 0;
        children[i].length = 0;
        children[i].timeout = 0;
    }
}

double leastSquares(uint8_t id) {
    double sumx = 435;
    double   sumxy = 0.0;
    double   sumy = 0.0;                      
    double   sumy2 = 0.0;          

    for (int i = 0; i < 30; i++) { 
        double yValue = children[id].values[children[id].index + i % 30];
        sumxy += i * yValue;
        sumy  += yValue;
        sumy2 += yValue * yValue; 
    }

    double denom = 67425;
    return ((30 * sumxy - sumx * sumy) / denom);
}

void printChildrenList() {
    for(int i = 0; i < N_NODES; i++) {
        LOG_INFO("Child %d has ID %d, index %d and length %d \n", i, children[i].id, children[i].index, children[i].length);
        for(int j = children[i].index; j < children[i].index + children[i].length; j++) {
            printf("Value %d: %u\n", j % 30, children[i].values[j % 30]);
        }
    }
}

void updateChildrenTimeouts() {
    for(int i = 0; i < N_NODES; i++) {
        if(children[i].id == -1) continue;

        if(--children[i].timeout == 0) removeChild(i);
    }
}

void removeChild(int i) {
    children[i].id = -1;
    children[i].index = 0;
    children[i].length = 0;
    children[i].timeout = 0;
}