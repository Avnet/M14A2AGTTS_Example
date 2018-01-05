
#include "mbed.h"

/*
 * Instantiate the configured network interface
 */

#include "easy-connect.h"
#include "WNC14A2AInterface.h"

/* \brief print_MAC - print_MAC  - helper function to print out MAC address
 * in: network_interface - pointer to network i/f
 *     bool log-messages   print out logs or not
 * MAC address is printed, if it can be acquired & log_messages is true.
 *
 */
void print_MAC(NetworkInterface* network_interface, bool log_messages) {
    const char *mac_addr = network_interface->get_mac_address();
    if (mac_addr == NULL) {
        if (log_messages) {
            printf("[EasyConnect] ERROR - No MAC address\n");
        }
        return;
    }
    if (log_messages) {
        printf("[EasyConnect] MAC address %s\n", mac_addr);
    }
}

WNC14A2AInterface *wnc;
#if MBED_CONF_APP_WNC_DEBUG == true
#include "WNCDebug.h"
WNCDebug dbgout(stderr);
#endif


NetworkInterface* easy_connect(bool log_messages) {
    NetworkInterface* network_interface = NULL;
    int connect_success = -1;

    if (log_messages) {
        printf("[EasyConnect] Using WNC14A2A\n");
#if MBED_CONF_APP_WNC_DEBUG == true
        printf("[EasyConnect] with debug output\n");
        printf("[WNC Driver ] debug = %d\n", MBED_CONF_APP_WNC_DEBUG_SETTING);
        wnc = new WNC14A2AInterface(&dbgout);
        wnc->doDebug(MBED_CONF_APP_WNC_DEBUG_SETTING);
#else
        wnc = new WNC14A2AInterface;
#endif
    }

    connect_success = wnc->connect();
    network_interface = wnc;

    if(connect_success == 0) {
        if (log_messages) {
            printf("[EasyConnect] Connected to Network successfully\n");
            print_MAC(network_interface, log_messages);
        }
    } else {
        if (log_messages) {
            print_MAC(network_interface, log_messages);
            printf("[EasyConnect] Connection to Network Failed %d!\n", connect_success);
        }
        return NULL;
    }

    const char *ip_addr  = network_interface->get_ip_address();
    if (ip_addr == NULL) {
        if (log_messages) {
            printf("[EasyConnect] ERROR - No IP address\n");
        }
        return NULL;
    }

    if (log_messages) {
        printf("[EasyConnect] IP address %s\n", ip_addr);
    }
    return network_interface;
}

