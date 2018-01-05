
#include "WNC14A2AInterface.h"
#include <Thread.h>
#include <string> 

/** WNC14A2AInterface class
 *  Implementation of the NetworkInterface for the AT&T IoT Starter Kit 
 *  based on the WNC 14A2A LTE Data Module
 */

#define WNC14A2A_MISC_TIMEOUT 3000
#define WNC14A2A_RESTART_TIMEOUT 10000
#define WNC14A2A_COMMUNICATION_TIMEOUT 100
#define READ_EVERYMS    500

/////////////////////////////////////////////////////
// NXP GPIO Pins that are used to initialize the WNC Shield
/////////////////////////////////////////////////////
DigitalOut  mdm_uart2_rx_boot_mode_sel(PTC17);  // on powerup, 0 = boot mode, 1 = normal boot
DigitalOut  mdm_power_on(PTB9);                 // 0 = modem on, 1 = modem off (hold high for >5 seconds to cycle modem)
DigitalOut  mdm_wakeup_in(PTC2);                // 0 = let modem sleep, 1 = keep modem awake -- Note: pulled high on shield
DigitalOut  mdm_reset(PTC12);                   // active high
DigitalOut  shield_3v3_1v8_sig_trans_ena(PTC4); // 0 = disabled (all signals high impedence, 1 = translation active
DigitalOut  mdm_uart1_cts(PTD0);

// Define pin associations for the controller class to use be careful to 
//  keep the order of the pins in the initialization list.

using namespace WncControllerK64F_fk;       // namespace for the controller class use

WncGpioPinListK64F wncPinList = { 
    &mdm_uart2_rx_boot_mode_sel,
    &mdm_power_on,
    &mdm_wakeup_in,
    &mdm_reset,
    &shield_3v3_1v8_sig_trans_ena,
    &mdm_uart1_cts
};

Thread smsThread;
static Mutex _pwnc_mutex;

static WNCSOCKET _sockets[WNC14A2A_SOCKET_COUNT];
BufferedSerial mdmUart(PTD3,PTD2,1024,1);       //UART for WNC Module

//-------------------------------------------------------------------------
//
// Class constructor.  May be invoked with or without the APN and/or pointer
// to a debug output.  After the constructor has completed, the user can 
// check _errors to determine if any errors occured during instanciation.
// _errors = 0 when no errors occured
//           1 when power-on error occured
//           2 when settng the APN error occured
//           4 when unable to get the network configuration
//           8 when allocating a new WncControllerK64F object failed
//          NSAPI_ERROR_UNSUPPORTED when attempting to create a second object
//

WNC14A2AInterface::WNC14A2AInterface(WNCDebug *dbg) : 
 m_wncpoweredup(0),
 _pwnc(NULL),
 m_active_socket(-1),
 m_smsmoning(0)
{
    _errors = NSAPI_ERROR_OK;
    
    m_debug=false;

    if( _pwnc ) { //can only have a single instance of class
        _errors =  NSAPI_ERROR_UNSUPPORTED;
        return;
        }
    for( int i=0; i<WNC14A2A_SOCKET_COUNT; i++ ) {
        _sockets[i].socket = i;
        _sockets[i].addr = NULL;
        _sockets[i].opened=false;
        _sockets[i]._wnc_opened=false;
        _sockets[i].proto=NSAPI_TCP;
        }

    
    memset(_mac_address,0x00,sizeof(_mac_address));

    _debugUart = dbg;
    if( dbg != NULL ) {
        dbg->printf((char*)"Adding DEBUG output\n");
        _pwnc = new WncControllerK64F(&wncPinList, &mdmUart, dbg);
        m_debug=true;
        if( _pwnc ) 
            _pwnc->enableDebug(1,1);
        }
    else 
        _pwnc = new WncControllerK64F_fk::WncControllerK64F(&wncPinList, &mdmUart, NULL);
        
    if( !_pwnc ) {
        debugOutput(_debugUart,(char*)", FAILED!\n");
        _errors = NSAPI_ERROR_DEVICE_ERROR;
        }
    else
        debugOutput(_debugUart, (char*)"\n");
}

/*-------------------------------------------------------------------------
 * standard destructor... free up allocated memory
 */

WNC14A2AInterface::~WNC14A2AInterface()
{
    delete _pwnc;  //free the existing WncControllerK64F object
}

nsapi_error_t WNC14A2AInterface::connect() 
{
    debugOutput(_debugUart,(char*)"+CALLED connect(void)\n");
    return connect(NULL,NULL,NULL);
}

/*-------------------------------------------------------------------------
 * This call powers up the WNC module and connects to the user specified APN.  If no APN is provided
 * a default one of 'm2m.com.attz' will be used (the NA APN)
 *
 * Input: *apn is the APN string to use
 *        *username - NOT CURRENTLY USED
 *        *password - NOT CURRENTLY USED
 *
 * Output: none
 *
 * Return: nsapi_error_t
 */
nsapi_error_t WNC14A2AInterface::connect(const char *apn, const char *username, const char *password) 
{
    debugOutput(_debugUart,(char*)"+ENTER connect(apn,user,pass)\n");
    if( !_pwnc )
        return (_errors=NSAPI_ERROR_NO_CONNECTION);

    if (!apn)
        apn = "m2m.com.attz";

    if (!m_wncpoweredup) {
        debugOutput(_debugUart,(char*)"+call powerWncOn using '%s'\n",apn);
        _pwnc_mutex.lock();
        m_wncpoweredup=_pwnc->powerWncOn(apn,40);
        _pwnc_mutex.unlock();
        _errors = m_wncpoweredup? 1:0;
        }
    else { //we've already called powerWncOn and set the APN, so just set the APN 
        debugOutput(_debugUart,(char*)"+already powered on, set APN to: %s\n",apn);
        _pwnc_mutex.lock();
        _errors = _pwnc->setApnName(apn)? 1:0;
        _pwnc_mutex.unlock();
        }

    _pwnc_mutex.lock();
    _errors |= _pwnc->getWncNetworkingStats(&myNetStats)? 2:0;
    _pwnc_mutex.unlock();

    debugOutput(_debugUart,(char*)"+EXIT connect %02X\n",_errors);
    return (!_errors)? NSAPI_ERROR_NO_CONNECTION : NSAPI_ERROR_OK;
}

/*--------------------------------------------------------------------------
 * This function calls the WNC to retrieve the device (this WNC) connected IP 
 * address once we are connected to the APN.
 *
 * Inputs: NONE.
 *
 * Output: none.
 *
 * Return: pointer to the IP string or NULL 
 */
const char *WNC14A2AInterface::get_ip_address()
{
    const char *ptr=NULL; 

    _pwnc_mutex.lock();
    if ( _pwnc->getWncNetworkingStats(&myNetStats) ) {
        CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), null);
        ptr = &myNetStats.ip[0];
    }
    _pwnc_mutex.unlock();
    _errors=NSAPI_ERROR_NO_CONNECTION;
    return ptr;
}

/* -------------------------------------------------------------------------
 * Open a socket for the WNC.  This doesn't actually open the socket within
 * the WNC, it only allocates a socket device and saves the pertinet info
 * that will be required with the WNC socket is opened. The m_active_socket
 * is also updated to this socket as it is assumed this socket should be used
 * for subsequent interactions.
 *
 * Input: 
 *  - a pointer to a handle pointer.  
 *  - The type of socket this will be, either NSAPI_UDP or NSAP_TCP
 *
 * Output: *handle is updated
 *
 * Return:
 *  - socket being used if successful, -1 on failure
 */
int WNC14A2AInterface::socket_open(void **handle, nsapi_protocol_t proto) 
{
    int i;
    debugOutput(_debugUart,(char*)"+ENTER socket_open()\n");

    // search through the available sockets (WNC can only support a max amount).
    for( i=0; i<WNC14A2A_SOCKET_COUNT; i++ )
        if( !_sockets[i].opened )
            break;

    if( i == WNC14A2A_SOCKET_COUNT ) {
        _errors=NSAPI_ERROR_NO_SOCKET;
        return -1;
        }

    m_active_socket = i;           
    _sockets[i].socket = i;                           //save this index to make easier later-on
    _sockets[i].url="";
    _sockets[i].opened = true;                        //ok, we are using this socket now
    _sockets[i]._wnc_opened=false;
    _sockets[i].addr = NULL;                          //but we haven't opened it yet
    _sockets[i].proto = proto;                        //set it up for what WNC wants
    _sockets[i]._callback = NULL;
    _sockets[i]._cb_data = NULL;         
    *handle = &_sockets[i];

    debugOutput(_debugUart,(char*)"+USING Socket index %d, OPEN=%s, protocol =%s\nEXIT socket_open()\n",
    i, _sockets[i].opened?"YES":"NO", (_sockets[i].proto==NSAPI_UDP)?"UDP":"TCP");
    
    _errors = NSAPI_ERROR_OK;
    return i;
}

/*-------------------------------------------------------------------------
 * Connect a socket to a IP/PORT.  Before you can connect a socket, you must have opened
 * it.
 *
 * Input: handle - pointer to the socket to use
 *        address- the IP/Port pair that will be used
 *
 * Output: none
 *
 * return: 0 or greater on success (value is the socket ID)
 *        -1 on failure
 */
int WNC14A2AInterface::socket_connect(void *handle, const SocketAddress &address) 
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;   

    debugOutput(_debugUart,(char*)"+ENTER socket_connect()\n");
    debugOutput(_debugUart,(char*)"+IP  = %s\n+PORT= %d\n", address.get_ip_address(), address.get_port());
    
    if (!_pwnc || m_active_socket == -1) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return -1;
        }

    if( !wnc->opened ) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return -1;
        }
    m_active_socket = wnc->socket;  //in case the user is asking for a different socket
    wnc->addr = address;
                                
    //we will always connect using the URL if possible, if no url has been provided, try the IP address
    if( wnc->url.empty() ) {
        debugOutput(_debugUart,(char*)"+call openSocketIpAddr(%d,%s,%d,%d)\n",m_active_socket, 
                           address.get_ip_address(), address.get_port(), (wnc->proto==NSAPI_UDP)?0:1);
        if( !_pwnc->openSocketIpAddr(m_active_socket, address.get_ip_address(), address.get_port(), 
                                 (wnc->proto==NSAPI_UDP)?0:1, WNC14A2A_COMMUNICATION_TIMEOUT) ) {
            _errors = NSAPI_ERROR_NO_SOCKET;
            return -1;
            }
        }
     else {
        debugOutput(_debugUart,(char*)"+call openSocketUrl(%d,%s,%d,%d)\n", m_active_socket, 
                           wnc->url.c_str(), wnc->addr.get_port(), (wnc->proto==NSAPI_UDP)?0:1);
        if( !_pwnc->openSocketUrl(m_active_socket, wnc->url.c_str(), wnc->addr.get_port(), (wnc->proto==NSAPI_UDP)?0:1) ) {
            _errors = NSAPI_ERROR_NO_SOCKET;
            return -1;
            }
        }
    wnc->_wnc_opened = true;

    debugOutput(_debugUart,(char*)"+SOCKET %d CONNECTED!\n+URL=%s\n+IP=%s; PORT=%d\n+EXIT socket_connect()\n\n",m_active_socket,
                wnc->url.c_str(), wnc->addr.get_ip_address(), wnc->addr.get_port());
    return 0;
}

/*-------------------------------------------------------------------------
 * Perform a URL name resolve, update the IP/Port pair, and nsapi_version. nsapi_version
 * could be either NSAPI_IPv4 or NSAPI_IPv6 but this functional is hard coded ti NSAPI_IPv4
 * for now. The currently active socket is used for the resolution.  
 *
 * Input: name - the URL to resolve
 *
 * Output: address - the IP/PORT pair this URL resolves to
 *         version - always assumed to be NSAPI_IPv4  currently
 *
 * Return: nsapi_error_t
 */

nsapi_error_t WNC14A2AInterface::gethostbyname(const char* name, SocketAddress *address, nsapi_version_t version)
{
    nsapi_error_t ret = NSAPI_ERROR_OK;
    char ipAddrStr[25];
    int  t_socket = 0;  //use a temporary socket place holder

    debugOutput(_debugUart,(char*)"+ENTER gethostbyname()()\n+CURRENTLY:\n");
    debugOutput(_debugUart,(char*)"+URL = %s\n+IP  = %s\n+PORT= %d\n", name, address->get_ip_address(), address->get_port());
    memset(ipAddrStr,0x00,sizeof(ipAddrStr));
    
    if (!_pwnc) 
        return (_errors = NSAPI_ERROR_NO_SOCKET);
        
    if (m_active_socket != -1)      //we might have been called before a socket was opened
        t_socket = m_active_socket; //if so, do nothing with the active socket index

    //Execute DNS query.  
    if( !_pwnc->resolveUrl(t_socket, name) )  
        return (_errors = NSAPI_ERROR_DEVICE_ERROR);

    //Now, get the IP address that the URL was resolved to
    if( !_pwnc->getIpAddr(t_socket, ipAddrStr) )
        return (_errors = NSAPI_ERROR_DEVICE_ERROR);

    address->set_ip_address(ipAddrStr);
    debugOutput(_debugUart,(char*)"+resolveUrl returned IP=%s\n",ipAddrStr);
    if( t_socket == m_active_socket ) {
        _sockets[m_active_socket].url=name;
        _sockets[m_active_socket].addr.set_ip_address(ipAddrStr);
        }

    debugOutput(_debugUart,(char*)"+EXIT gethostbyname()\n+URL = %s\n+IP  = %s\n+PORT= %d\n\n", 
                _sockets[m_active_socket].url.c_str(), address->get_ip_address(), 
                address->get_port());
    _errors = ret;
    return ret;
}
 
/*-------------------------------------------------------------------------
 * using the specified socket, send the data.
 *
 * Input: handle of the socket to use
 *        pointer to the data to send
 *        amount of data being sent
 *
 * Output: none
 *
 * Return: number of bytes that was sent
 */
int WNC14A2AInterface::socket_send(void *handle, const void *data, unsigned size) 
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;
    int r = -1;
    debugOutput(_debugUart,(char*)"+ENTER socket_send()\n");

    if (!_pwnc || m_active_socket == -1) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return 0;
        }
    else
        m_active_socket = wnc->socket; //just in case sending to a socket that wasn't last used

    debugOutput(_debugUart,(char*)"+SOCKET %d is %s, URL=%s, IP=%s, PORT=%d TYPE=%s\n",
        m_active_socket, (char*)wnc->opened?"OPEN":"CLOSED",wnc->url.c_str(),
        wnc->addr.get_ip_address(),wnc->addr.get_port(),(wnc->proto==NSAPI_UDP)?"UDP":"TCP");
    debugOutput(_debugUart,(char*)"+WRITE [%s] (%d bytes) to socket #%d\n",data,size,wnc->socket);

    _pwnc_mutex.lock();
    if( _pwnc->write(m_active_socket, (const uint8_t*)data, size) ) 
       r = size;
    else
       debugOutput(_debugUart,(char*)"+ERROR: write to socket %d failed!\n",m_active_socket);
    _pwnc_mutex.unlock();

    debugOutput(_debugUart,(char*)"+EXIT socket_send(), successful: %d\n\n",r);

    if( wnc->_callback != NULL )
        wnc->_callback(wnc->_cb_data);

    return r;
}  

/*-------------------------------------------------------------------------
 * Called to receive data.  
 *
 * Input: handle to the socket we want to read from
 *
 * Output: data we receive is placed into the data buffer
 *         size is the size of the buffer
 *
 * Returns: The number of bytes received or -1 if an error occured
 */
int WNC14A2AInterface::socket_recv(void *handle, void *data, unsigned size) 
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;
    size_t done, cnt;
    Timer t;

    debugOutput(_debugUart,(char*)"+ENTER socket_recv(); read up to %d bytes\n",size);

    memset(data,0x00,size);  
    if (!_pwnc || m_active_socket == -1) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return -1;
        }
    else
        m_active_socket = wnc->socket; //just in case sending to a socket that wasn't last used

    t.start();
    do {
        if( !(t.read_ms() % READ_EVERYMS) )
          cnt = done = _pwnc->read(m_active_socket, (uint8_t *)data, (uint32_t) size);
        done = (cnt || (t.read_ms() > WNC14A2A_MISC_TIMEOUT))? 1:0;
        }
    while( !done );
    t.stop();
    
    if( _pwnc->getWncStatus() != WNC_GOOD ) {
        _errors = NSAPI_ERROR_DEVICE_ERROR;
        return -1;
        }

    debugOutput(_debugUart,(char*)"+EXIT socket_recv(), ret=%d\n",cnt);

    if( wnc->_callback != NULL )
        wnc->_callback(wnc->_cb_data);

    return cnt;
}

/*-------------------------------------------------------------------------
 * Close a socket 
 *
 * Input: the handle to the socket to close
 *
 * Output: none
 *
 * Return: -1 on error, otherwise 0
 */
int WNC14A2AInterface::socket_close(void *handle)
{
    WNCSOCKET *wnc = (WNCSOCKET*)handle;
    debugOutput(_debugUart,(char*)"+CALLED socket_close()\n");

    if (!_pwnc || m_active_socket == -1) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return -1;
        }
    else
        m_active_socket = wnc->socket; //just in case sending to a socket that wasn't last used
    
    if( !_pwnc->closeSocket(m_active_socket) ) {
        _errors = NSAPI_ERROR_DEVICE_ERROR;
        return -1;
        }

    wnc->opened = false;     //no longer in use
    wnc->addr = NULL;        //not open
    wnc->proto = NSAPI_TCP;  //assume TCP for now
    wnc->_cb_data = NULL;
    wnc->_callback= NULL;

    _errors = NSAPI_ERROR_OK;
    return 0;
}

/*-------------------------------------------------------------------------
 * return the MAC for this device.  Because there is no MAC Ethernet 
 * address to return, this function returns a bogus MAC address created 
 * from the ICCD on the SIM that is being used.
 *
 * Input: none
 *
 * Output: none
 *
 * Return: MAC string containing "NN:NN:NN:NN:NN:NN" or NULL
 */
const char *WNC14A2AInterface::get_mac_address()
{
    string mac, str;
    debugOutput(_debugUart,(char*)"+ENTER get_mac_address()\n");

    if( _pwnc->getICCID(&str) ) {
        CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), null);
        mac = str.substr(3,20);
        mac[2]=mac[5]=mac[8]=mac[11]=mac[14]=':';
        strncpy(_mac_address, mac.c_str(), mac.length());
        return _mac_address;
    }
    return NULL;
}

/*-------------------------------------------------------------------------
 * return a pointer to the current WNC14A2AInterface
 */
NetworkStack *WNC14A2AInterface::get_stack() {
    debugOutput(_debugUart,(char*)"+CALLED get_stack()\n");
    return this;
}

/*-------------------------------------------------------------------------
 * Disconnnect from the 14A2A, but we can not do that
 * so just return saying everything is ok.
 */
nsapi_error_t WNC14A2AInterface::disconnect() 
{
    debugOutput(_debugUart,(char*)"+CALLED disconnect()\n");
    return NSAPI_ERROR_OK;
}

/*-------------------------------------------------------------------------
 * allow the user to change the APN. The API takes username and password
 * but they are not used.
 *
 * Input: apn string
 *        username - not used
 *        password - not used
 *
 * Output: none
 *
 * Return: nsapi_error_t 
 */
nsapi_error_t WNC14A2AInterface::set_credentials(const char *apn, const char *username, const char *password) 
{
    debugOutput(_debugUart,(char*)"+ENTER set_credentials()\n");
    if( !_pwnc ) 
        return (_errors=NSAPI_ERROR_NO_CONNECTION);
        
    if( !apn )
        return (_errors=NSAPI_ERROR_PARAMETER);

    if( !_pwnc->setApnName(apn) )
        return (_errors=NSAPI_ERROR_DEVICE_ERROR);

    return (_errors=NSAPI_ERROR_OK);
}

/*-------------------------------------------------------------------------
 * check to see if we are currently regisered with the network.
 *
 * Input: none
 *
 * Output: none
 *
 * Return: ture if we are registerd, false if not or an error occured
 */
bool WNC14A2AInterface::registered()
{
    debugOutput(_debugUart,(char*)"+ENTER registered()\n");
    if( !_pwnc ) {
        _errors=NSAPI_ERROR_NO_CONNECTION;
        return false;
        }

    if ( _pwnc->getWncStatus() == WNC_GOOD ){
        _errors=NSAPI_ERROR_OK;
        return true;
        }
    _errors=NSAPI_ERROR_NO_CONNECTION;
    return false;
}

/*-------------------------------------------------------------------------
 * doDebug is just a handy way to allow a developer to set different levels
 * of debug for the WNC14A2A device.
 *
 * Input:  a Bitfield of -
 *   basic debug   = 0x01
 *   more debug    = 0x02
 *   network debug = 0x04
 *   all debug     = 0x07
 *
 * Output: none
 *
 * Returns: void
 */
void WNC14A2AInterface::doDebug( int v )
{
    if( !_pwnc )
        _errors = NSAPI_ERROR_DEVICE_ERROR;
    else
        _pwnc->enableDebug( (v&1), (v&2) );

    m_debug=(v&4);
    debugOutput(_debugUart,(char*)"+SETTING debug flag to 0x%02X\n",v);
}

/*-------------------------------------------------------------------------
 * Simple function to allow for writing debug messages.  It always 
 * checks to see if debug has been enabled or not before it
 * outputs the message.
 *
 * Input: The debug uart pointer followed by format string and vars
 *
 * Output: none
 *
 * Return: void
 */
void WNC14A2AInterface::debugOutput(WNCDebug *dbgOut, char * format, ...) 
{
    if( dbgOut && m_debug ) {
        char buffer[256];
        va_list args;
        va_start (args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        dbgOut->puts(buffer);
        va_end (args);
        }
}


////////////////////////////////////////////////////////////////////
//  SMS methods
///////////////////////////////////////////////////////////////////

/*-------------------------------------------------------------------------
 * IOTSMS message don't use a phone number, they use the device ICCID.  This 
 * function returns the ICCID based number that is used for this device.
 *
 * Input: none
 * Output: none
 * Return: string containing the IOTSMS number to use
 */
char* WNC14A2AInterface::getSMSnbr( void ) 
{
    char * ret=NULL;
    string iccid_str;
    static string msisdn_str;

    if( !_pwnc ) {
        _errors=NSAPI_ERROR_NO_CONNECTION;
        return NULL;
        }

    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), null);

    if( !_pwnc->getICCID(&iccid_str) ) 
        return ret;
 
    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), null);

    if( _pwnc->convertICCIDtoMSISDN(iccid_str, &msisdn_str) )
         ret = (char*)msisdn_str.c_str();    
    return ret;
}


/*-------------------------------------------------------------------------
 * Normally the user attaches his call-back function when performing
 * the listen call which enables the SMS system, but this function
 * allows them to update the callback if desired.
 *
 * input: pointer to the function to call
 * output: none
 * return: none
 */
void WNC14A2AInterface::sms_attach(void (*callback)(IOTSMS *))
{
    debugOutput(_debugUart,(char*)"+CALLED sms_attach() called\n");
    _sms_cb = callback;
}

/*-------------------------------------------------------------------------
 * Call this to start the SMS system.  It is needed to reset the WNC 
 * internal data structures related to SMS messaging
 */
void WNC14A2AInterface::sms_start(void)
{
    _pwnc_mutex.lock();                       //delete any message currently in storage
    _pwnc->deleteSMSTextFromMem('*');       //so we are notified of new incomming messages
    _pwnc_mutex.unlock();
}

/*-------------------------------------------------------------------------
 * Initialize the IoT SMS system.  Initializing it allows the user to set a 
 * polling period to check for SMS messages, and a SMS is recevied, then 
 * a user provided function is called.  
 *
 * Input: polling period in seconds. If not specified 30 seconds is used.
 *        pointer to a users calllback function
 * Output: none
 *
 * Returns: void
 */
void WNC14A2AInterface::sms_listen(uint16_t pp)
{
    debugOutput(_debugUart,(char*)"+CALLED sms_listen(%d) called\n",pp);
    if( !_pwnc ) {
        _errors=NSAPI_ERROR_NO_CONNECTION;
        return;
        }

    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), fail);

    if( m_smsmoning )
        m_smsmoning = false;
    if( pp < 1)
        pp = 30;


    debugOutput(_debugUart,(char*)"+setup event queue\n");
    smsThread.start(callback(&sms_queue,&EventQueue::dispatch_forever));

    _pwnc_mutex.lock();                       //delete any message currently in storage
    _pwnc->deleteSMSTextFromMem('*');       //so we are notified of new incomming messages
    _pwnc_mutex.unlock();
    sms_queue.call_every(pp*1000, mbed::Callback<void()>((WNC14A2AInterface*)this,&WNC14A2AInterface::handle_sms_event));

    m_smsmoning = true;
    debugOutput(_debugUart,(char*)"+EXIT sms_listen()\n");
}

/*-------------------------------------------------------------------------
 * process to check SMS messages that is called at the user specified period
 * 
 * input: none
 * output:none
 * return:void
 */
void WNC14A2AInterface::handle_sms_event()
{
    int msgs_available;
    debugOutput(_debugUart,(char*)"+CALLED handle_sms_event() called\n");

    if ( _sms_cb && m_smsmoning ) {
        CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), fail);
        _pwnc_mutex.lock();
        msgs_available = _pwnc->readUnreadSMSText(&m_smsmsgs, true);
        _pwnc_mutex.unlock();
        if( msgs_available ) {
            debugOutput(_debugUart,(char*)"+Have %d unread texts present\n",m_smsmsgs.msgCount);
            for( int i=0; i< m_smsmsgs.msgCount; i++ ) {
                m_MsgText.number = m_smsmsgs.e[i].number;
                m_MsgText.date = m_smsmsgs.e[i].date;
                m_MsgText.time = m_smsmsgs.e[i].time;
                m_MsgText.msg = m_smsmsgs.e[i].msg;
                _sms_cb(&m_MsgText);
                }
            }
        }
    debugOutput(_debugUart,(char*)"+EXIT handle_sms_event\n");
}


/*-------------------------------------------------------------------------
 * Check for any SMS messages that are present. If there are, then  
 * fetch them and pass to the users call-back function for processing
 *
 * input: pointer to a IOTSMS message buffer array (may be more than 1 msg)
 * output:message buffer pointer is updated
 * return: the number of messages being returned
 */
int WNC14A2AInterface::getSMS(IOTSMS **pmsg) 
{
    int msgs_available;

    debugOutput(_debugUart,(char*)"+CALLED getSMS()\n");
    CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), fail);

    _pwnc_mutex.lock();
    msgs_available = _pwnc->readUnreadSMSText(&m_smsmsgs, true);
    _pwnc_mutex.unlock();

    if( msgs_available ) {
        debugOutput(_debugUart,(char*)"+Have %d unread texts present\n",m_smsmsgs.msgCount);
        for( int i=0; i< m_smsmsgs.msgCount; i++ ) {
            m_MsgText_array[i].number = m_smsmsgs.e[i].number;
            m_MsgText_array[i].date   = m_smsmsgs.e[i].date;
            m_MsgText_array[i].time   = m_smsmsgs.e[i].time;
            m_MsgText_array[i].msg    = m_smsmsgs.e[i].msg;
            pmsg[i] = (IOTSMS*)&m_MsgText_array[i];
            }
        debugOutput(_debugUart,(char*)"+DONE getting messages\n");
        msgs_available = m_smsmsgs.msgCount;
        }
    debugOutput(_debugUart,(char*)"+EXIT getSMS\n");
    return msgs_available;
}


/*-------------------------------------------------------------------------
 * send a message to the specified user number. 
 *
 * input: string containing users number
 *        string with users message
 * ouput: none
 *
 * return: true if no problems occures, false if failed to send
 */
int WNC14A2AInterface::sendIOTSms(const string& number, const string& message) 
{

    debugOutput(_debugUart,(char*)"+CALLED sendIOTSms(%s,%s)\n",number.c_str(), message.c_str());
    _pwnc_mutex.lock();
    int i =  _pwnc->sendSMSText((char*)number.c_str(), message.c_str());
    _pwnc_mutex.unlock();

    return i;
}

//
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//      NetworkStack API's that are not support in the WNC14A2A
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//

int inline WNC14A2AInterface::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address) 
{
    debugOutput(_debugUart,(char*)"+CALLED socket_accept()\n");
    _errors = NSAPI_ERROR_UNSUPPORTED;
    return -1;
}

int inline WNC14A2AInterface::socket_bind(void *handle, const SocketAddress &address) 
{
    debugOutput(_debugUart,(char*)"+CALLED socket_bind()\n");
    _errors = NSAPI_ERROR_UNSUPPORTED;
    return -1;
}


int inline WNC14A2AInterface::socket_listen(void *handle, int backlog)
{
   debugOutput(_debugUart,(char*)"+CALLED socket_listen()\n");
    _errors = NSAPI_ERROR_UNSUPPORTED;
    return -1;
}

////////////////////////////////////////////////////////////////////
//  socket Attach
///////////////////////////////////////////////////////////////////

/*-------------------------------------------------------------------------
 * Register a callback on state change of the socket.FROM NetworkStack
 *  @param handle       Socket handle
 *  @param callback     Function to call on state change
 *  @param data         Argument to pass to callback
 *  @note Callback may be called in an interrupt context.
 */
void WNC14A2AInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;

    debugOutput(_debugUart,(char*)"+CALLED socket_attach()\n");
    wnc->_callback = callback;
    wnc->_cb_data = data;
}

////////////////////////////////////////////////////////////////////
//  UDP methods
///////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
//sends data to a UDP socket
int WNC14A2AInterface::socket_sendto(void *handle, const SocketAddress &address, const void *data, unsigned size)
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;
    
    debugOutput(_debugUart,(char*)"+CALLED socket_sendto()\n");
    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), fail);
    if (!wnc->_wnc_opened) {
       int err = socket_connect(wnc, address);
       if (err < 0) 
           return err;
       }
    wnc->addr = address;

    return socket_send(wnc, data, size);
}

//receives from a UDP socket
// *address is the addres that this data is from
// *buffer is the data that was sent
// size is the numbr of bytes in the buffer
int WNC14A2AInterface::socket_recvfrom(void *handle, SocketAddress *address, void *buffer, unsigned size)
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;
    debugOutput(_debugUart,(char*)"+CALLED socket_recvfrom(%p, %p, %p, %d)\n", handle, address, buffer, size);

    if (!wnc->_wnc_opened) {
       debugOutput(_debugUart,(char*)"+Need to open a WNC socket first.\n");
       int err = socket_connect(wnc, *address);
       if (err < 0) 
           return err;
       }
    debugOutput(_debugUart,(char*)"+SOCKET_RECVFROM using Socket %d, ip=%s, port=%d\n", 
                 wnc->socket, wnc->addr.get_ip_address(), wnc->addr.get_port());

    int ret = socket_recv(wnc, (char *)buffer, size);
    if (ret >= 0 && address) 
        *address = wnc->addr;
    return ret;
}
