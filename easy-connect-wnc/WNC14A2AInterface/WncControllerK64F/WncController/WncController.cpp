/*
    Copyright (c) 2016 Fred Kellerman
 
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
 
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
 
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
    
    @file          WncController.cpp
    @purpose       Controls WNC 14A2A Cellular Modem
    @version       1.0
    @date          July 2016
    @author        Fred Kellerman
    
    
    An Example of usage:
    
    WncControllerK64F mdm(&wncPinList, &mdmUart, &debugUart);

    mdm.enableDebug(true, true);

    if (false == mdm.powerWncOn("m2m.com.attz", 60)) {
        while(1);
    }

    // ICCID and MSISDN
    string iccid; string msisdn;
    if (mdm.getICCID(&iccid) == true) {
        if (mdm.convertICCIDtoMSISDN(iccid, &msisdn) == true) {
           // Send an SMS message (must use 15-digit MISDN number!)
           mdm.sendSMSText(msisdn.c_str(), "Hello from WNC Kit -> from WNC");
        }
    }

    // Get an IP address setup for the socket #1 (0 indexed))
    if (true == mdm.resolveUrl(0, "www.att.com"))
    {
        // Report server IP
        if (true == mdm.getIpAddr(0, ipAddrStr)) {
            debugUart.puts("Server IP: ");
            debugUart.puts(ipAddrStr);
            debugUart.puts("\r\n");
        }

        // Open Socket #1, TCP=true resolved IP on port 80:
        if (true == mdm.openSocket(0, 80, true)) {
            // Write some data
            const uint8_t * dataStr = "GET /index.html HTTP/1.0\r\nFrom: someuser@someuser.com\r\nUser-Agent: HTTPTool/1.0\r\n\r\n";
            if (true == mdm.write(0, dataStr, strlen((const char *)dataStr)))
            {
                const uint8_t * myBuf;
                mdm.setReadRetries(0, 20);
                uint32_t n = mdm.read(0, &myBuf);
                if (n > 0)
                    debugUart.printf("Read %d chars: %s\r\n", n, myBuf);
                else
                    debugUart.puts("No read data!\r\n");
            }
        }
    }
    
*/


#include <cstdlib>
#include <cctype>
#include <string.h>
#include "WncController.h"

namespace WncController_fk {

/////////////////////////////////////////////////////
// Static initializers
/////////////////////////////////////////////////////
WncController::WncSocketInfo_s WncController::m_sSock[MAX_NUM_WNC_SOCKETS];
const WncController::WncSocketInfo_s WncController::defaultSockStruct = { 0, false, "192.168.0.1", 80, 0, 25, true, 30 };

WncController::WncState_e WncController::m_sState = WNC_OFF;
uint16_t WncController::m_sCmdTimeoutMs = WNC_CMD_TIMEOUT_MS;
string WncController::m_sApnStr = "NULL";
string WncController::m_sWncStr;
uint8_t WncController::m_sPowerUpTimeoutSecs = MAX_POWERUP_TIMEOUT;
bool WncController::m_sDebugEnabled = false;
bool WncController::m_sMoreDebugEnabled = false;
bool WncController::m_sCheckNetStatus = false;   // Turn on internet status check between every command
const char * const WncController::INVALID_IP_STR = "";
bool WncController::m_sReadyForSMS = false;


/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 */
static char* itoa(int64_t value, char* result, int base)
{
    // check that the base is valid
    if ( base < 2 || base > 36 ) {
        *result = '\0';
        return result;
    }

    char* ptr = result, *ptr1 = result, tmp_char;
    int64_t tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if ( tmp_value < 0 )
        *ptr++ = '-';

    *ptr-- = '\0';

    while ( ptr1 < ptr ) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return result;
}

const char * WncController::_to_string(int64_t value)
{
    static char str[21];  // room for signed 64-bit + null
    itoa(value, str, 10);
    return (str);
}

const char * WncController::_to_hex_string(uint8_t value)
{
    static char str[3];   // room for 8-bit + null
    itoa(value, str, 16);
    return (str);
}

WncController::WncController(void)
{
    for(unsigned i=0; i<MAX_NUM_WNC_SOCKETS; i++)
        m_sSock[i] = defaultSockStruct;
}

WncController::~WncController(void) {};

void WncController::enableDebug(bool on, bool moreDebugOn)
{
    m_sDebugEnabled = on;
    m_sMoreDebugEnabled = moreDebugOn;
}

WncController::WncState_e WncController::getWncStatus(void)
{
    return (m_sState);
}

int16_t WncController::getDbmRssi(void)
{
    int16_t rssi, ber;
    if (at_getrssiber_wnc(&rssi, &ber) == true)
        return (rssi);
    else
        return (99);
}

int16_t WncController::get3gBer(void)
{
    int16_t rssi, ber;
    if (at_getrssiber_wnc(&rssi, &ber) == true)
        return (ber);
    else
        return (99);
}

bool WncController::powerWncOn(const char * const apn, uint8_t powerUpTimeoutSecs)
{
    dbgPuts("Waiting for WNC to Initialize...");
    m_sPowerUpTimeoutSecs = powerUpTimeoutSecs;
    m_sState = WNC_ON_NO_CELL_LINK;  // Turn soft on to allow "AT" for init to be sent!
    if (initWncModem(powerUpTimeoutSecs) == true) {
        // Set the Apn
        setApnName(apn);
        if (false == softwareInitMdm()) {
            dbgPuts("Software init failed!");
            m_sState = WNC_OFF;
        }
    }
    else {
        dbgPuts("Power up failed!");
        m_sState = WNC_OFF;
    }
    
    return ((m_sState == WNC_ON) || (m_sState == WNC_ON_NO_CELL_LINK));
}

size_t WncController::sendCustomCmd(const char * cmd, char * resp, size_t sizeRespBuf, int ms_timeout)
{
    string * respStr;
    
    if (sizeRespBuf > 0) {
        at_send_wnc_cmd(cmd, &respStr, ms_timeout);
        strncpy(resp, respStr->c_str(), sizeRespBuf);
        if (respStr->size() > sizeRespBuf)
            dbgPuts("sendCustomCmd truncated!");
            
        return (respStr->size());
    }
    
    dbgPuts("sendCustomCmd: would have overrun!");
    
    return (0);
}

bool WncController::pingUrl(const char * url)
{
    string ipAddr;
    
    if (true == at_dnsresolve_wnc(url, &ipAddr))
        return (pingIp(ipAddr.c_str()));
    else
        dbgPuts("pingUrl DNS resolve: failed!");
        
    return (false);
}

bool WncController::pingIp(const char * ip)
{
    if (true == at_ping_wnc(ip))
        return (true);
    else
        dbgPuts("pingIp: failed!");
    
    return (false);
}

bool WncController::getWncNetworkingStats(WncIpStats * s)
{
    return (at_get_wnc_net_stats(s));
}

bool WncController::getIpAddr(uint16_t numSock, char myIpAddr[MAX_LEN_IP_STR])
{
    if (numSock < MAX_NUM_WNC_SOCKETS) {
        strncpy(myIpAddr, m_sSock[numSock].myIpAddressStr.c_str(), MAX_LEN_IP_STR);
        myIpAddr[MAX_LEN_IP_STR - 1] = '\0';
        return (true);
    }
    else {
        myIpAddr[0] = '\0';
        return (false);
    }
}

bool WncController::setApnName(const char * const apnStr)
{
    if (at_setapn_wnc(apnStr) == true)
    {
        m_sApnStr = apnStr;
        return (true);
    }
    else
        return (false);
}

bool WncController::resolveUrl(uint16_t numSock, const char * url)
{
    bool cmdRes;
    
    if (numSock < MAX_NUM_WNC_SOCKETS) {
        if (strlen(url) > 0) {
            cmdRes = at_dnsresolve_wnc(url, &m_sSock[numSock].myIpAddressStr);
            if (cmdRes == false)
                dbgPuts("Cannot resolve URL!");
            return (cmdRes);
        }
        else
            dbgPuts("Invalid URL");
    }
    else
        dbgPuts("Invalid Sock num!");

    return (false);
}

bool WncController::setIpAddr(uint16_t numSock, const char * ipStr)
{
    if (numSock < MAX_NUM_WNC_SOCKETS) {
        m_sSock[numSock].myIpAddressStr = ipStr;
        return (true);
    }
    else {
        dbgPuts("Bad socket num!");
        return (false);
    }
}

void WncController::setWncCmdTimeout(uint16_t toMs)
{
    m_sCmdTimeoutMs = toMs;
}

bool WncController::openSocketUrl(uint16_t numSock, const char * url, uint16_t port, bool tcp, uint16_t timeOutSec)
{
    if (resolveUrl(numSock, url) == true)
        return (openSocket(numSock, port, tcp, timeOutSec));
    
    return (false);
}

bool WncController::openSocketIpAddr(uint16_t numSock, const char * ipAddr, uint16_t port, bool tcp, uint16_t timeOutSec)
{
    if (setIpAddr(numSock, ipAddr) == true)
        return (openSocket(numSock, port, tcp, timeOutSec));
    
    return (false);
}
 
bool WncController::openSocket(uint16_t numSock, uint16_t port, bool tcp, uint16_t timeOutSec)
{
    if (numSock < MAX_NUM_WNC_SOCKETS) {
        // IPV4 ip addr sanity check!
        size_t lenIpStr = m_sSock[numSock].myIpAddressStr.size();
        if (lenIpStr < 7 || lenIpStr > 15) {
            dbgPuts("Invalid IP Address!");
            return (false);
        }
        
        // Already open ? Must close if want to re-open with new settings.
        if (m_sSock[numSock].open == true) {
            dbgPuts("Socket already open, close then re-open!");
            if (true == at_sockclose_wnc(m_sSock[numSock].numWncSock))
                m_sSock[numSock].open = false;
            else
                return (false);
        }
        
        m_sSock[numSock].myPort = port;
        m_sSock[numSock].isTcp = tcp;
        m_sSock[numSock].timeOutSec = timeOutSec;
        
        int16_t numWncSock = at_sockopen_wnc(m_sSock[numSock].myIpAddressStr.c_str(), port, numSock, tcp, timeOutSec);
        m_sSock[numSock].numWncSock = numWncSock;
        if (numWncSock > 0 && numWncSock <= (uint16_t)MAX_NUM_WNC_SOCKETS)
            m_sSock[numSock].open = true;
        else {
            m_sSock[numSock].open = false;
            dbgPuts("Socket open fail!!!!");
            
            // If the modem is not responding don't bother it.
            if (WNC_NO_RESPONSE != getWncStatus()) {
                // Work-around.  If the sock open fails it needs to be told
                // to close.  If 6 sock opens happen with a fail, it further
                // crashes the WNC.  Not sure why the sock won't open.
                at_sockclose_wnc(m_sSock[numSock].numWncSock);
            }
        }
    }
    else {
        dbgPuts("Bad socket num or IP!");
        return (false);
    }

    return (m_sSock[numSock].open);
}

bool WncController::sockWrite(const uint8_t * const s, uint16_t n, uint16_t numSock, bool isTcp)
{
    bool result = true;
    
    AtCmdErr_e cmdRes = at_sockwrite_wnc(s, n, m_sSock[numSock].numWncSock, isTcp);
    if (cmdRes != WNC_AT_CMD_OK) {
        if ((cmdRes == WNC_AT_CMD_ERREXT) || (cmdRes == WNC_AT_CMD_ERRCME))
        {
            // This may throw away any data that hasn't been written out of the WNC
            //  but at this point with the way the WNC currently works we have
            //  no choice.
            closeOpenSocket(numSock);
        }
        result = false;
    }
    
    return (result);
}

bool WncController::write(uint16_t numSock, const uint8_t * s, uint32_t n)
{
    bool result;
    
    if (numSock < MAX_NUM_WNC_SOCKETS) {
        if (m_sSock[numSock].open == true) {
            if (n <= MAX_WNC_WRITE_BYTES) {
                result = sockWrite(s, n, numSock, m_sSock[numSock].isTcp);
            }
            else {
                uint16_t rem = n % MAX_WNC_WRITE_BYTES;
                while (n >= MAX_WNC_WRITE_BYTES) {
                    n -= MAX_WNC_WRITE_BYTES;
                    result = sockWrite(s, MAX_WNC_WRITE_BYTES, numSock, m_sSock[numSock].isTcp);
                    if (result == false) {
                        n = 0;
                        rem = 0;
                        dbgPuts("Sock write fail!");
                    }
                    else
                        s += MAX_WNC_WRITE_BYTES;
                }
                if (rem > 0)
                    result = sockWrite(s, rem, numSock, m_sSock[numSock].isTcp);                
            }
        }
        else {
            dbgPuts("Socket is closed for write!");
            result = false;
        }
    }
    else {
        dbgPuts("Bad socket num!");
        result = false;
    }

    return (result);
}

size_t WncController::read(uint16_t numSock, const uint8_t ** readBuf)
{
    static string theBuf;
    string readStr;
    
    theBuf.erase();  // Clean-up from last time

    if (numSock < MAX_NUM_WNC_SOCKETS) {
        if (m_sSock[numSock].open == true) {
            uint8_t   i = m_sSock[numSock].readRetries;
            uint16_t to = m_sSock[numSock].readRetryWaitMs;
            bool foundData = false;
            do {
                AtCmdErr_e cmdRes;
                cmdRes = at_sockread_wnc(&readStr, m_sSock[numSock].numWncSock, m_sSock[numSock].isTcp);
                if (WNC_AT_CMD_OK == cmdRes) {
                    // This will let this loop read until the socket data is
                    //  empty.  If no data, then wait the retry amount of time.
                    if (readStr.size() > 0) {
                        theBuf += readStr;
                        foundData = true;
                        i = 1;
                    }
                    else {
                        // Once data is found start returning it asap
                        if (foundData == false)
                            waitMs(to);
                    }
                }
                else {
                    theBuf += readStr; // Append what if any we got before it errored.
                    dbgPuts("Sockread failed!");
                    if (WNC_NO_RESPONSE == getWncStatus()) {
                        i = 0;
                    }
                    else if ((cmdRes == WNC_AT_CMD_ERREXT) || (cmdRes == WNC_AT_CMD_ERRCME))
                    {
                        // This may throw away any data that hasn't been read out of the WNC
                        //  but at this point with the way the WNC currently works we have
                        //  no choice.
                        closeOpenSocket(numSock);
                        i = 0;
                    }
                    else
                        waitMs(to);
                }
            } while (i-- > 0);
        }
        else {
            dbgPuts("Socket is closed for read");
        }
    }
    else {
        dbgPuts("Bad socket num!");
    }

    *readBuf = (const uint8_t *)theBuf.c_str();

    return (theBuf.size());
}

size_t WncController::read(uint16_t numSock, uint8_t * readBuf, uint32_t maxReadBufLen)
{
    uint32_t numCopied = 0;
    
    if (numSock < MAX_NUM_WNC_SOCKETS) {
        if (m_sSock[numSock].open == true) {
            uint8_t   i = m_sSock[numSock].readRetries;
            uint16_t to = m_sSock[numSock].readRetryWaitMs;
            bool foundData = false;
            uint16_t numRead;
            do {
                AtCmdErr_e cmdRes;
                if (maxReadBufLen < MAX_WNC_READ_BYTES)
                    cmdRes = at_sockread_wnc(readBuf, &numRead, maxReadBufLen, m_sSock[numSock].numWncSock, m_sSock[numSock].isTcp);
                else
                    cmdRes = at_sockread_wnc(readBuf, &numRead, MAX_WNC_READ_BYTES, m_sSock[numSock].numWncSock, m_sSock[numSock].isTcp);

                if (WNC_AT_CMD_OK == cmdRes) {
                    // This will let this loop read until the socket data is
                    //  empty.  If no data, then wait the retry amount of time.
                    if (numRead > 0) {
                        foundData = true;
                        i = 1;
                        if (numRead <= maxReadBufLen) {
                            maxReadBufLen -= numRead;
                            numCopied     += numRead;
                            readBuf       += numRead;
                        }
                        else {
                            i = 0; // No more room for data!
                            dbgPutsNoTime("No more room for read data!");
                        } 
                    }
                    else {
                        // Once data is found start returning it asap
                        if (foundData == false)
                            waitMs(to);
                    }
                }
                else {
                    dbgPuts("Sockread failed!");
                    if (WNC_NO_RESPONSE == getWncStatus()) {
                        i = 0;
                    }
                    else if ((cmdRes == WNC_AT_CMD_ERREXT) || (cmdRes == WNC_AT_CMD_ERRCME))
                    {
                        // This may throw away any data that hasn't been read out of the WNC
                        //  but at this point with the way the WNC currently works we have
                        //  no choice.
                        closeOpenSocket(numSock);
                        i = 0;
                    }
                    else
                        waitMs(to);
                }
            } while ((i-- > 0) && (maxReadBufLen > 0));
        }
        else {
            dbgPuts("Socket is closed for read");
        }
    }
    else {
        dbgPuts("Bad socket num!");
    }

    return (numCopied);
}

void WncController::setReadRetries(uint16_t numSock, uint16_t retries)
{
    if (numSock < MAX_NUM_WNC_SOCKETS)
        m_sSock[numSock].readRetries = retries;
    else
        dbgPuts("Bad socket num!");
}

void WncController::setReadRetryWait(uint16_t numSock, uint16_t readRetryWaitMs)
{
    if (numSock < MAX_NUM_WNC_SOCKETS)
        m_sSock[numSock].readRetryWaitMs = readRetryWaitMs;
    else
        dbgPuts("Bad socket num!");
}

bool WncController::closeSocket(uint16_t numSock)
{
    if (numSock < MAX_NUM_WNC_SOCKETS) {

        if (false == at_sockclose_wnc(m_sSock[numSock].numWncSock))
            dbgPuts("Sock close may not have closed!");
                
        // Even with an error the socket could have closed,
        //  can't tell for sure so just soft close it for now.
        m_sSock[numSock].open = false;
    }
    else {
        dbgPuts("Bad socket num!");
    }

    return (m_sSock[numSock].open == false);
}

size_t WncController::mdmGetline(string * buff, int timeout_ms)
{
    char chin = '\0';
    char chin_last;
    size_t len = 0;

    startTimerB();
    while ((len <= MAX_LEN_WNC_CMD_RESPONSE) && (getTimerTicksB_mS() < timeout_ms)) {
        if (charReady()) {
            chin_last = chin;
            chin = getc();
            if (isprint(chin)) {
                *buff += chin;
                len++;  // Bound the copy length to something reaonable just in case
                continue;
            }
            else if ((('\r' == chin_last) && ('\n' == chin)) || (('\n' == chin_last) && ('\r' == chin)))  {
                break;
            }
        }
    }
    stopTimerB();
    
    if (len > MAX_LEN_WNC_CMD_RESPONSE)
        dbgPuts("Max cmd length reply exceeded!");

    return (len);
}

bool WncController::softwareInitMdm(void)
{
  static bool reportStatus = true;
  unsigned i;
  
  if (checkCellLink() == true) {
      if (reportStatus == false) {
          dbgPuts("Re-connected to cellular network!");
          reportStatus = true;
      }
      
      // WNC has SIM and registered on network so 
      //  soft initialize the WNC.
      for (i = 0; i < WNC_SOFT_INIT_RETRY_COUNT; i++)
          if (at_init_wnc() == true)
              break;
                  
      // If it did not respond try a hardware init
      if (i == WNC_SOFT_INIT_RETRY_COUNT)
      {
          at_reinitialize_mdm();
          return (at_init_wnc(true));  // Hard reset occurred so make it go through the software init();
      }
      else
          return (true);
  }
  else
  {
      if (reportStatus == true) {
           dbgPuts("Not connected to cellular network!");
           reportStatus = false;
      }
      return (false);
  }
}

WncController::AtCmdErr_e WncController::sendWncCmd(const char * const s, string ** r, int ms_timeout)
{
    if (checkCellLink() == false) {
        static string noRespStr;

        // Save some run-time!
        if (m_sDebugEnabled)
        {
            dbgPuts("FAIL send cmd: ", false);
            if (m_sMoreDebugEnabled && m_sDebugEnabled) {
                dbgPutsNoTime(s);
            }
            else {
                size_t n = strlen(s);
                if (n <= WNC_TRUNC_DEBUG_LENGTH) {
                    dbgPutsNoTime(s);
                }
                else {
                    string truncStr(s,WNC_TRUNC_DEBUG_LENGTH/2);
                    truncStr += "..";
                    truncStr += &s[n-(WNC_TRUNC_DEBUG_LENGTH/2)];
                    dbgPutsNoTime(truncStr.c_str());
                }
            }    
        }
        
        noRespStr.erase();
        *r = &noRespStr;

        return (WNC_AT_CMD_NO_CELL_LINK);
    }
    
    if (m_sCheckNetStatus)
    {
        if (m_sMoreDebugEnabled)
            dbgPuts("[---------- Network Status -------------");
        string * pRespStr;
        at_send_wnc_cmd("AT@SOCKDIAL?", &pRespStr, m_sCmdTimeoutMs);
        if (m_sMoreDebugEnabled)
           dbgPuts("---------------------------------------]");
    }
    
    // If WNC ready, send user command
    return (at_send_wnc_cmd(s, r, ms_timeout));
}

WncController::AtCmdErr_e WncController::at_send_wnc_cmd(const char * s, string ** r, int ms_timeout)
{
    // Save some run-time!
    if (m_sDebugEnabled)
    {
        if (m_sMoreDebugEnabled) {
           dbgPuts("TX: ", false); dbgPutsNoTime(s);
        }
        else {
            if (m_sDebugEnabled) {  // Save some run-time!
                size_t n = strlen(s);
                if (n <= WNC_TRUNC_DEBUG_LENGTH) {
                    dbgPuts("TX: ", false); dbgPutsNoTime(s);
                }
                else {
                    string truncStr(s,WNC_TRUNC_DEBUG_LENGTH/2);
                    truncStr += "..";
                    truncStr += &s[n - (WNC_TRUNC_DEBUG_LENGTH/2)];
                    dbgPuts("TX: ", false); dbgPutsNoTime(truncStr.c_str());
                }
            }
        }
    }

    AtCmdErr_e atResult = mdmSendAtCmdRsp(s, ms_timeout, &m_sWncStr);
    *r = &m_sWncStr;   // Return a pointer to the static string
      
    if (atResult != WNC_AT_CMD_TIMEOUT) {
        // If a prior command timed out but a new one works then
        //  change the state back to ON.  We don't know here in this 
        //  method if the Cell Link is good so assume it is. When a command
        //  that depends on the cell link is made it will update the state.
        if (m_sState == WNC_NO_RESPONSE)
            m_sState = WNC_ON;
            
        // Save some run-time!
        if (m_sDebugEnabled)
        {        
            dbgPuts("RX: ", false);
            if (m_sMoreDebugEnabled) {
                dbgPutsNoTime(m_sWncStr.c_str());
            }
            else {
                if (m_sWncStr.size() <= WNC_TRUNC_DEBUG_LENGTH) {
                    dbgPutsNoTime(m_sWncStr.c_str());
                }
                else {
                    string truncStr = m_sWncStr.substr(0,WNC_TRUNC_DEBUG_LENGTH/2) + "..";
                    truncStr += m_sWncStr.substr(m_sWncStr.size() - (WNC_TRUNC_DEBUG_LENGTH/2), WNC_TRUNC_DEBUG_LENGTH/2);
                    dbgPutsNoTime(truncStr.c_str());
                }
            }
        }
    }
    else {
        m_sState = WNC_NO_RESPONSE;
        dbgPuts("AT Cmd TIMEOUT!");
        dbgPuts("RX: ", false); dbgPutsNoTime(m_sWncStr.c_str());
    }
    
    return (atResult);
}

void WncController::closeOpenSocket(uint16_t numSock)
{
    // Try to open and close the socket
    do {
        dbgPuts("Try to close and re-open socket");
        if (false == at_sockclose_wnc(m_sSock[numSock].numWncSock)) {
            if (WNC_NO_RESPONSE == getWncStatus()) {
                dbgPuts("No response for closeOpenSocket1");
                return ;
            }
        }        

        int numWncSock = at_sockopen_wnc(m_sSock[numSock].myIpAddressStr.c_str(), m_sSock[numSock].myPort, numSock, m_sSock[numSock].isTcp, m_sSock[numSock].timeOutSec);
        m_sSock[numSock].numWncSock = numWncSock;
        if (numWncSock > 0 && numWncSock <= (int)MAX_NUM_WNC_SOCKETS)
            m_sSock[numSock].open = true;
        else {
            m_sSock[numSock].open = false;
            dbgPuts("Failed to re-open socket!");
        }
        
        if (WNC_NO_RESPONSE == getWncStatus()) {
            dbgPuts("No response for closeOpenSocket2");
            return ;
        }
    } while (m_sSock[numSock].open == false);
}

bool WncController::getICCID(string * iccid)
{
    if (at_geticcid_wnc(iccid) == false) {
        dbgPuts("getICCID error!");
        return (false);
    }
    
    return (true);
}

bool WncController::at_geticcid_wnc(string * iccid)
{
    string * respStr;
    
    iccid->erase();
    
    AtCmdErr_e r = at_send_wnc_cmd("AT%CCID", &respStr, m_sCmdTimeoutMs);

    if (r != WNC_AT_CMD_OK || respStr->size() == 0)
        return (false);

    // New Firmware versions respond to the %CCID command with "%CCID:"
    // but old version respond with "AT%CCID", so check to see which we have
    size_t pos = respStr->find(":");
    if (pos == string::npos) 
        pos = respStr->find("AT%CCID");
    else 
        pos = respStr->find("%CCID");

    if (pos == string::npos)
        return (false);

    pos += 7; // Advanced to the number

    size_t posOK = respStr->rfind("OK");
    if (posOK == string::npos)
        return (false);

    *iccid = respStr->substr(pos, posOK - pos);
    
    return (true);
}

bool WncController::convertICCIDtoMSISDN(const string & iccid, string * msisdn)
{
    msisdn->erase();
    
    if (iccid.size() != 20 && iccid.size() != 19) {
        dbgPuts("Invalid ICCID length!");
        return (false);
    }
 
    *msisdn = "882350";
    
    if (iccid.size() == 20)
        *msisdn += iccid.substr(10,iccid.size() - 11);
    else
        *msisdn += iccid.substr(10,iccid.size() - 10);
    
    return (true);
}

bool WncController::sendSMSText(const char * const phoneNum, const char * const text)
{
    if (at_sendSMStext_wnc(phoneNum, text) == true)
        return (true);
    else {
        dbgPuts("sendSMSText: Failed!");
        return (false);
    }
}

bool WncController::readSMSLog(struct WncSmsList * log)
{
    string * logStr;
    uint16_t i;
    
    if (at_readSMSlog_wnc(&logStr) == false) {
        dbgPuts("readSMSLog: Failed!");
        return (false);
    }
    
    // Clean slate    
    log->msgCount = 0;

    if (logStr->size() == 0)
        return (false);

    // Pick out the stuff from the string and convert to struct
    string s;
    size_t pos2;
    size_t pos = logStr->find("+CMGL:");
        
    for(i=0; i<MAX_WNC_SMS_MSG_SLOTS; i++) {
        // Start with a clean slate, let parsing fill out later.
        log->e[i].unread = false;
        log->e[i].incoming = false;
        log->e[i].unsent = false;
        log->e[i].pduMode = false;
        log->e[i].msgReceipt = false;

        log->e[i].idx = logStr->at(pos + 7);
        if (pos == string::npos)
            return (false);
        pos2 = logStr->find(",\"", pos);
        if (pos2 == string::npos) {
            // If the WNC acts wrong and receives a PDU mode
            //  SMS there will not be any quotes in the response,
            //  just take the whole reply and make it the message body for
            //  now, mark it as an unread message, set the pdu flag!
            log->e[log->msgCount].unread = true;
            log->e[log->msgCount].pduMode = true;
            log->msgCount++;

            pos2 = logStr->find("+CMGL", pos + 5);
            if (pos2 == string::npos) {
                pos2 = logStr->find("OK", pos + 5);
                if (pos2 == string::npos) {
                    dbgPuts("Strange SMS Log Ending!");
                    return (false);
                }
                i = MAX_WNC_SMS_MSG_SLOTS;
            }
            log->e[log->msgCount].msg = logStr->substr(0, pos2 - pos);
            pos = pos2;  // for loop starts off expecting pos to point to next log msg
            continue;
        }
        pos += 2;  // Advance to the text we want
        pos2 = logStr->find("\",", pos);
        if ((pos2 == string::npos) || (pos >= pos2))
            return (false);
                    
        // Setup attributes
        s = logStr->substr(pos, pos2 - pos);
        if (s.find("REC READ") != string::npos)
            log->e[i].incoming = true;
        if (s.find("REC UNREAD") != string::npos) {
            log->e[i].unread = true;
            log->e[i].incoming = true;
        }
        if (s.find("STO UNSENT") != string::npos)
            log->e[i].unsent = true;
        if (logStr->find(",,") == string::npos)
            log->e[i].msgReceipt = true;
            
        // Tele number
        pos2 = logStr->find(",\"", pos2);
        if (pos2 == string::npos)
            return (false);  
        pos2 += 2;  // Advance to next field
        pos = logStr->find("\",", pos2);
        if ((pos == string::npos) || (pos2 > pos))
            return (false);
        if (pos == pos2)
            log->e[i].number.erase();
        else    
            log->e[i].number = logStr->substr(pos2, pos - pos2);
        
        // Date
        pos = logStr->find(",\"", pos);
        if (pos == string::npos)
            return (false);
        pos += 2; // Beginning of date field
        pos2 = logStr->find(",", pos); // End of timestamp field
        if ((pos2 == string::npos) || (pos > pos2))
            return (false);
        if (pos == pos2)
            log->e[i].date.erase();
        else
            log->e[i].date = logStr->substr(pos, pos2 - pos);

        // Timestamp
        pos = logStr->find("\",", pos2); // End of timestamp
        if (pos == string::npos)
            return (false);
        pos2 += 1; // Beginning of time field
        if (pos < pos2)
            return (false);
        if (pos == pos2)
            log->e[i].time.erase();
        else
            log->e[i].time = logStr->substr(pos2, pos - pos2);
        
        // Message field
        
        // We don't know how many messages we have so the next search
        // could end with +CMGL or OK.
        pos += 2;  // Advanced to message text
        pos2 = logStr->find("+CMGL", pos);
        if (pos2 == string::npos) {
            pos2 = logStr->find("OK", pos);
            if (pos2 == string::npos) {
                dbgPuts("Strange SMS Log Ending!");
                return (false);
            }
            i = MAX_WNC_SMS_MSG_SLOTS; // break
        }
        if (pos > pos2)
            return (false);
        if (pos == pos2)
            log->e[log->msgCount].msg.erase();
        else
            log->e[log->msgCount].msg = logStr->substr(pos, pos2 - pos);

        log->msgCount++;  // Message complete
    }    
    
    return (true);
}

bool WncController::readUnreadSMSText(struct WncSmsList * w, bool deleteRead)
{
    struct WncController::WncSmsList tmp;
    
    if (readSMSLog(&tmp) == false)
        return (false);
    
    w->msgCount = 0;
    for(uint16_t i = 0; i < tmp.msgCount; i++) {
        if (tmp.e[i].unread == true) {
            w->e[w->msgCount] = tmp.e[i];
            w->msgCount++;
            if (deleteRead == true) {
                // Clean up message that was copied out and read
                deleteSMSTextFromMem(w->e[i].idx);
            }
        }
    }
    
    return (w->msgCount > 0);
}

size_t WncController::getSignalQuality(const char ** log)
{
    size_t n;

    n = at_getSignalQuality_wnc(log);
    if (n == 0)
        dbgPuts("readSMSText: Failed!");
        
    return (n);
}

size_t WncController::at_getSignalQuality_wnc(const char ** log)
{
    string * pRespStr;
    static string logStr;
    
    logStr.erase();

    if (at_send_wnc_cmd("AT%MEAS=\"0\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr = *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=0: failed!");
        
    if (at_send_wnc_cmd("AT%MEAS=\"1\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=1: failed!");

    if (at_send_wnc_cmd("AT%MEAS=\"2\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=2: failed!");

    if (at_send_wnc_cmd("AT%MEAS=\"3\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=3: failed!");

    if (at_send_wnc_cmd("AT%MEAS=\"4\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=4: failed!");

    if (at_send_wnc_cmd("AT%MEAS=\"5\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=5: failed!");
        
    if (at_send_wnc_cmd("AT%MEAS=\"8\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=8: failed!");
        
    if (at_send_wnc_cmd("AT%MEAS=\"98\"", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        logStr += *pRespStr;
        logStr += "\r\n";
    }
    else
        dbgPuts("AT%MEAS=98: failed!");

    *log = logStr.c_str();
    
    return (logStr.size());
}

bool WncController::getTimeDate(struct WncDateTime * tod)
{
    if (at_gettimedate_wnc(tod) == true)
        return (true);
    else {
        dbgPuts("Get time date failed!");
        return (false);
    }
}

bool WncController::at_ping_wnc(const char * ip)
{
    string * pRespStr;
    string cmdStr = "AT@PINGREQ=\"";
    cmdStr += ip;
    cmdStr += "\"";
    return (at_send_wnc_cmd(cmdStr.c_str(), &pRespStr, WNC_PING_CMD_TIMEOUT_MS) == WNC_AT_CMD_OK);
}

bool WncController::at_gettimedate_wnc(struct WncDateTime * tod)
{
    string * pRespStr;
    char * pEnd;

    if (at_send_wnc_cmd("AT+CCLK?", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK) {
        if (pRespStr->size() > 0) {
            size_t pos1 = pRespStr->find("+CCLK:");
            if (pos1 != string::npos) {
                pEnd = (char *)pRespStr->c_str() + pos1 + 8;
                tod->year  = strtol(pEnd, &pEnd, 10);
                tod->month = strtol(pEnd+1, &pEnd, 10);
                tod->day   = strtol(pEnd+1, &pEnd, 10);
                tod->hour  = strtol(pEnd+1, &pEnd, 10);
                tod->min   = strtol(pEnd+1, &pEnd, 10);
                tod->sec   = strtol(pEnd+1, &pEnd, 10);
                return (true);
            }
        }
    }

    return (false);
}

bool WncController::at_get_wnc_net_stats(WncIpStats * s)
{
    string * pRespStr;
    AtCmdErr_e cmdRes = at_send_wnc_cmd("AT+CGCONTRDP=1", &pRespStr, m_sCmdTimeoutMs);
    
    if (WNC_AT_CMD_OK == cmdRes) {
        if (pRespStr->size() > 0) {
            memset((void*)s, '\0', sizeof(*s));  // Clean-up
            string ss;
            size_t pe;
            size_t ps = pRespStr->rfind("\"");
            if (ps != string::npos) {
                ps += 2;  // Skip the , after the "
                pe = ps;

                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;

                ss = pRespStr->substr(ps, pe - 1 - ps);
                strncpy(s->ip, ss.c_str(), MAX_LEN_IP_STR);
                s->ip[MAX_LEN_IP_STR - 1] = '\0';
                ps = pe;

                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(",", pe);

                ss = pRespStr->substr(ps, pe - ps);
                strncpy(s->mask, ss.c_str(), MAX_LEN_IP_STR);
                s->mask[MAX_LEN_IP_STR - 1] = '\0';
                ps = pe + 1;

                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(",", pe);

                ss = pRespStr->substr(ps, pe - ps);
                strncpy(s->gateway, ss.c_str(), MAX_LEN_IP_STR);
                s->gateway[MAX_LEN_IP_STR - 1] = '\0';
                ps = pe + 1;

                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(",", pe);


                ss = pRespStr->substr(ps, pe - ps);
                strncpy(s->dnsPrimary, ss.c_str(), MAX_LEN_IP_STR);
                s->dnsPrimary[MAX_LEN_IP_STR - 1] = '\0';
                ps = pe + 1;

                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(".", pe);
                if (pe == string::npos)
                    return (false);
                else
                    pe += 1;
                pe = pRespStr->find(",", pe);


                ss = pRespStr->substr(ps, pe - ps);
                strncpy(s->dnsSecondary, ss.c_str(), MAX_LEN_IP_STR);
                s->dnsSecondary[MAX_LEN_IP_STR - 1] = '\0';
                
                dbgPuts("~~~~~~~~~~ WNC IP Stats ~~~~~~~~~~~~");
                dbgPuts("ip: ", false);      dbgPutsNoTime(s->ip);
                dbgPuts("mask: ", false);    dbgPutsNoTime(s->mask);
                dbgPuts("gateway: ", false); dbgPutsNoTime(s->gateway);
                dbgPuts("dns pri: ", false); dbgPutsNoTime(s->dnsPrimary);
                dbgPuts("dns sec: ", false); dbgPutsNoTime(s->dnsSecondary);
                dbgPuts("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
            
                return (true);
            }
        }
    }

    return (false);
}

bool WncController::deleteSMSTextFromMem(char msgIdx)
{
    const char * err = "deleteSMSTextFromMem: Failed!";
    
    switch (msgIdx)
    {
        case '*':
            at_deleteSMSTextFromMem_wnc('1');
            at_deleteSMSTextFromMem_wnc('2');
            at_deleteSMSTextFromMem_wnc('3');
            return (true); // WNC may error if slot empty, just ignore!

        case '1':
        case '2':
        case '3':
            if (true == at_deleteSMSTextFromMem_wnc(msgIdx))
                return (true);
            else {
                dbgPuts(err);
                return (false);
            }

        default:
            dbgPuts(err);
            return (false);
    }
}

bool WncController::sendSMSTextFromMem(char msgIdx)
{
    const char * err = "deleteSMSTextFromMem: Failed!";
    
    switch (msgIdx)
    {
        case '*':
            at_sendSMStextMem_wnc('1');
            at_sendSMStextMem_wnc('2');
            at_sendSMStextMem_wnc('3');
            return (true); // WNC may error if slot is empty, just ignore!

        case '1':
        case '2':
        case '3':
            if (at_sendSMStextMem_wnc(msgIdx) == true)
                return (true);
            else {
                dbgPuts(err);
                return (false);
            }

        default:
            dbgPuts(err);
            return (false);
    }
}

bool WncController::at_deleteSMSTextFromMem_wnc(char n)
{
    string cmdStr, respStr;
    // Message is stored in WNC, now send it!
    cmdStr = "AT+CMGD=";
    cmdStr += n;
    cmdStr += "\r\n";
    dbgPuts("TX: ", false); dbgPutsNoTime(cmdStr.c_str(), false);
    AtCmdErr_e r = mdmSendAtCmdRsp(cmdStr.c_str(), m_sCmdTimeoutMs, &respStr);
    dbgPuts("RX: ", false); dbgPutsNoTime(respStr.c_str());
    return (r == WNC_AT_CMD_OK);
}

bool WncController::at_sendSMStextMem_wnc(char n)
{
    string cmdStr, respStr;
    // Message is stored in WNC, now send it!
    cmdStr = "AT+CMSS=";
    cmdStr += n;
    cmdStr += "\r\n";
    dbgPuts("TX: ", false); dbgPutsNoTime(cmdStr.c_str(), false);
    AtCmdErr_e r = mdmSendAtCmdRsp(cmdStr.c_str(), m_sCmdTimeoutMs, &respStr);
    dbgPuts("RX: ", false); dbgPutsNoTime(respStr.c_str());
    return (r == WNC_AT_CMD_OK);
}    

bool WncController::at_sendSMStext_wnc(const char * const phoneNum, const char * const text)
{
    string respStr;
    string * pRespStr;
    size_t l = strlen(text);
    
    if (l <= MAX_WNC_SMS_LENGTH)
    {
        // Check to see if the SMS service is available
        checkCellLink();
        if (m_sReadyForSMS == true) {
            at_send_wnc_cmd("AT+CMGF=1", &pRespStr, m_sCmdTimeoutMs);
            string cmdStr("AT+CMGS=\"");
            cmdStr += phoneNum;
            cmdStr += "\"";
            dbgPuts("TX: ", false); dbgPutsNoTime(cmdStr.c_str());
            cmdStr += "\x0d"; // x0d = <ENTER>
            // Send raw command with short timeout (the timeout will fail cause the WNC is not supposed to reply yet!
            // And we want a delay before sending the actual text part of the string!
            mdmSendAtCmdRsp(cmdStr.c_str(), 300, &respStr, false);  //  False turns off auto-addition of CR+LF (the WNC wants nothing here)
            dbgPuts("RX: ", false); dbgPutsNoTime(respStr.c_str());
            if ((respStr.size() > 0) && (respStr.find("ERROR") == string::npos)) {
                // Part 2 of the text, this is the actual text part:
                cmdStr = text;
                dbgPuts("TX: ", false); dbgPutsNoTime(cmdStr.c_str());
                cmdStr += "\x1A";  // <CTRL>-Z is what tells the WNC the message is complete to send!
                AtCmdErr_e r = mdmSendAtCmdRsp(cmdStr.c_str(), 10000, &respStr);
                dbgPuts("RX: ", false); dbgPutsNoTime(respStr.c_str());
                if (respStr.size() == 0)
                    return (false);
                else
                    return (r == WNC_AT_CMD_OK);
            }
        }
    }

    return (false);
}

bool WncController::saveSMSText(const char * const phoneNum, const char * const text, char * msgIdx)
{
    if (at_saveSMStext_wnc(phoneNum, text, msgIdx) == true)
        return (true);
    else {
        dbgPuts("saveSMSTextToMem: failed!\r\n");
        return (false);
    }
}

bool WncController::at_saveSMStext_wnc(const char * const phoneNum, const char * const text, char * msgIdx)
{
    string respStr;
    size_t l = strlen(text);
    
    if (l <= MAX_WNC_SMS_LENGTH)
    {
        // Check to see if the SMS service is available
        checkCellLink();
        if (m_sReadyForSMS == true) {
            string cmdStr("AT+CMGW=\"");
            cmdStr += phoneNum;
            cmdStr += "\"";
            dbgPuts("TX: ", false); dbgPutsNoTime(cmdStr.c_str());
            cmdStr += "\x0d"; // x0d = <ENTER>
            // Send raw command with short timeout (the timeout will fail cause the WNC is not supposed to reply yet!
            // And we want a delay before sending the actual text part of the string!
            mdmSendAtCmdRsp(cmdStr.c_str(), 300, &respStr, false);  //  False turns off auto-addition of CR+LF (the WNC wants nothing here)
            dbgPuts("RX: ", false); dbgPutsNoTime(respStr.c_str());
            if ((respStr.size() > 0) && (respStr.find("ERROR") == string::npos)) {
                // Part 2 of the text, this is the actual text part:
                cmdStr = text;
                dbgPuts("TX: ", false); dbgPutsNoTime(cmdStr.c_str());
                cmdStr += "\x1A";  // <CTRL>-Z is what tells the WNC the message is complete to save!
                mdmSendAtCmdRsp(cmdStr.c_str(), 10000, &respStr);
                dbgPuts("RX: ", false); dbgPutsNoTime(respStr.c_str());
                if (respStr.size() > 0) {
                    // respStr will have the SMS index
                    size_t pos1 = respStr.find("+CMGW: ");
                    size_t pos2 = respStr.rfind("OK");
                    if (pos1 != string::npos && pos2 != string::npos) {
                        *msgIdx = *string(respStr.substr(pos1+7, 1)).c_str();
                        return (true);
                    }
                    else {
                        *msgIdx = '!';
                    }
                }
            }
        }
    }
        
    return (false);
}

bool WncController::at_readSMSlog_wnc(string ** log)
{
    return (at_send_wnc_cmd("AT+CMGL", log, m_sCmdTimeoutMs) == WNC_AT_CMD_OK);
}

size_t WncController::at_readSMStext_wnc(const char n, const char ** log)
{
    static string smsReadTxtStr;
    string * pRespStr;
    string cmdStr;
        
    smsReadTxtStr.erase();
    cmdStr = "AT+CMGR";
    cmdStr += '1';
    if (at_send_wnc_cmd("AT+CMGR", &pRespStr, m_sCmdTimeoutMs) == WNC_AT_CMD_OK)
        *log = pRespStr->c_str();
    else
        *log = "\0";
        
    return (pRespStr->size());
}

bool WncController::at_at_wnc(void)
{
    string * pRespStr;
    return (WNC_AT_CMD_OK == at_send_wnc_cmd("AT", &pRespStr, WNC_QUICK_CMD_TIMEOUT_MS)); // Heartbeat?
}

bool WncController::at_init_wnc(bool hardReset)
{
  string * pRespStr;
  AtCmdErr_e cmdRes;
  
  if (hardReset == true)
      dbgPuts("Hard Soft Reset!");
  
  dbgPuts("Start AT init of WNC:");
  
  // Kick it twice to perhaps remove cued responses from an incomplete
  //  power cycle.
  at_send_wnc_cmd("AT", &pRespStr, WNC_QUICK_CMD_TIMEOUT_MS);
  at_send_wnc_cmd("AT", &pRespStr, WNC_QUICK_CMD_TIMEOUT_MS);
  
  // Dump the firmware revision on the debug log:
  at_send_wnc_cmd("AT+GMR", &pRespStr, m_sCmdTimeoutMs);

  // Quick commands below do not need to check cellular connectivity
  at_send_wnc_cmd("ATE0", &pRespStr, WNC_QUICK_CMD_TIMEOUT_MS);  // Echo Off
  at_send_wnc_cmd("AT+CMEE=2", &pRespStr, m_sCmdTimeoutMs);      // 2 - verbose error, 1 - numeric error, 0 - just ERROR

  // Setup 3 memory slots in the WNC SIM for SMS usage.
  at_send_wnc_cmd("AT+CMGF=1", &pRespStr, m_sCmdTimeoutMs);
  at_send_wnc_cmd("AT+CPMS=\"SM\",\"SM\",\"SM\"", &pRespStr, m_sCmdTimeoutMs);

  cmdRes = at_send_wnc_cmd("AT", &pRespStr, WNC_QUICK_CMD_TIMEOUT_MS);     // Heartbeat?
  
  // If the simple commands are not working, no chance of more complex.
  //  I have seen re-trying commands make it worse.
  if (cmdRes != WNC_AT_CMD_OK)
      return (false);

  // Disable unsolicited RRCSTATE responses. These are supposed to be off
  // by default but have been found to be active.
  // This problem introduced in: NQ_MPSS_IMA3_v10.58.174043 LTE-M firmware
  cmdRes = at_send_wnc_cmd("AT%NOTIFYEV=\"ALL\",0", &pRespStr, m_sCmdTimeoutMs);
  if (cmdRes != WNC_AT_CMD_OK)
      return (false);
  
  cmdRes = at_send_wnc_cmd("AT@INTERNET=1", &pRespStr, m_sCmdTimeoutMs);
  if (cmdRes != WNC_AT_CMD_OK)
      return (false);
  
  cmdRes = at_send_wnc_cmd("AT@SOCKDIAL=1", &pRespStr, m_sCmdTimeoutMs);
  if (cmdRes != WNC_AT_CMD_OK)
      return (false);
  
  dbgPuts("SUCCESS: AT init of WNC!");
  
  return (true);
}

int16_t WncController::at_sockopen_wnc(const char * const ip, uint16_t port, uint16_t numSock, bool tcp, uint16_t timeOutSec)
{
    string * pRespStr;
    string cmd_str("AT@SOCKCREAT=");
    AtCmdErr_e res;

    if (tcp) cmd_str += "1";  // TCP
    else cmd_str += "2";      // else UDP

    cmd_str += ",0";
    res = sendWncCmd(cmd_str.c_str(), &pRespStr, m_sCmdTimeoutMs);
    if (res == WNC_AT_CMD_OK && pRespStr->size() > 0)
    {
        size_t pos1 = pRespStr->find("T:");
        size_t pos2 = pRespStr->rfind("OK");
        if ((pos1 != string::npos) && (pos2 != string::npos)) {
            size_t numLen = pos2 - (pos1 + 2);
            string sockStr = pRespStr->substr(pos1 + 2, numLen);
            cmd_str = "AT@SOCKCONN=";
            cmd_str += sockStr;
            cmd_str += ",\"";
            cmd_str += ip;
            cmd_str += "\",";
            cmd_str += _to_string(port);
            cmd_str += ",";
            if (timeOutSec < 30)
                timeOutSec = 30;
            else if (timeOutSec > 360)
                timeOutSec = 360;
            cmd_str += _to_string(timeOutSec);
            res = sendWncCmd(cmd_str.c_str(), &pRespStr, 1000 * timeOutSec + 1000);
            if (m_sMoreDebugEnabled) {
                at_send_wnc_cmd("AT@SOCKCREAT?", &pRespStr, m_sCmdTimeoutMs);
                at_send_wnc_cmd("AT@SOCKCONN?", &pRespStr, m_sCmdTimeoutMs);
            }
            return (strtol(sockStr.c_str(), NULL, 10));
        }
        else {
            dbgPuts("Invalid sockcreat response!");
            return (0);
        }
    }
    else
        return (0);
}

bool WncController::at_sockclose_wnc(uint16_t numSock)
{
    string * pRespStr;
    string cmd_str("AT@SOCKCLOSE=");

    cmd_str += _to_string(numSock);
 
    // Don't check the cell status to close the socket
    AtCmdErr_e res = at_send_wnc_cmd(cmd_str.c_str(), &pRespStr, m_sCmdTimeoutMs);
    
    if ((res != WNC_AT_CMD_TIMEOUT) && (res != WNC_AT_CMD_OK)) {
        for (unsigned i = 0; i < WNC_SOCK_CLOSE_RETRY_CNT; i++) {
            res = at_send_wnc_cmd(cmd_str.c_str(), &pRespStr, m_sCmdTimeoutMs);
            if ((res == WNC_AT_CMD_TIMEOUT) || (res == WNC_AT_CMD_OK))
                break;
        }
    }
    
    return (res == WNC_AT_CMD_OK); 
}

bool WncController::at_dnsresolve_wnc(const char * s, string * ipStr)
{
    string * pRespStr;
    string str(s);
    AtCmdErr_e r;

    ipStr->erase(); // Clear out string until resolved!
    str = "AT@DNSRESVDON=\"" + str;
    str += "\"";
    r = sendWncCmd(str.c_str(), &pRespStr, WNC_DNS_RESOLVE_WAIT_MS);
    if (r == WNC_AT_CMD_OK && pRespStr->size() > 0) {
        size_t pos_start = pRespStr->find("ON:\""); 
        size_t pos_end = pRespStr->find("\"", (pos_start + 4));
        if ((pos_start !=  string::npos) && (pos_end != string::npos)) {
            pos_start += 4;
            pos_end -= 1;
  
            if (pos_end > pos_start) {
                // Make a copy for use later (the source string is re-used)
                *ipStr = pRespStr->substr(pos_start, pos_end - pos_start + 1);
                return (true);
            }
        }
    }

    *ipStr = INVALID_IP_STR;

    return (false);
}

bool WncController::waitForPowerOnModemToRespond(uint8_t timeoutSecs)
{
    // Now, give the modem x seconds to start responding by
    // sending simple 'AT' commands to modem once per second.
    if (timeoutSecs > 0) {
        do {
            timeoutSecs--;
            dbgPutsNoTime("\rWaiting ", false); dbgPutsNoTime(_to_string(timeoutSecs), false);
            dbgPutsNoTime(" ", false);
            AtCmdErr_e rc = mdmSendAtCmdRsp("AT", 500, &m_sWncStr);
            if (rc == WNC_AT_CMD_OK) {
                dbgPutsNoTime("");  // CR LF
                return true; //timer.read();
            }
            waitMs(500);
        }
        while (timeoutSecs > 0);    
        dbgPutsNoTime(""); // CR LF
    }
    
    return (false);
}

WncController::AtCmdErr_e WncController::at_sockwrite_wnc(const uint8_t * s, uint16_t n, uint16_t numSock, bool isTcp)
{
    AtCmdErr_e result;

    if ((n > 0) && (n <= MAX_WNC_WRITE_BYTES)) {
        string * pRespStr;
        const char * num2str;
        string cmd_str;

        if (isTcp == true)
            cmd_str="AT@SOCKWRITE=";
        else
            cmd_str="AT@SOCKWRITE="; // "AT@SOCKSEND=";

        cmd_str += _to_string(numSock);
        cmd_str += ",";
        cmd_str += _to_string(n);
        cmd_str += ",\"";
        while(n > 0) {
            n--;
            num2str = _to_hex_string(*s++);
            // Always 2-digit ascii hex:
            if (num2str[1] == '\0')
                cmd_str += '0';
            cmd_str += num2str;
        }
        cmd_str += "\"";
        result = sendWncCmd(cmd_str.c_str(), &pRespStr, m_sCmdTimeoutMs);
    }
    else {
        dbgPuts("sockwrite Err, string len bad!");
        result = WNC_AT_CMD_ERR;
    }
    
    return (result);
}

WncController::AtCmdErr_e WncController::at_sockread_wnc(string * pS, uint16_t numSock, bool isTcp)
{
    AtCmdErr_e result = WNC_AT_CMD_OK;

    string * pRespStr;
    string cmd_str;
    size_t pos_start=0, pos_end=0;
    int i;
    
    pS->erase();  // Start with a fresh string

    if (isTcp == true)
        cmd_str="AT@SOCKREAD=";
    else
        cmd_str="AT@SOCKREAD="; // "AT@SOCKRECV=";

    cmd_str += _to_string(numSock);
    cmd_str += ",";
    cmd_str += _to_string(MAX_WNC_READ_BYTES);
            
    // Experimental: read should not need to check cell net status
    result = at_send_wnc_cmd(cmd_str.c_str(), &pRespStr, m_sCmdTimeoutMs);
    if (result == WNC_AT_CMD_OK) {
        if (pRespStr->size() > 0) {
            pos_start = pRespStr->find("\"");
            pos_end   = pRespStr->rfind("\"");
            // Make sure search finds what it's looking for!
            if (pos_start != string::npos && pos_end != string::npos) {
                pos_start++;
                i = pos_end - pos_start;  // Num hex chars, 2 per byte
            }
            else
                i = 0;
        }
        else
            i = 0;
            
        if ((i < 0) || ((i % 2) == 1))
            dbgPuts("Invalid READ string!");
        
        if (i > 2*MAX_WNC_READ_BYTES) {
            i = 2*MAX_WNC_READ_BYTES;
            dbgPuts("DANGER WNC read data does not match length!");
        }
            
        // If data, convert the hex string into byte values
        while (i > 0) {
            i -= 2;
            *pS += (uint8_t)strtol(pRespStr->substr(pos_start, 2).c_str(), NULL, 16);
            pos_start += 2;
        }
    }

    return (result);
}

WncController::AtCmdErr_e WncController::at_sockread_wnc(uint8_t * pS, uint16_t * numRead, uint16_t n, uint16_t numSock, bool isTcp)
{
    AtCmdErr_e result = WNC_AT_CMD_OK;
    *numRead = 0;
    
    if ((n > 0) && (n <= MAX_WNC_READ_BYTES)) {
        string * pRespStr;
        string cmd_str;
        size_t pos_start=0, pos_end=0;
        int i;

        if (isTcp == true)
            cmd_str="AT@SOCKREAD=";
        else
            cmd_str="AT@SOCKREAD="; // "AT@SOCKRECV=";

        cmd_str += _to_string(numSock);
        cmd_str += ",";
        cmd_str += _to_string(n);
            
        // Experimental: read should not need to check cell net status
        result = at_send_wnc_cmd(cmd_str.c_str(), &pRespStr, m_sCmdTimeoutMs);
        if (result == WNC_AT_CMD_OK) {
            if (pRespStr->size() > 0) {
                pos_start = pRespStr->find("\"");
                pos_end   = pRespStr->rfind("\"");
                // Make sure search finds what it's looking for!
                if (pos_start != string::npos && pos_end != string::npos) {
                    pos_start++;
                    i = pos_end - pos_start;  // Num hex chars, 2 per byte
                }
                else
                    i = 0;
            }
            else
                i = 0;
                
            if ((i < 0) || ((i % 2) == 1))
                dbgPuts("Invalid READ string!");
                
            if (i > 2*n) {
                // Bound the ill formated WNC read string!
                i = 2*n;
                dbgPuts("TRUNCATING read data!");
            }

            // If data, convert the hex string into byte values
            i /= 2;
            *numRead = i;
            while (i > 0) {
                i--;
                *pS++ = (uint8_t)strtol(pRespStr->substr(pos_start, 2).c_str(), NULL, 16);
                pos_start += 2;
            }
        }
    }
    else {
        dbgPuts("sockread Err, to many to read!");
        result = WNC_AT_CMD_ERR;
    }

    return (result);
}

bool WncController::at_reinitialize_mdm(void)
{
     // Atempt to re-register
//     string * pRespStr;
//     dbgPuts("Force re-register!");
//     at_send_wnc_cmd("AT+CFUN=0,0", &pRespStr, m_sCmdTimeoutMs);
//     waitMs(31000);
//     at_send_wnc_cmd("AT+CFUN=1,0", &pRespStr, m_sCmdTimeoutMs);
//     waitMs(31000);
    
    // Initialize the modem
    dbgPuts("Modem RE-initializing with SOFT Reset...");

    string * pRespStr;
    at_send_wnc_cmd("AT@DMREBOOT", &pRespStr, m_sCmdTimeoutMs);
    waitMs(5000);

    // Now, give the modem time to start responding by
    // sending simple 'AT' commands to the modem once per second.
    int timeoutSecs = WNC_REINIT_MAX_TIME_MS;
    do {
        dbgPuts("\rWaiting ", false); dbgPutsNoTime(_to_string(timeoutSecs), false);
        AtCmdErr_e rc = mdmSendAtCmdRsp("AT", 500, &m_sWncStr);
        if (rc == WNC_AT_CMD_OK) {
            dbgPutsNoTime("");  // CR LF
            break;
        }
        waitMs(500);
        timeoutSecs--;
    }
    while (timeoutSecs > 0);    
    
    if (timeoutSecs <= 0)
        dbgPuts("\r\nModem RE-init FAILED!");
    else
        dbgPuts("\r\nModem RE-init complete!");
        
    return (timeoutSecs > 0);
}

WncController::AtCmdErr_e WncController::mdmSendAtCmdRsp(const char *cmd, int timeout_ms, string * rsp, bool crLf)
{
    rsp->erase(); // Clean up from possible prior cmd response

    // Don't bother the WNC if user hasn't turned it on.
    if (m_sState == WNC_OFF)
        return (WNC_AT_CMD_WNC_NOT_ON);
        
    size_t n = strlen(cmd);
    
    // Wait per WNC advise
    waitMs(WNC_WAIT_FOR_AT_CMD_MS);
 
    if (cmd && n > 0) {
        sendCmd(cmd, crLf);
//        sendCmd(cmd, n, 1000, crLf);  // 3rd arg is micro seconds between chars sent
    }

    startTimerA();
    while (getTimerTicksA_mS() < timeout_ms) {
        n = mdmGetline(rsp, timeout_ms - getTimerTicksA_mS());

        if (n == 0)
            continue;
        
        if (rsp->rfind("OK") != string::npos) {
            stopTimerA();
            return (WNC_AT_CMD_OK);
        }
        
        if (rsp->rfind("+CME ERROR") != string::npos) {
            stopTimerA();
            return (WNC_AT_CMD_ERRCME);
        }
        
        if (rsp->rfind("@EXTERR") != string::npos) {
            stopTimerA();
            return (WNC_AT_CMD_ERREXT);
        }
            
        if (rsp->rfind("ERROR") != string::npos) {
            stopTimerA();
            return (WNC_AT_CMD_ERR);
        }
    }
    stopTimerA();
    
    return (WNC_AT_CMD_TIMEOUT);
}

bool WncController::at_setapn_wnc(const char * const apnStr)
{
    string * pRespStr;
    
    string cmd_str("AT%PDNSET=1,");
    cmd_str += apnStr;
    cmd_str += ",IP";
    if (WNC_AT_CMD_OK == at_send_wnc_cmd(cmd_str.c_str(), &pRespStr, WNC_APNSET_TIMEOUT_MS))  // Set APN, cmd seems to take a little longer sometimes
        return (true);
    else
        return (false);
}

bool WncController::at_getrssiber_wnc(int16_t * dBm, int16_t * ber)
{
    string * pRespStr;
    AtCmdErr_e cmdRes;    
    cmdRes = at_send_wnc_cmd("AT+CSQ", &pRespStr, m_sCmdTimeoutMs);       // Check RSSI,BER
    if (cmdRes != WNC_AT_CMD_OK)
        return (false);
    
    if (pRespStr->size() == 0) {
        dbgPuts("Strange RSSI result!");
        return (false);
    }
    else {
        size_t pos1 = pRespStr->find("SQ:");
        size_t pos2 = pRespStr->rfind(",");
        // Sanity check
        if ((pos1 != string::npos) && (pos2 != string::npos) && (pos2 > pos1)) {
            string subStr = pRespStr->substr(pos1 + 4, pos2 - pos1 );
            int rawRssi = atoi(subStr.c_str());
        
            // Convert WNC RSSI into dBm range:
            //  0 - -113 dBm
            //  1 - -111 dBm
            //  2..30 - -109 to -53 dBm
            //  31 - -51dBm or >
            //  99 - not known or not detectable
            if (rawRssi == 99)
                *dBm = -199;
            else if (rawRssi == 0)
                *dBm = -113;
            else if (rawRssi == 1)
                *dBm = -111;
            else if (rawRssi == 31)
                *dBm = -51;
            else if (rawRssi >= 2 && rawRssi <= 30)
                *dBm = -113 + 2 * rawRssi;
            else {
                dbgPuts("Invalid RSSI!");
                return (false);
            }
            // Parse out BER: 0..7 as RXQUAL values in the table 3GPP TS 45.008 subclause 8.2.4
            //                99 - unknown or undetectable
            subStr = pRespStr->substr(pos2 + 1, pRespStr->length() - (pos2 + 1));
            *ber = atoi(subStr.c_str());
        }
        else {
            dbgPuts("Strange RSSI result2!");
            return (false);
        }
    }
    
    return (true);
}

bool WncController::checkCellLink(void)
{
    string * pRespStr;
    size_t pos;
    int regSts;
    int cmdRes1, cmdRes2;

    if (m_sState == WNC_OFF)
        return (false);
    
    m_sState = WNC_ON_NO_CELL_LINK;

    if (m_sMoreDebugEnabled)
        dbgPuts("<-------- Begin Cell Status ------------");

    cmdRes1 = at_send_wnc_cmd("AT+CSQ", &pRespStr, m_sCmdTimeoutMs);       // Check RSSI,BER

    // If no response, don't bother with more commands
    if (cmdRes1 != WNC_AT_CMD_TIMEOUT)
        cmdRes2 = at_send_wnc_cmd("AT+CPIN?", &pRespStr, m_sCmdTimeoutMs);     // Check if SIM locked
    else {
        if (m_sMoreDebugEnabled)
            dbgPuts("------------ WNC No Response! --------->");

        return (false);
    }
    
    if ((cmdRes1 != WNC_AT_CMD_OK) || (cmdRes2 != WNC_AT_CMD_OK) || (pRespStr->size() == 0))
    {
        if (m_sMoreDebugEnabled)
        {
            if ((cmdRes1 == WNC_AT_CMD_TIMEOUT) || (cmdRes2 == WNC_AT_CMD_TIMEOUT))
                dbgPuts("------------ WNC No Response! --------->");
            else
                dbgPuts("------------ WNC Cmd Error! ----------->");
        }
        
        // If by a miracle it responds to the 2nd after the 1st, keep going
        if ((cmdRes2 == WNC_AT_CMD_TIMEOUT) || (pRespStr->size() == 0))
            return (false);      
    }
  
    // If SIM Card not ready don't bother with commands!
    if (pRespStr->find("CPIN: READY") == string::npos)
    {
        if (m_sMoreDebugEnabled)
            dbgPuts("------------ WNC SIM Problem! --------->");

        return (false);
    }

    // SIM card OK, now check for signal and cellular network registration
    cmdRes1 = at_send_wnc_cmd("AT+CREG?", &pRespStr, m_sCmdTimeoutMs);      // Check if registered on network
    if (cmdRes1 != WNC_AT_CMD_OK || pRespStr->size() == 0)
    {
        if (m_sMoreDebugEnabled)
            dbgPuts("------------ WNC +CREG? Fail! --------->");

        return (false);
    }
    else
    {
        pos = pRespStr->find("CREG: ");
        if (pos != string::npos)
        {
            // The registration is the 2nd arg in the comma separated list
            *pRespStr = pRespStr->substr(pos+8, 1);
            regSts = atoi(pRespStr->c_str());
            switch (regSts) {
                case 1:
                case 5:
                case 6:
                case 7:
                    m_sReadyForSMS = true;
                    break;
                default:
                    m_sReadyForSMS = false;
                    dbgPuts("SMS Service Down!");
            }

            // 1 - registered home, 5 - registered roaming
            if ((regSts != 1) && (regSts != 5))
            {
                if (m_sMoreDebugEnabled)
                    dbgPuts("------ WNC Cell Link Down for Data! --->");

                return (false);
            }
        }

        if (m_sMoreDebugEnabled)
            dbgPuts("------------ WNC Ready ---------------->");
    }
    
    // If we made it this far and the WNC did respond, keep the ON state
    if (m_sState != WNC_NO_RESPONSE)
        m_sState = WNC_ON;
    
    return (true);
}

int WncController::dbgPutsNoTime(const char * s, bool crlf)
{
    if (m_sDebugEnabled == true) {
        int r = dbgWriteChars(s);
        if (crlf == true)
            return (dbgWriteChars("\r\n"));
        else
            return (r);
    }
    else
        return 0;
};

int WncController::dbgPuts(const char * s, bool crlf)
{
    dbgPutsNoTime("[*] ", false);
    dbgPutsNoTime(_to_string(getLogTimerTicks()), false);
    dbgPutsNoTime(" ", false);

    int r = dbgPutsNoTime(s, false);
    
    if (crlf == true)
        return (dbgPutsNoTime("", true));
    else
        return (r);
};
    
void WncController::sendCmd(const char * cmd, bool crLf)
{
    puts(cmd);
    if (crLf == true)
        puts("\r\n");
}

void WncController::sendCmd(const char * cmd, unsigned n, unsigned wait_uS, bool crLf)
{
    while (n--) {
        putc(*cmd++);
        waitUs(wait_uS);
    };
    if (crLf == true) {
        putc('\r');
        waitUs(wait_uS);
        putc('\n');
        waitUs(wait_uS);
    }
}

}; // End namespace WncController_fk

