/* WNC14A2A implementation of NetworkInterfaceAPI
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef WNC14A2A_INTERFACE_H
#define WNC14A2A_INTERFACE_H

#include <stdint.h>

#include "mbed.h"
#include "Callback.h"
#include "WNCDebug.h"
#include "WncControllerK64F/WncControllerK64F.h"

#define WNC14A2A_SOCKET_COUNT 5


typedef struct smsmsg_t {
        string number;
        string date;
        string time;
        string msg;
    } IOTSMS;

typedef struct socket_t {
        int socket;                         //index of this socket
        string url;
        SocketAddress addr;                 //hold info for this socket
        bool opened;                        //has the socket been opened
        bool _wnc_opened;                   //has the socket been opened
        int proto;                          //this is a TCP or UDP socket
        void (*_callback)(void*);           //callback used with attach
        void *_cb_data;                     //callback data to be returned
    } WNCSOCKET;

#define WNC_DEBUG   0           //1=enable the WNC startup debug output
                                //0=disable the WNC startup debug output
#define STOP_ON_FE  1           //1=hang forever if a fatal error occurs
                                //0=simply return failed response for all socket calls
#define DISPLAY_FE  1           //1 to display the fatal error when it occurs
                                //0 to NOT display the fatal error
#define RESETON_FE  0           //1 to cause the MCU to reset on fatal error
                                //0 to NOT reset the MCU

#define APN_DEFAULT             "m2m.com.attz"
                        
//
// WNC Error Handling macros & data
//
#define FATAL_FLAG  WncController::WNC_NO_RESPONSE
#define WNC_GOOD    WncController::WNC_ON

#define RETfail return -1
#define RETvoid return
#define RETnull return NULL
#define RETresume   

#define DORET(x) RET##x

#define TOSTR(x) #x
#define INTSTR(x) TOSTR(x)
#define FATAL_STR (char*)(__FILE__ ":" INTSTR(__LINE__))

#if RESETON_FE == 1   //reset on fatal error
#define MCURESET     ((*((volatile unsigned long *)0xE000ED0CU))=(unsigned long)((0x5fa<<16) | 0x04L))
#define RSTMSG       "RESET MCU! "
#else
#define MCURESET
#define RSTMSG       ""
#endif

#if DISPLAY_FE == 1  //display fatal error message
#define PFE     {if(_debugUart)_debugUart->printf((char*)RSTMSG "\r\n>>WNC FAILED @ %s\r\n", FATAL_STR);}
#else
#define PFE
#endif

#if STOP_ON_FE == 1  //halt cpu on fatal error
#define FATAL_WNC_ERROR(v)  {_fatal_err_loc=FATAL_STR;PFE;MCURESET;while(1);}
#else
#define FATAL_WNC_ERROR(v)  {_fatal_err_loc=FATAL_STR;PFE;DORET(v);}
#endif

#define CHK_WNCFE(x,y)    if( x ){FATAL_WNC_ERROR(y);}

#define MAX_SMS_MSGS    3

using namespace WncController_fk;
 
/** WNC14A2AInterface class
 *  Implementation of the NetworkInterface for WNC14A2A 
 */
class WNC14A2AInterface : public NetworkStack, public CellularInterface
{
public:
    /** WNC14A2AInterface Constructors...
     * @param can include an APN string and/or a debug uart
     */
    WNC14A2AInterface(WNCDebug *_dbgUart = NULL);
    ~WNC14A2AInterface();

    /** Set the cellular network APN and credentials
     *
     *  @param apn      Optional name of the network to connect to
     *  @param user     Optional username for the APN
     *  @param pass     Optional password fot the APN
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t set_credentials(const char *apn = 0,
            const char *username = 0, const char *password = 0);
 
    /** Start the interface
     *
     *  @param apn      Optional name of the network to connect to
     *  @param username Optional username for your APN
     *  @param password Optional password for your APN 
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t connect(const char *apn,
            const char *username = 0, const char *password = 0);
 
    /** Start the interface
     *
     *  Attempts to connect to a cellular network based on supplied credentials
     *
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t connect();

    /** Stop the interface
     *
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t disconnect();
 
    /** Get the internally stored IP address. From NetworkStack Class
     *  @return             IP address of the interface or null if not yet connected
     */
    virtual const char *get_ip_address();
 
    /** Get the network assigned IP address.
     *  @return             IP address of the interface or null if not yet connected
     */
    const char *get_my_ip_address();

    /** Get the internally stored MAC address.  From CellularInterface Class
     *  @return             MAC address of the interface
     */
    virtual const char *get_mac_address();
 
    /** Attach a function to be called when a text is recevieds
     *  @param callback  function pointer to a callback that will accept the message 
     *  contents when a text is received.
     */
    void sms_attach(void (*callback)(IOTSMS *));

    void doDebug(int v);

    bool registered();

    void sms_start(void);

    /** start listening for incomming SMS messages
     *  @param time in msec to check
     */
    void sms_listen(uint16_t=1000);         // Configure device to listen for text messages 
        
    int getSMS(IOTSMS **msg);

    int sendIOTSms(const string&, const string&);

    char* getSMSnbr();


protected:

    /** Get Host IP by name. From NetworkStack Class
     */
    virtual nsapi_error_t gethostbyname(const char* name, SocketAddress *address, nsapi_version_t version);


    /** Provide access to the NetworkStack object
     *
     *  @return The underlying NetworkStack object
     */
    virtual NetworkStack *get_stack();

    /** Open a socket. FROM NetworkStack
     *  @param handle       Handle in which to store new socket
     *  @param proto        Type of socket to open, NSAPI_TCP or NSAPI_UDP
     *  @return             0 on success, negative on failure
     */
    virtual int socket_open(void **handle, nsapi_protocol_t proto);
 
    /** Close the socket. FROM NetworkStack
     *  @param handle       Socket handle
     *  @return             0 on success, negative on failure
     *  @note On failure, any memory associated with the socket must still 
     *        be cleaned up
     */
    virtual int socket_close(void *handle);
 
    /** Bind a server socket to a specific port.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param address      Local address to listen for incoming connections on 
     *  @return             0 on success, negative on failure.
     */
    virtual int socket_bind(void *handle, const SocketAddress &address);
 
    /** Start listening for incoming connections.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param backlog      Number of pending connections that can be queued up at any
     *                      one time [Default: 1]
     *  @return             0 on success, negative on failure
     */
    virtual int socket_listen(void *handle, int backlog);
 
    /** Connects this TCP socket to the server.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param address      SocketAddress to connect to
     *  @return             0 on success, negative on failure
     */
    virtual int socket_connect(void *handle, const SocketAddress &address);
 
    /** Accept a new connection.FROM NetworkStack
     *  @param handle       Handle in which to store new socket
     *  @param server       Socket handle to server to accept from
     *  @return             0 on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual int socket_accept(nsapi_socket_t server,
            nsapi_socket_t *handle, SocketAddress *address=0);
 
    /** Send data to the remote host.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param data         The buffer to send to the host
     *  @param size         The length of the buffer to send
     *  @return             Number of written bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual int socket_send(void *handle, const void *data, unsigned size);
 
    /** Receive data from the remote host.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param data         The buffer in which to store the data received from the host
     *  @param size         The maximum length of the buffer
     *  @return             Number of received bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual int socket_recv(void *handle, void *data, unsigned size);
 
    /** Send a packet to a remote endpoint.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param address      The remote SocketAddress
     *  @param data         The packet to be sent
     *  @param size         The length of the packet to be sent
     *  @return the         number of written bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual int socket_sendto(void *handle, const SocketAddress &address, const void *data, unsigned size);
 
    /** Receive a packet from a remote endpoint.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param address      Destination for the remote SocketAddress or null
     *  @param buffer       The buffer for storing the incoming packet data
     *                      If a packet is too long to fit in the supplied buffer,
     *                      excess bytes are discarded
     *  @param size         The length of the buffer
     *  @return the         number of received bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual int socket_recvfrom(void *handle, SocketAddress *address, void *buffer, unsigned size);
 
    /** Register a callback on state change of the socket.FROM NetworkStack
     *  @param handle       Socket handle
     *  @param callback     Function to call on state change
     *  @param data         Argument to pass to callback
     *  @note Callback may be called in an interrupt context.
     */
    virtual void socket_attach(void *handle, void (*callback)(void *), void *data);
    
    /** check for errors that may have occured
     *  @param none.
     *  @note this function can be called after any WNC14A2A class operation 
     *        to determine error specifics if desired.
     */
    uint16_t wnc14a2a_chk_error(void) { return _errors; }


private:
    // WncController Class for managing the 14A2a
    friend class WncControllerK64F;  

    bool m_wncpoweredup;                    //track if WNC has been power-up
    bool m_debug;

    WncIpStats myNetStats;                  //maintaint the network statistics
    WncControllerK64F_fk::WncControllerK64F *_pwnc; //pointer to the WncController instance

    int m_active_socket;                    // a 'pseudo' global to track the active socket
    WNCDebug *_debugUart;                     // Serial object for parser to communicate with radio
    char *_fatal_err_loc;                   // holds string containing location of fatal error
    nsapi_error_t _errors;

    bool m_smsmoning;                       // Track if the SMS monitoring thread is running
    EventQueue sms_queue;                   // Queue used to schedule for SMS checks
    Semaphore sms_rx_sem;                   // Semaphore to signal sms_event_thread to check for incoming text 
    void (*_sms_cb)(IOTSMS *);              // Callback when text message is received. User must define this as 
                                            // a static function because I'm not handling an object offset
    IOTSMS m_MsgText, m_MsgText_array[MAX_SMS_MSGS];       // Used to pass SMS message to the user
    struct WncController::WncSmsList m_smsmsgs;            //use the WncSmsList structure to hold messages

    void handle_sms_event();                // Handle incoming text data

    char _mac_address[NSAPI_MAC_SIZE];      // local Mac
    void debugOutput(WNCDebug *dbgOut, char * format, ...);
};

#endif

