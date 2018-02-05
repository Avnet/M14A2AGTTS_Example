/** 
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
    
    @file          WncController.h
    @purpose       Controls WNC Cellular Modem
    @version       1.0
    @date          July 2016
    @author        Fred Kellerman
    
    Notes: This code originates from the following mbed repository:
    
    https://developer.mbed.org/teams/Avnet/code/WncControllerLibrary/
*/


#ifndef __WNCCONTROLLER_H_
#define __WNCCONTROLLER_H_

#include <string>
#include <stdint.h>

namespace WncController_fk {

using namespace std;

/** @defgroup API The WncControllerLibrary API */
/** @defgroup MISC Misc WncControllerLibrary functions */
/** @defgroup INTERNALS WncControllerLibrary Internals */

static const uint8_t  MAX_LEN_IP_STR = 16;         // Length includes room for the extra NULL

/** \brief  Contains info fields for the WNC Internet Attributes */
struct WncIpStats
{
    string wncMAC;
    char ip[MAX_LEN_IP_STR];
    char mask[MAX_LEN_IP_STR];
    char gateway[MAX_LEN_IP_STR];
    char dnsPrimary[MAX_LEN_IP_STR];
    char dnsSecondary[MAX_LEN_IP_STR];
};


/**
 * @author Fred Kellerman
 * @see API 
 *
 * <b>WncController</b> This mbed C++ class is for controlling the WNC
 *  Cellular modem via the serial AT command interface.  This was
 *  developed with respect to version 1.3 of the WNC authored
 *  unpublished spec.  This class is only designed to have 1 instantiation,
 *  it is also not multi-thread safe.  There are no OS specific
 *  entities being used, there are pure virtual methods that an 
 *  inheriting class must fulfill.  That inheriting class will have
 *  OS and platform specific entities.  See WncControllerK64F for an
 *  example for the NXP K64F Freedom board.
 */
class WncController
{
public:

    static const unsigned MAX_NUM_WNC_SOCKETS = 5;  // Max number of simultaneous sockets that the WNC supports
    static const unsigned MAX_POWERUP_TIMEOUT = 60; // How long the powerUp method will try to turn on the WNC Shield
                                                    //  (this is the default if the user does not over-ride on power-up

    /** Tracks mode of the WNC Shield hardware */
    enum WncState_e {
        WNC_OFF = 0,
        WNC_ON, // This is intended to mean all systems go, including cell link up but socket may not be open
        WNC_ON_NO_CELL_LINK,
        WNC_NO_RESPONSE
    };

    /**
     *
     * Constructor for WncController class, sets up internals.
     * @ingroup API
     * @return none.
     */
    WncController(void);
    virtual ~WncController()=0;

    /**
     *
     * Used internally but also make public for a user of the Class to 
     * interrogate state as well.
     * @ingroup API
     * @return the current state of the Wnc hardware.
     */
    WncState_e getWncStatus(void);
    
    /**
     *
     * Allows a user to set the WNC modem to use the given Cellular APN 
     * @ingroup API
     * @param apnStr - a null terminated c-string
     * @return true if the APN set was succesful, else false
     */
    bool setApnName(const char * const apnStr);

    /**
     *
     * Queries the WNC modem for the current RX RSSI in units of coded dBm
     * @ingroup API
     * @return 0 – -113 dBm or less
     *         1 – -111 dBm
     *         2...30 – -109 dBm to –53 dBm
     *        31 – -51 dBm or greater
     *        99 – not known or not detectable
     */
    int16_t getDbmRssi(void);
    
    /**
     *
     * Queries the WNC modem for the current Bit Error Rate
     * @ingroup API
     * @return 0...7 – as RXQUAL values in the table in 3GPP TS 45.008
     *            subclause 8.2.4
     *         99 – not known or not detectable
     */
    int16_t get3gBer(void);

    /**
     *
     * Powers up the WNC modem
     * @ingroup API
     * @param apn - the apn c-string to set the WNC modem to use
     * @param powerUpTimeoutSecs - the amount of time to wait for the WNC modem to turn on
     * @return true if powerup was a success, else false.
     */
    bool powerWncOn(const char * const apn, uint8_t powerUpTimeoutSecs = MAX_POWERUP_TIMEOUT);

    /**
     *
     * Returns the NAT Self, gateway, masks and dns IP
     * @ingroup API
     * @param s - a pointer to a struct that will contain the IP info.
     * @return true if success, else false.
     */
    bool getWncNetworkingStats(WncIpStats * s);

    /**
     *
     * Takes a text URL and converts it internally to an IP address for the
     * socket number given.
     * @ingroup API
     * @param numSock - The number of the socket to lookup the IP address for.
     * @param url - a c-string text URL
     * @return true if success, else false.
     */
    bool resolveUrl(uint16_t numSock, const char * url);

    /**
     *
     * If you know the IP address you can set the socket up to use it rather
     * than using a text URL.
     * @ingroup API
     * @param numSock - The number of the socket to use the IP address for.
     * @param ipStr - a c-string text IP addrese like: 192.168.0.1
     * @return true if success, else false.
     */
    bool setIpAddr(uint16_t numSock, const char * ipStr);

    /**
     *
     * Opens a socket for the given number, port and IP protocol.  Before
     * using open, you must use either resolveUrl() or setIpAddr().
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @param port - the IP port to open
     * @param tcp - set true for TCP, false for UDP
     * @param timeoutSec - the amount of time in seconds to wait for the open to complete
     * @return true if success, else false.
     */
    bool openSocket(uint16_t numSock, uint16_t port, bool tcp, uint16_t timeOutSec = 30);

    /**
     *
     * Opens a socket for the given text URL, number, port and IP protocol.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @param url - a c-string text URL, the one to open a socket for.
     * @param port - the IP port to open.
     * @param tcp - set true for TCP, false for UDP.
     * @param timeoutSec - the amount of time in seconds to wait for the open to complete.
     * @return true if success, else false.
     */
    bool openSocketUrl(uint16_t numSock, const char * url, uint16_t port, bool tcp, uint16_t timeOutSec = 30);

    /**
     *
     * Opens a socket for the given text IP address, number, port and IP protocol.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @param ipAddr - a c-string text IP address like: "192.168.0.1".
     * @param port - the IP port to open.
     * @param tcp - set true for TCP, false for UDP.
     * @param timeoutSec - the amount of time in seconds to wait for the open to complete.
     * @return true if success, else false.
     */
    bool openSocketIpAddr(uint16_t numSock, const char * ipAddr, uint16_t port, bool tcp, uint16_t timeOutSec = 30);


    /**
     *
     * Write data bytes to a Socket, the Socket must already be open.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @parma s - an array of bytes to write to the socket.
     * @param n - the number of bytes to write.
     * @return true if success, else false.
     */
     bool write(uint16_t numSock, const uint8_t * s, uint32_t n);

    /**
     *
     * Poll to read available data bytes from an already open Socket.  This method
     * will retry reads to what setReadRetries() sets it to and the delay in between
     * retries that is set with setReadRetryWait()
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @parma readBuf - a pointer to where read will put the data.
     * @param maxReadBufLen - The number of bytes readBuf has room for.
     * @return the number of bytes actually read into readBuf.  0 is a valid value if no data is available.
     */
    size_t read(uint16_t numSock, uint8_t * readBuf, uint32_t maxReadBufLen);
    
    /**
     *
     * Poll to read available data bytes from an already open Socket.  This method
     * will retry reads to what setReadRetries() sets it to and the delay in between
     * retries that is set with setReadRetryWait()
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @parma readBuf - a pointer to pointer that will be set to point to an internal byte buffer that contains any read data.
     * @return the number of bytes actually read into the pointer that readBuf points to.  0 is a valid value if no data is available.
     */
    size_t read(uint16_t numSock, const uint8_t ** readBuf);

    /**
     *
     * Set the number of retries that the read methods will use.  If a read returns 0 data this setting will have the read
     * re-read to see if new data is available.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @parma retries - the number of retries to perform.
     * @return none.
     */
    void setReadRetries(uint16_t numSock, uint16_t retries);

    /**
     *
     * Set the time between retires that the read methods will use.  If a read returns 0 data this setting will have the read
     * re-read and use this amount of delay in between the re-reads.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @parma waitMs - the amount of time in mS to wait between retries.
     * @return none.
     */
    void setReadRetryWait(uint16_t numSock, uint16_t waitMs);

    /**
     *
     * Closes an already open Socket.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @return true if success else false.
     */
    bool closeSocket(uint16_t numSock);

    /**
     *
     * Sets the amount of time to wait between the raw AT commands that are sent to the WNC modem.
     * Generally you don't want to use this but it is here just in case.
     * @ingroup API
     * @param toMs - num mS to wait between the AT cmds.
     * @return none.
     */
    void setWncCmdTimeout(uint16_t toMs);

    /**
     *
     * Gets the IP address of the given socket number.
     * @ingroup API
     * @param numSock - The number of the socket to open.
     * @param myIpAddr - a c-string that contains the socket's IP address.
     * @return true if success else false.
     */
    bool getIpAddr(uint16_t numSock, char myIpAddr[MAX_LEN_IP_STR]);
    
    /**
     *
     * Enables debug output from this class.
     * @ingroup API
     * @param on - true enables debug output, false disables
     * @param moreDebugOn - true enables verbose debug, false truncates debug output.
     * @return none.
     */
    void enableDebug(bool on, bool moreDebugOn);
    
    ///////////////////////////////////////////
    //  SMS messaging
    ///////////////////////////////////////////

    static const uint16_t MAX_WNC_SMS_MSG_SLOTS = 3;   // How many SMS messages the WNC can store and receive at a time.
    static const uint16_t MAX_WNC_SMS_LENGTH    = 160; // The maximum length of a 7-bit SMS message the WNC can send and receive.
    
    /** Struct for SMS messages */
    struct WncSmsInfo
    {
        // Content
        char   idx;
        string number;
        string date;
        string time;
        string msg;
        
        // Attributes
        bool incoming;
        bool unsent;
        bool unread;
        bool pduMode;
        bool msgReceipt;
    };

    /** Struct to contain a list of SMS message structs */
    struct WncSmsList
    {
        uint8_t msgCount;
        WncSmsInfo e[MAX_WNC_SMS_MSG_SLOTS];
    };

    /**
     *
     * Sends an SMS text message to someone.
     * @ingroup API
     * @param phoneNum - c-string 15 digit MSISDN number or ATT Jasper number (standard phone number not supported because ATT IoT SMS does not support it).
     * @param text - the c-string text to send to someone.
     * @return true if success else false.
     */
    bool sendSMSText(const char * const phoneNum, const char * const text);

    /**
     *
     * Incoming messages are stored in a log in the WNC modem, this will read that
     * log.
     * @ingroup API
     * @param log - the log contents if reading it was successful.
     * @return true if success else false.
     */
    bool readSMSLog(struct WncSmsList * log);

    /**
     *
     * Incoming messages are stored in a log in the WNC modem, this will read out
     * messages that are unread and also then mark them read.
     * @ingroup API
     * @param w - a list of SMS messages that unread messages will be put into.
     * @param deleteRead - if a message is read and this is set true the message will be deleted from the WNC modem log.
     * If it is false the message will remain in the internal log but be marked as read.
     * @return true if success else false.
     */
    bool readUnreadSMSText(struct WncSmsList * w, bool deleteRead = true);
    
    /**
     *
     * Saves a text message into internal SIM card memory of the WNC modem.
     * There are only 3 slots available this is for unread, read and saved.
     * @ingroup API
     * @param phoneNum - c-string 15 digit MSISDN number or ATT Jasper number (standard phone number not supported because ATT IoT SMS does not support it).
     * @param text - the c-string text to send to someone.
     * @param msgIdx - the slot position to save the message: '1', '2', '3'
     * @return true if success else false.
     */
    bool saveSMSText(const char * const phoneNum, const char * const text, char * msgIdx);

    /**
     *
     * Sends a prior stored a text message from internal SIM card memory of the WNC modem.
     * If no messages are stored the behaviour of this method is undefined.
     * @ingroup API
     * @param msgIdx - the slot position to save the message: '1', '2', '3'
     * @return true if success else false.
     */
    bool sendSMSTextFromMem(char msgIdx);

    /**
     *
     * Deletes a prior stored a text message from internal SIM card memory of the WNC modem.
     * If no messages are stored the behaviour of this method is undefined.
     * @ingroup API
     * @param msgIdx - the slot position to save the message: '1', '2', '3' or '*' deletes them all.
     * @return true if success else false.
     */
    bool deleteSMSTextFromMem(char msgIdx);
    
    /**
     *
     * Retreives the SIM card ICCID number.
     * @ingroup API
     * @param iccid - a pointer to C++ string that contains the retrieved number.
     * @return true if success else false.
     */
    bool getICCID(string * iccid);

    /**
     *
     * Converts an ICCID number into a MSISDN number.  The ATT SMS system for IoT only allows use of the 15-digit MSISDN number.
     * @ingroup API
     * @param iccid - the number to convert.
     * @param msisdn - points to a C++ string that has the converted number.
     * @return true if success else false.
     */
    bool convertICCIDtoMSISDN(const string & iccid, string * msisdn);
    
    ///////////////////////////////////////////
    // Neighborhood Cell Info
    ///////////////////////////////////////////
    
    /**
     *
     * Fetches the signal quality log from the WNC modem.
     * @ingroup API
     * @param log - a pointer to an internal buffer who's contents contain the signal quality metrics.
     * @return The number of chars in the log.
     */
    size_t getSignalQuality(const char ** log);
    
    /**  A struct for the WNC modem Date and Time */
    struct WncDateTime
    {
        uint8_t  year;
        uint8_t  month;
        uint8_t  day;
        uint8_t  hour;
        uint8_t  min;
        uint8_t  sec;
    };

    /**
     *
     * Fetches the cell tower's time and date.  The time is accurate when read
     * but significant delays exist between the time it is read and returned.
     * @ingroup API
     * @param tod - User supplies a pointer to a tod struct and this method fills it in.
     * @return true if success else false.
     */
    bool getTimeDate(struct WncDateTime * tod);
    
    /**
     *
     * ICMP Pings a URL, the results are only output to the debug log for now!
     * @ingroup API
     * @param url - a c-string whose URL is to be pinged.
     * @return true if success else false.
     */
    bool pingUrl(const char * url);

    /**
     *
     * ICMP Pings an IP, the results are only output to the debug log for now!
     * @ingroup API
     * @param ip - a c-string whose IP is to be pinged.
     * @return true if success else false.
     */
    bool pingIp(const char * ip);
    
    /**
     *
     * Allows a user to send a raw AT command to the WNC modem.
     * @ingroup API
     * @param cmd - the c-string cmd to send like: "AT"
     * @param resp - a pointer to the c-string cmd's response.
     * @param sizeRespBuf - how large the command response buffer is, sets the max response length.
     * @param ms_timeout - how long to wait for the WNC to respond to your command.
     * @return the number of characters in the response from the WNC modem.
     */
    size_t sendCustomCmd(const char * cmd, char * resp, size_t sizeRespBuf, int ms_timeout);

protected:

    // Debug output methods
    int dbgPutsNoTime(const char * s, bool crlf = true);
    int dbgPuts(const char * s, bool crlf = true);
    const char * _to_string(int64_t value);
    const char * _to_hex_string(uint8_t value);    

    // Sends commands to WNC via
    enum AtCmdErr_e {
        WNC_AT_CMD_OK,
        WNC_AT_CMD_ERR,
        WNC_AT_CMD_ERREXT,
        WNC_AT_CMD_ERRCME,
        WNC_AT_CMD_INVALID_RESPONSE,
        WNC_AT_CMD_TIMEOUT,
        WNC_AT_CMD_NO_CELL_LINK,
        WNC_AT_CMD_WNC_NOT_ON
    };

    bool waitForPowerOnModemToRespond(uint8_t powerUpTimeoutSecs);    
    AtCmdErr_e sendWncCmd(const char * const s, string ** r, int ms_timeout);

    // Users must define these functionalities in the inheriting class:
        // General I/O and timing:
    virtual int putc(char c)              = 0;
    virtual int puts(const char * s)      = 0;
    virtual char getc(void)               = 0;
    virtual int charReady(void)           = 0;
    virtual int dbgWriteChar(char b)      = 0;
    virtual int dbgWriteChars(const char *b) = 0;
    virtual void waitMs(int t)            = 0;
    virtual void waitUs(int t)            = 0;
    virtual bool initWncModem(uint8_t powerUpTimeoutSecs) = 0;
    
        // Isolate OS timers
    virtual int  getLogTimerTicks(void)  = 0;
    virtual void startTimerA(void)       = 0;
    virtual void stopTimerA(void)        = 0;
    virtual int  getTimerTicksA_mS(void) = 0;
    virtual void startTimerB(void)       = 0;
    virtual void stopTimerB(void)        = 0;
    virtual int  getTimerTicksB_mS(void) = 0;
    
private:

    bool softwareInitMdm(void);
    bool checkCellLink(void);
    AtCmdErr_e mdmSendAtCmdRsp(const char * cmd, int timeout_ms, string * rsp, bool crLf = true);
    size_t mdmGetline(string * buff, int timeout_ms);
    bool at_at_wnc(void);
    bool at_init_wnc(bool hardReset = false);
    int16_t at_sockopen_wnc(const char * const ip, uint16_t port, uint16_t numSock, bool tcp, uint16_t timeOutSec);
    bool at_sockclose_wnc(uint16_t numSock);
    bool at_dnsresolve_wnc(const char * s, string * ipStr);
    AtCmdErr_e at_sockwrite_wnc(const uint8_t * s, uint16_t n, uint16_t numSock, bool isTcp);
    AtCmdErr_e at_sockread_wnc(uint8_t * pS, uint16_t * numRead, uint16_t n, uint16_t numSock, bool isTcp);
    AtCmdErr_e at_sockread_wnc(string * pS, uint16_t numSock, bool isTcp);
    bool at_reinitialize_mdm(void);
    AtCmdErr_e at_send_wnc_cmd(const char * s, string ** r, int ms_timeout);
    bool at_setapn_wnc(const char * const apnStr);
    bool at_sendSMStext_wnc(const char * const phoneNum, const char * const text);
    bool at_get_wnc_net_stats(WncIpStats * s);
    bool at_readSMSlog_wnc(string ** log);
    size_t at_readSMStext_wnc(const char ** log);
    size_t at_readSMStext_wnc(const char n, const char ** log);
    bool at_getrssiber_wnc(int16_t * dBm, int16_t * ber3g);
    void closeOpenSocket(uint16_t numSock);
    bool sockWrite(const uint8_t * const s, uint16_t n, uint16_t numSock, bool isTcp);
    bool at_sendSMStextMem_wnc(char n);
    bool at_deleteSMSTextFromMem_wnc(char n);
    bool at_saveSMStext_wnc(const char * const phoneNum, const char * const text, char * msgIdx);
    size_t at_getSignalQuality_wnc(const char ** log);
    bool at_gettimedate_wnc(struct WncDateTime * tod);
    bool at_ping_wnc(const char * ip);
    bool at_geticcid_wnc(string * iccid);
    
    // Utility methods
    void sendCmd(const char * cmd, bool crLf);
    void sendCmd(const char * cmd, unsigned n, unsigned wait_uS, bool crLf);    
    inline void rx_char_wait(void) {
        // waitUs(1000);
    }
    
    // Important constants
    static const uint16_t MAX_WNC_READ_BYTES        = 1500;                            // This bounds the largest amount of data that the WNC read from a socket will return
    static const uint16_t MAX_WNC_WRITE_BYTES       = MAX_WNC_READ_BYTES;              // This is the largest amount of data that the WNC can write per sockwrite.
    static const uint16_t MAX_LEN_WNC_CMD_RESPONSE  = (MAX_WNC_READ_BYTES * 2 + 100);  // Max number of text characters in a WNC AT response *2 because bytes are converted into 2 hex-digits +100 for other AT@ chars.
    static const uint16_t WNC_AUTO_POLL_MS          = 250;   // Sets default (may be overriden with method) poll interval (currently not used, future possible feature.
    static const uint16_t WNC_CMD_TIMEOUT_MS        = 40000; // Sets default (may be overriden) time that the software waits for an AT response from the WNC.
    static const uint16_t WNC_QUICK_CMD_TIMEOUT_MS  = 2000;  // Used for simple commands that should immediately respond such as "AT", cmds that are quicker than WNC_CMD_TIMEOUT_MS.
    static const uint16_t WNC_WAIT_FOR_AT_CMD_MS    = 0;     // Wait this much between multiple in a row AT commands to the WNC.
    static const uint16_t WNC_SOFT_INIT_RETRY_COUNT = 10;    // How many times the WNC will be tried to revive if it stops responding.
    static const uint16_t WNC_DNS_RESOLVE_WAIT_MS   = 60000; // How much time to wait for the WNC to respond to a DNS resolve/lookup.
    static const uint16_t WNC_TRUNC_DEBUG_LENGTH    = 80;    // Always make this an even number, how many chars for the debug output before shortening the debug ouput, this is used when moreDebug = false. 
    static const uint16_t WNC_APNSET_TIMEOUT_MS     = 60000; // How long to wait for the WNC to respond to setting the APN string.
    static const uint16_t WNC_PING_CMD_TIMEOUT_MS   = 60000; // Amount of time to wait for the WNC to respond to AT@PINGREQ (with cmd default params for timeout, does not change WNC cmd's timeout) 
    static const int      WNC_REINIT_MAX_TIME_MS    = 60000; // How long to wait for the WNC to reset after it was already up and running after power-up.
    static const uint16_t WNC_SOCK_CLOSE_RETRY_CNT  = 3;     // How many times to try to close the socket if the WNC gives an error.
    static const char * const INVALID_IP_STR;                // Just a string set to an IP address when DNS resolve fails.
        
    struct WncSocketInfo_s {
        int16_t numWncSock;
        bool open;
        string myIpAddressStr;
        uint16_t myPort;
        uint8_t readRetries;
        uint16_t readRetryWaitMs;
        bool isTcp;
        uint16_t timeOutSec;
    };

    static WncSocketInfo_s m_sSock[MAX_NUM_WNC_SOCKETS];
    static const WncSocketInfo_s defaultSockStruct;
    static WncState_e m_sState;
    static uint16_t m_sCmdTimeoutMs;
    static string m_sApnStr;
    static string m_sWncStr;
    static uint8_t m_sPowerUpTimeoutSecs;
    static bool m_sDebugEnabled;
    static bool m_sMoreDebugEnabled;
    static bool m_sCheckNetStatus;
    static bool m_sReadyForSMS;
};

};  // End namespace WncController_fk

#endif

