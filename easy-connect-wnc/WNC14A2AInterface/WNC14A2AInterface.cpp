/**
* copyright (c) 2017-2018, James Flynn
* SPDX-License-Identifier: Apache-2.0
*/

/* 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
 
/**
*   @file   WNC14A2AInterface.cpp
*   @brief  WNC14A2A implementation of NetworkInterfaceAPI for Mbed OS
*
*   @author James Flynn
*
*   @date   1-Feb-2018
*/

#include "WNC14A2AInterface.h"
#include <Thread.h>
#include "mbed_events.h"

#include <string> 
#include <ctype.h>

#define WNC14A2A_READ_TIMEOUTMS        2000              //read this long total until we decide there is no data to receive in MS
#define WNC14A2A_COMMUNICATION_TIMEOUT 100               //how long (ms) to wait for a WNC14A2A connect response
#define WNC_BUFF_SIZE                  1500              //total number of bytes in a single WNC call
#define UART_BUFF_SIZE                 4000              //size of our internal uart buffer
#define ISR_FREQ                       250               //frequency in ms to run the isr handler

//
// The WNC device does not generate interrutps on received data, so the module must be polled 
// for data availablility.  To implement a non-blocking mode, interrupts are simulated using
// mbed OS Event Queues.  These Constants are used to manage that sequence.
//
#define READ_INIT                      10
#define READ_START                     11
#define READ_ACTIVE                    12
#define DATA_AVAILABLE                 13
#define TX_IDLE                        20
#define TX_STARTING                    21
#define TX_ACTIVE                      22
#define TX_COMPLETE                    23

#if MBED_CONF_APP_WNC_DEBUG == true
#define debugOutput(...)      WNC14A2AInterface::_dbOut(__VA_ARGS__)
#define debugDump_arry(...)   WNC14A2AInterface::_dbDump_arry(__VA_ARGS__)
#else
#define debugOutput(...)      {/* __VA_ARGS__ */}
#define debugDump_arry(...)   {/* __VA_ARGS__ */}
#endif


//
// GPIO Pins used to initialize the WNC parts on the Avnet WNC Shield
//
DigitalOut  mdm_uart2_rx_boot_mode_sel(PTC17);  // on powerup, 0 = boot mode, 1 = normal boot
DigitalOut  mdm_power_on(PTB9);                 // 0 = modem on, 1 = modem off (hold high for >5 seconds to cycle modem)
DigitalOut  mdm_wakeup_in(PTC2);                // 0 = let modem sleep, 1 = keep modem awake -- Note: pulled high on shield
DigitalOut  mdm_reset(PTC12);                   // active high
DigitalOut  shield_3v3_1v8_sig_trans_ena(PTC4); // 0 = disabled (all signals high impedence, 1 = translation active
DigitalOut  mdm_uart1_cts(PTD0);                // WNC doesn't utilize RTS/CTS but the pin is connected

using namespace WncControllerK64F_fk;            // namespace for the AT controller class use

//! associations for the controller class to use. Order of pins is critical.
WncGpioPinListK64F wncPinList = { 
    &mdm_uart2_rx_boot_mode_sel,
    &mdm_power_on,
    &mdm_wakeup_in,
    &mdm_reset,
    &shield_3v3_1v8_sig_trans_ena,
    &mdm_uart1_cts
};

Thread smsThread, isrThread;                          //SMS thread for receiving SMS, recv is for simulated rx-interrupt
static Mutex _pwnc_mutex;                           //because WNC class is not re-entrant

static WNCSOCKET _sockets[WNC14A2A_SOCKET_COUNT];     //WNC supports 8 open sockets but driver only supports 1 currently
BufferedSerial   mdmUart(PTD3,PTD2,UART_BUFF_SIZE,1); //UART for WNC Module

/*   Constructor
*
*  @brief    May be invoked with or without the debug pointer.
*  @note     After the constructor has completed, call check 
*  _errors to determine if any errors occured. Possible values:
*           NSAPI_ERROR_UNSUPPORTED 
*           NSAPI_ERROR_DEVICE_ERROR
*/
WNC14A2AInterface::WNC14A2AInterface(WNCDebug *dbg) : 
 m_wncpoweredup(0),
 _pwnc(NULL),
 m_active_socket(-1),
 m_smsmoning(0)
{
    _errors = NSAPI_ERROR_OK;  //tracks internal driver errors only
    m_debug=0;                 //for internal driver debug

    if( _pwnc ) {              //only a single instance allowed
        _errors =  NSAPI_ERROR_UNSUPPORTED;
        return;
        }
    memset(_mac_address,0x00,sizeof(_mac_address));
    for( int i=0; i<WNC14A2A_SOCKET_COUNT; i++ ) {
        _sockets[i].socket = i;
        _sockets[i].addr = NULL;
        _sockets[i].opened=false;
        _sockets[i]._wnc_opened=false;
        _sockets[i].proto=NSAPI_TCP;
        }

    _debugUart = dbg;           
    if( dbg != NULL ) 
        _pwnc = new WncControllerK64F(&wncPinList, &mdmUart, dbg);
    else 
        _pwnc = new WncControllerK64F_fk::WncControllerK64F(&wncPinList, &mdmUart, NULL);
        
    if( !_pwnc ) {
        debugOutput("FAILED to open WncControllerK64!");
        _errors = NSAPI_ERROR_DEVICE_ERROR;
        }

    isrThread.start(callback(&isr_queue,&EventQueue::dispatch_forever));
}

//! Standard destructor
WNC14A2AInterface::~WNC14A2AInterface()
{
    delete _pwnc;  //free the existing WncControllerK64F object
}


nsapi_error_t WNC14A2AInterface::connect()   //can be called with no arguments or with arguments
{
    debugOutput("ENTER connect(void)");
    return connect(NULL,NULL,NULL);
}

nsapi_error_t WNC14A2AInterface::connect(const char *apn, const char *username, const char *password) 
{
    debugOutput("ENTER connect(apn,user,pass)");
    if( !_pwnc )
        return (_errors=NSAPI_ERROR_NO_CONNECTION);

    if (!apn)
        apn = "m2m.com.attz";

    _pwnc_mutex.lock();
    if (!m_wncpoweredup) {
        debugOutput("call powerWncOn(%s,40)",apn);
        m_wncpoweredup=_pwnc->powerWncOn(apn,40);
        _errors = m_wncpoweredup? 1:0;
        }
    else {          //powerWncOn already called, set a new APN
        debugOutput("set APN=%s",apn);
        _errors = _pwnc->setApnName(apn)? 1:0;
        }

    _errors |= _pwnc->getWncNetworkingStats(&myNetStats)? 2:0;
    _pwnc_mutex.unlock();

    debugOutput("EXIT connect (%02X)",_errors);
    return (!_errors)? NSAPI_ERROR_NO_CONNECTION : NSAPI_ERROR_OK;
}

const char *WNC14A2AInterface::get_ip_address()
{
    const char *ptr=NULL; 

    _pwnc_mutex.lock();
    if ( _pwnc->getWncNetworkingStats(&myNetStats) ) {
        _pwnc_mutex.unlock();
        CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), null);
        ptr = &myNetStats.ip[0];
    }
    _pwnc_mutex.unlock();
    _errors=NSAPI_ERROR_NO_CONNECTION;
    return ptr;
}

int WNC14A2AInterface::socket_open(void **handle, nsapi_protocol_t proto) 
{
    int i;
    debugOutput("ENTER socket_open()");

    // search through the available sockets (WNC can only support a max amount).
    for( i=0; i<WNC14A2A_SOCKET_COUNT; i++ )
        if( !_sockets[i].opened )
            break;

    if( i == WNC14A2A_SOCKET_COUNT ) {
        _errors=NSAPI_ERROR_NO_SOCKET;
        return -1;
        }

    m_active_socket = i;               //this is the active socket
    _sockets[i].socket = i;            //also save for later
    _sockets[i].url="";
    _sockets[i].opened = true;         
    _sockets[i]._wnc_opened=false;
    _sockets[i].addr = NULL;           //not yet open
    _sockets[i].proto = proto;         //don't know if it is TCP/UDP
    _sockets[i]._callback = NULL;
    _sockets[i]._cb_data = NULL;         
    *handle = &_sockets[i];

    m_recv_wnc_state = READ_START;
    m_tx_wnc_state = TX_IDLE;

    debugOutput("EXIT socket_open; Socket=%d, OPEN=%s, protocol =%s",
                i, _sockets[i].opened?"YES":"NO", (_sockets[i].proto==NSAPI_UDP)?"UDP":"TCP");
    
    _errors = NSAPI_ERROR_OK;
    return i;
}

int WNC14A2AInterface::socket_connect(void *handle, const SocketAddress &address) 
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;   
    int rval = 0;

    debugOutput("ENTER socket_connect(); IP=%s; PORT=%d;", address.get_ip_address(), address.get_port());
    
    if (!_pwnc || m_active_socket == -1 || !wnc->opened ) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return -1;
        }

    m_active_socket = wnc->socket;  //in case the user is asking for a different socket
    wnc->addr = address;
                                
    //
    //try connecting to URL if possible, if no url, try IP address
    //

    _pwnc_mutex.lock();
    if( wnc->url.empty() ) {
        if( !_pwnc->openSocketIpAddr(m_active_socket, address.get_ip_address(), address.get_port(), 
                                       (wnc->proto==NSAPI_UDP)?0:1, WNC14A2A_COMMUNICATION_TIMEOUT) ) 
            rval = -1;
        }
     else {
        if( !_pwnc->openSocketUrl(m_active_socket, wnc->url.c_str(), wnc->addr.get_port(), (wnc->proto==NSAPI_UDP)?0:1) ) 
            rval = -1;
        }
    _pwnc_mutex.unlock();
    if( !rval ) {
        wnc->_wnc_opened = true;
        debugOutput("EXIT socket_connect()");
        }

    m_recv_wnc_state = READ_START;
    m_tx_wnc_state = TX_IDLE;

    if( wnc->_callback != NULL ) 
        wnc->_callback( wnc->_cb_data );

    return rval;
}

nsapi_error_t WNC14A2AInterface::gethostbyname(const char* name, SocketAddress *address, nsapi_version_t version)
{
    nsapi_error_t ret = NSAPI_ERROR_OK;
    char ipAddrStr[25];
    int  t_socket = 0;  //use a temporary socket place holder

    debugOutput("ENTER gethostbyname(); IP=%s; PORT=%d; URL=%s;", address->get_ip_address(), address->get_port(), name);
    memset(ipAddrStr,0x00,sizeof(ipAddrStr));
    
    if (!_pwnc) 
        return (_errors = NSAPI_ERROR_NO_SOCKET);
        
    if (m_active_socket != -1)      //we might have been called before a socket was opened
        t_socket = m_active_socket; //if so, do nothing with the active socket index

    //Execute DNS query.  
    _pwnc_mutex.lock();
    if( !_pwnc->resolveUrl(t_socket, name) )  
        ret = _errors = NSAPI_ERROR_DEVICE_ERROR;

    //Get IP address that the URL was resolved to
    if( !_pwnc->getIpAddr(t_socket, ipAddrStr) )
        ret = _errors = NSAPI_ERROR_DEVICE_ERROR;
    _pwnc_mutex.unlock();

    if( ret != NSAPI_ERROR_OK )
        return ret;

    address->set_ip_address(ipAddrStr);

    if( t_socket == m_active_socket ) {
        _sockets[m_active_socket].url=name;
        _sockets[m_active_socket].addr.set_ip_address(ipAddrStr);
        }

    debugOutput("EXIT gethostbyname()");
    return (_errors = ret);
}
 

int WNC14A2AInterface::socket_close(void *handle)
{
    WNCSOCKET *wnc = (WNCSOCKET*)handle;
    int rval = 0;

    debugOutput("ENTER socket_close()");

    if (!_pwnc || m_active_socket == -1) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return -1;
        }
    else
        m_active_socket = wnc->socket; //just in case sending to a socket that wasn't last used
    
    m_tx_wnc_state = TX_IDLE;               //reset TX state
    if( m_recv_wnc_state != READ_START ) {  //reset RX state
        m_recv_events = 0;  //force a timeout
        while( m_recv_wnc_state !=  DATA_AVAILABLE ) 
            wait(1);  //someone called close while a read was happening
        }

    _pwnc_mutex.lock();
    if( !_pwnc->closeSocket(m_active_socket) ) {
        _errors = NSAPI_ERROR_DEVICE_ERROR;
        rval = -1;
        }
    _pwnc_mutex.unlock();

    if( !rval ) {
        wnc->opened   = false;       //no longer in use
        wnc->addr     = NULL;        //not open
        wnc->proto    = NSAPI_TCP;   //assume TCP for now
        _errors       = NSAPI_ERROR_OK;
        wnc->_cb_data = NULL;
        wnc->_callback= NULL;
        }

    debugOutput("EXIT socket_close()");
    return rval;
}

const char *WNC14A2AInterface::get_mac_address()
{
    string mac, str;
    debugOutput("ENTER get_mac_address()");

    _pwnc_mutex.lock();
    if( _pwnc->getICCID(&str) ) {
        _pwnc_mutex.unlock();
        CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), null);
        mac = str.substr(3,20);
        mac[2]=mac[5]=mac[8]=mac[11]=mac[14]=':';
        strncpy(_mac_address, mac.c_str(), mac.length());
        debugOutput("EXIT get_mac_address() - %s",_mac_address);
        return _mac_address;
    }
    _pwnc_mutex.unlock();
    debugOutput("EXIT get_mac_address() - NULL");
    return NULL;
}

NetworkStack *WNC14A2AInterface::get_stack() {
    debugOutput("ENTER/EXIT get_stack()");
    return this;
}

nsapi_error_t WNC14A2AInterface::disconnect() 
{
    debugOutput("ENTER/EXIT disconnect()");
    return NSAPI_ERROR_OK;
}

nsapi_error_t WNC14A2AInterface::set_credentials(const char *apn, const char *username, const char *password) 
{

    _errors=NSAPI_ERROR_OK;
    debugOutput("ENTER set_credentials()");
    if( !_pwnc ) 
        return (_errors=NSAPI_ERROR_NO_CONNECTION);
        
    if( !apn )
        return (_errors=NSAPI_ERROR_PARAMETER);

    _pwnc_mutex.lock();
    if( !_pwnc->setApnName(apn) )
        _errors=NSAPI_ERROR_DEVICE_ERROR;
    _pwnc_mutex.unlock();
    debugOutput("EXIT set_credentials()");
    return _errors;
}

bool WNC14A2AInterface::registered()
{
    debugOutput("ENTER registered()");
    _errors=NSAPI_ERROR_OK;

    if( !_pwnc ) {
        _errors=NSAPI_ERROR_NO_CONNECTION;
        return false;
        }

    _pwnc_mutex.lock();
    if ( _pwnc->getWncStatus() != WNC_GOOD )
        _errors=NSAPI_ERROR_NO_CONNECTION;
    _pwnc_mutex.unlock();

    debugOutput("EXIT registered()");
    return (_errors==NSAPI_ERROR_OK);
}

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

    _pwnc_mutex.lock();
    if( !_pwnc->getICCID(&iccid_str) ) {
        _pwnc_mutex.unlock();
        return ret;
        }
    _pwnc_mutex.unlock();
 
    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), null);

    _pwnc_mutex.lock();
    if( _pwnc->convertICCIDtoMSISDN(iccid_str, &msisdn_str) )
         ret = (char*)msisdn_str.c_str();    
    _pwnc_mutex.unlock();
    return ret;
}

void WNC14A2AInterface::sms_attach(void (*callback)(IOTSMS *))
{
    debugOutput("ENTER/EXIT sms_attach()");
    _sms_cb = callback;
}

void WNC14A2AInterface::sms_start(void)
{
    _pwnc_mutex.lock();                     
    _pwnc->deleteSMSTextFromMem('*');       
    _pwnc_mutex.unlock();
}

void WNC14A2AInterface::sms_listen(uint16_t pp)
{
    debugOutput("ENTER sms_listen(%d)",pp);
    if( !_pwnc ) {
        _errors=NSAPI_ERROR_NO_CONNECTION;
        return;
        }

    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), fail);

    if( m_smsmoning )
        m_smsmoning = false;
    if( pp < 1)
        pp = 30;


    debugOutput("setup sms_listen event queue");
    smsThread.start(callback(&sms_queue,&EventQueue::dispatch_forever));

    sms_start();
    sms_queue.call_every(pp*1000, mbed::Callback<void()>((WNC14A2AInterface*)this,&WNC14A2AInterface::handle_sms_event));

    m_smsmoning = true;
    debugOutput("EXIT sms_listen()");
}

void WNC14A2AInterface::handle_sms_event()
{
    int msgs_available;
    debugOutput("ENTER handle_sms_event()");

    if ( _sms_cb && m_smsmoning ) {
        CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), fail);
        _pwnc_mutex.lock();
        msgs_available = _pwnc->readUnreadSMSText(&m_smsmsgs, true);
        _pwnc_mutex.unlock();
        if( msgs_available ) {
            debugOutput("Have %d unread texts present",m_smsmsgs.msgCount);
            for( int i=0; i< m_smsmsgs.msgCount; i++ ) {
                m_MsgText.number = m_smsmsgs.e[i].number;
                m_MsgText.date = m_smsmsgs.e[i].date;
                m_MsgText.time = m_smsmsgs.e[i].time;
                m_MsgText.msg = m_smsmsgs.e[i].msg;
                _sms_cb(&m_MsgText);
                }
            }
        }
    debugOutput("EXIT handle_sms_event");
}


int WNC14A2AInterface::getSMS(IOTSMS **pmsg) 
{
    int msgs_available;

    debugOutput("ENTER getSMS()");
    CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), fail);

    _pwnc_mutex.lock();
    msgs_available = _pwnc->readUnreadSMSText(&m_smsmsgs, true);
    _pwnc_mutex.unlock();

    if( msgs_available ) {
        debugOutput("Have %d unread texts present",m_smsmsgs.msgCount);
        for( int i=0; i< m_smsmsgs.msgCount; i++ ) {
            m_MsgText_array[i].number = m_smsmsgs.e[i].number;
            m_MsgText_array[i].date   = m_smsmsgs.e[i].date;
            m_MsgText_array[i].time   = m_smsmsgs.e[i].time;
            m_MsgText_array[i].msg    = m_smsmsgs.e[i].msg;
            pmsg[i] = (IOTSMS*)&m_MsgText_array[i];
            }
        msgs_available = m_smsmsgs.msgCount;
        }
    debugOutput("EXIT getSMS");
    return msgs_available;
}


int WNC14A2AInterface::sendIOTSms(const string& number, const string& message) 
{

    debugOutput("ENTER sendIOTSms(%s,%s)",number.c_str(), message.c_str());
    _pwnc_mutex.lock();
    int i =  _pwnc->sendSMSText((char*)number.c_str(), message.c_str());
    _pwnc_mutex.unlock();

    debugOutput("EXIT sendIOTSms(%s,%s)",number.c_str(), message.c_str());
    return i;
}


void WNC14A2AInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;

    debugOutput("ENTER/EXIT socket_attach()");
    wnc->_callback = callback;
    wnc->_cb_data = data;
}

int WNC14A2AInterface::socket_sendto(void *handle, const SocketAddress &address, const void *data, unsigned size)
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;
    
    debugOutput("ENTER socket_sendto()");
    
    CHK_WNCFE(( _pwnc->getWncStatus() == FATAL_FLAG ), fail);
   
    if (!wnc->_wnc_opened) {
       int err = socket_connect(wnc, address);
       if (err < 0) 
           return err;
       }
    wnc->addr = address;

    debugOutput("EXIT socket_sendto()");
    return socket_send(wnc, data, size);
}

int WNC14A2AInterface::socket_recvfrom(void *handle, SocketAddress *address, void *buffer, unsigned size)
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;
    debugOutput("ENTER socket_recvfrom()");

    if (!wnc->_wnc_opened) {
       debugOutput("need to open a WNC socket first");
       int err = socket_connect(wnc, *address);
       if (err < 0) 
           return err;
       }

    int ret = socket_recv(wnc, (char *)buffer, size);
    if (ret >= 0 && address) 
        *address = wnc->addr;
    debugOutput("EXIT socket_recvfrom()");
    return ret;
}


int inline WNC14A2AInterface::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address) 
{
    debugOutput("ENTER/EXIT socket_accept()");
    _errors = NSAPI_ERROR_UNSUPPORTED;
    return -1;
}

int inline WNC14A2AInterface::socket_bind(void *handle, const SocketAddress &address) 
{
    debugOutput("ENTER/EXIT socket_bind()");
    _errors = NSAPI_ERROR_UNSUPPORTED;
    return -1;
}


int inline WNC14A2AInterface::socket_listen(void *handle, int backlog)
{
   debugOutput("ENTER/EXIT socket_listen()");
    _errors = NSAPI_ERROR_UNSUPPORTED;
    return -1;
}

void WNC14A2AInterface::doDebug( int v )
{
    #if MBED_CONF_APP_WNC_DEBUG == true
    if( !_pwnc )
        _errors = NSAPI_ERROR_DEVICE_ERROR;
    else {
        _pwnc_mutex.lock();
        _pwnc->enableDebug( (v&1), (v&2) );
        _pwnc_mutex.unlock();
        }

    m_debug= v;

    debugOutput("SET debug flag to 0x%02X",v);
    #endif
}

/** function to dump a user provided array.
*
* @author James Flynn
* @param  data    pointer to the data array to dump
* @param  size    number of bytes to dump
* @return void
* @date 1-Feb-2018
*/
void WNC14A2AInterface::_dbDump_arry( const uint8_t* data, unsigned int size )
{
    #if MBED_CONF_APP_WNC_DEBUG == true
    char buffer[256];
    unsigned int i, k;

    if( _debugUart != NULL && (m_debug & 0x08) ) {
        for (i=0; i<size; i+=16) {
            sprintf(buffer,"[WNC Driver]:0x%04X: ",i);
            _debugUart->puts(buffer);
            for (k=0; k<16; k++) {
                sprintf(buffer, "%02X ", data[i+k]);
                _debugUart->puts(buffer);
                }
            _debugUart->puts(" -- ");
            for (k=0; k<16; k++) {
                sprintf(buffer, "%2c", isprint(data[i+k])? data[i+k]:'.');
                _debugUart->puts(buffer);
                }
            _debugUart->puts("\n\r");
            }
        }
    #endif
}

void WNC14A2AInterface::_dbOut(const char *format, ...) 
{
    #if MBED_CONF_APP_WNC_DEBUG == true
    char buffer[256];

    sprintf(buffer,"[WNC Driver]: ");
    if( _debugUart != NULL && (m_debug & 0x0c) ) {
        va_list args;
        va_start (args, format);
        _debugUart->puts(buffer);
        vsnprintf(buffer, sizeof(buffer), format, args);
        _debugUart->puts(buffer);
        _debugUart->putc('\n');
        va_end (args);
        }
    #endif
}

int WNC14A2AInterface::socket_recv(void *handle, void *data, unsigned size) 
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;

    debugOutput("ENTER socket_recv(), request %d bytes",size);

    if (!_pwnc || m_active_socket == -1) 
        return (_errors = NSAPI_ERROR_NO_SOCKET);

    if( size < 1 || data == NULL )  // should never happen
        return 0; 

    switch( m_recv_wnc_state ) {
        case READ_START:  //need to start a read sequence of events
            m_recv_wnc_state = READ_INIT;
            m_recv_socket   = wnc->socket; //just in case sending to a socket that wasn't last used
            m_recv_dptr     = (uint8_t*)data;
            m_recv_orig_size= size;
            m_recv_total_cnt= 0;
            m_recv_timer    = 0;
            m_recv_events   = 1;
            m_recv_req_size = (uint32_t)size;
            m_recv_return_cnt=0;

            if( m_recv_req_size > WNC_BUFF_SIZE) {
                m_recv_events  =  ((uint32_t)size/WNC_BUFF_SIZE);
                m_recv_req_size= WNC_BUFF_SIZE;
                }
            m_recv_callback = wnc->_callback;
            m_recv_cb_data  = wnc->_cb_data;

            if( !rx_event() ){
                m_recv_wnc_state = READ_ACTIVE;
                isr_queue.call_in(ISR_FREQ,mbed::Callback<void()>((WNC14A2AInterface*)this,&WNC14A2AInterface::wnc_isr_event));
                return NSAPI_ERROR_WOULD_BLOCK;
                }
            //was able to get the requested data in a single transaction so fall thru and finish
            //no need to schedule the background task

        case DATA_AVAILABLE:
            debugOutput("EXIT socket_recv(), return %d bytes",m_recv_return_cnt);
            debugDump_arry((const uint8_t*)data,m_recv_return_cnt);
            m_recv_wnc_state = READ_START;
            return m_recv_return_cnt;

        case READ_INIT:
        case READ_ACTIVE:
            return NSAPI_ERROR_WOULD_BLOCK;

        default:
            debugOutput("EXIT socket_recv(), NSAPI_ERROR_DEVICE_ERROR");
            return (_errors = NSAPI_ERROR_DEVICE_ERROR);
        }
}


int WNC14A2AInterface::socket_send(void *handle, const void *data, unsigned size) 
{
    WNCSOCKET *wnc = (WNCSOCKET *)handle;

    debugOutput("ENTER socket_send() send %d bytes",size);

    if (!_pwnc || m_active_socket == -1) {
        _errors = NSAPI_ERROR_NO_SOCKET;
        return 0;
        }
    else
        m_active_socket = wnc->socket; //just in case sending to a socket that wasn't last used

    if( size < 1 || data == NULL )  // should never happen
        return 0; 

    switch( m_tx_wnc_state ) {
        case TX_IDLE:
            m_tx_wnc_state = TX_STARTING;
            debugDump_arry((const uint8_t*)data,size);
            m_tx_socket    = wnc->socket; //just in case sending to a socket that wasn't last used
            m_tx_dptr      = (uint8_t*)data;
            m_tx_orig_size = size;
            m_tx_req_size  = (uint32_t)size;
            m_tx_total_sent= 0;
            m_tx_callback  = wnc->_callback;
            m_tx_cb_data   = wnc->_cb_data;

            if( m_tx_req_size > UART_BUFF_SIZE ) 
                m_tx_req_size= UART_BUFF_SIZE;

            if( !tx_event() ) {   //if we didn't sent all the data at once, continue in background
                m_tx_wnc_state = TX_ACTIVE;
                isr_queue.call_in(ISR_FREQ,mbed::Callback<void()>((WNC14A2AInterface*)this,&WNC14A2AInterface::wnc_isr_event));
                return NSAPI_ERROR_WOULD_BLOCK;
                }
            //all data sent so fall through to TX_COMPLETE

        case TX_COMPLETE:
            debugOutput("EXIT socket_send(), sent %d bytes", m_tx_total_sent);
            m_tx_wnc_state = TX_IDLE;
            return m_tx_total_sent;

        case TX_ACTIVE:
        case TX_STARTING:
            return NSAPI_ERROR_WOULD_BLOCK;

        default:
            debugOutput("EXIT socket_send(), NSAPI_ERROR_DEVICE_ERROR");
            return (_errors = NSAPI_ERROR_DEVICE_ERROR);
        }
}


void WNC14A2AInterface::wnc_isr_event()
{
    int done = 1;

    debugOutput("ENTER wnc_isr_event()");

    if( m_recv_wnc_state == READ_ACTIVE ) 
        done &= rx_event();
    if( m_tx_wnc_state == TX_ACTIVE )
        done &= tx_event();

    if( !done ) 
        isr_queue.call_in(ISR_FREQ,mbed::Callback<void()>((WNC14A2AInterface*)this,&WNC14A2AInterface::wnc_isr_event));

    debugOutput("EXIT wnc_isr_event()");
}


int WNC14A2AInterface::tx_event()
{
    debugOutput("ENTER tx_event()");

    _pwnc_mutex.lock();
    if( _pwnc->write(m_tx_socket, m_tx_dptr, m_tx_req_size) ) 
        m_tx_total_sent += m_tx_req_size;
    else
        debugOutput("tx_event WNC failed to send()");
    CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), resume);
    _pwnc_mutex.unlock();
    
    if( m_tx_total_sent < m_tx_orig_size ) {
        m_tx_dptr += m_tx_req_size;
        m_tx_req_size = m_tx_orig_size-m_tx_total_sent;

        if( m_tx_req_size > UART_BUFF_SIZE) 
            m_tx_req_size= UART_BUFF_SIZE;

        debugOutput("EXIT tx_event(), need to send %d more bytes.",m_tx_req_size);
        return 0;
        }
    debugOutput("EXIT tx_event, data sent");
    m_tx_wnc_state = TX_COMPLETE;
    if( m_tx_callback != NULL ) 
        m_tx_callback( m_tx_cb_data );
    m_tx_cb_data = NULL; 
    m_tx_callback = NULL;

    return 1;
}

int WNC14A2AInterface::rx_event()
{
    uint32_t k;

    debugOutput("ENTER rx_event()");
    _pwnc_mutex.lock();
    int cnt = _pwnc->read(m_recv_socket, m_recv_dptr,  m_recv_req_size);
    CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), resume);
    _pwnc_mutex.unlock();
    if( cnt ) {
        m_recv_dptr += cnt;
        m_recv_total_cnt += cnt;
        m_recv_req_size = m_recv_orig_size-m_recv_total_cnt;
        if( m_recv_req_size > WNC_BUFF_SIZE )
            m_recv_req_size = WNC_BUFF_SIZE;
        --m_recv_events;
        m_recv_timer = 0;  //restart the timer
        }
    else if( ++m_recv_timer > (WNC14A2A_READ_TIMEOUTMS/ISR_FREQ) ) {
        //didn't get all requested data and we timed out waiting
        CHK_WNCFE((_pwnc->getWncStatus()==FATAL_FLAG), resume);
        debugOutput("EXIT rx_event(), TIME-OUT!");
        k = m_recv_return_cnt = m_recv_total_cnt;
        m_recv_wnc_state = DATA_AVAILABLE;
        if( m_recv_callback != NULL ) 
            m_recv_callback( m_recv_cb_data );
        m_recv_cb_data = NULL; 
        m_recv_callback = NULL;
        m_recv_return_cnt = k;
        return 1;
        }

    if( m_recv_events > 0 ) {
        debugOutput("EXIT rx_event() but sechedule for more data.");
        return 0;
        }
    else if( m_recv_total_cnt >= m_recv_orig_size ){
        k = m_recv_return_cnt = m_recv_total_cnt;  //save because the callback func may call RX again on us
        debugOutput("EXIT rx_event(), data available.");
        m_recv_wnc_state = DATA_AVAILABLE;  
        if( m_recv_callback != NULL ) 
            m_recv_callback( m_recv_cb_data );
        m_recv_cb_data = NULL;
        m_recv_callback = NULL;
        m_recv_return_cnt = k;
        return 1;
        }
    else{
        debugOutput("EXIT rx_event() but sechedule for more data.");
        return 0;
        }
     
}

