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
    
    @file          WncControllerK64F.h
    @purpose       Contains K64F and mbed specifics to control the WNC modem using the WncController base class.
    @version       1.0
    @date          July 2016
    @author        Fred Kellerman
*/

#ifndef __WNCCONTROLLERK64F_H_
#define __WNCCONTROLLERK64F_H_

#include <string>
#include <stdint.h>
#include "mbed.h"
#include "WNCDebug.h"
#include "WncController.h"

namespace WncControllerK64F_fk {

using namespace WncController_fk;
using namespace std;

/** List of K64F pins that are used to control and setup the ATT IoT Kit WNC Shield */
struct WncGpioPinListK64F {
    /////////////////////////////////////////////////////
    // NXP GPIO Pins that are used to initialize the WNC Shield
    /////////////////////////////////////////////////////
    DigitalOut * mdm_uart2_rx_boot_mode_sel;   // on powerup, 0 = boot mode, 1 = normal boot
    DigitalOut * mdm_power_on;                 // 0 = turn modem on, 1 = turn modem off (should be held high for >5 seconds to cycle modem)
    DigitalOut * mdm_wakeup_in;                // 0 = let modem sleep, 1 = keep modem awake -- Note: pulled high on shield
    DigitalOut * mdm_reset;                    // active high
    DigitalOut * shield_3v3_1v8_sig_trans_ena; // 0 = disabled (all signals high impedence, 1 = translation active
    DigitalOut * mdm_uart1_cts;
};    


/**
 * @author Fred Kellerman
 * @see API 
 *
 * <b>WncControllerK64F</b> This mbed C++ class is for controlling the WNC
 * Cellular modem from the NXP K64F Freedom board.  It uses the control code
 * from it's base class WncController to handle the WNC Modem AT cmds.  This
 * class fulfills various pure virtual methods of the base class.  The point of
 * this class is to have the platform specific code in it thus isolating the
 * control code logic from any particular platform or OS.
 */
class WncControllerK64F : public WncController
{
public:

    /**
     *
     * Sets up the resources to control the WNC modem shield.
     * @ingroup API
     * @param pPins - pointer to a list of K64F pins that are used to setup and control the ATT IoT Kit's WNC Shield.
     * @param wnc_uart - a pointer to the serial uart that is used to communicate with the WNC modem.
     * @param debug_uart - a pointer to a serial uart for the debug output to go out of, if NULL debug will not be output.
     */
    WncControllerK64F(struct WncGpioPinListK64F * pPins, BufferedSerial * wnc_uart, WNCDebug * debug_uart = NULL);
    
    /**
     *
     *  Activates a mode where the user can send text to and from the K64F
     *  debug Serial port directly to the WNC.  The mode is entered via this
     *  call.  The mode is exited when the user types CTRL-Q.  While in this
     *  mode all text to and from the WNC is consumed by the debug Serial port.
     *  No other methods in the class will receive any of the WNC output.
     * @ingroup API
     * @param pUart - a pointer to a uart to use to collect the user input and put the output from the WNC.
     * @param echoOn - set to true to echo what is input back to the output of pUart.
     */
    bool enterWncTerminalMode(BufferedSerial *pUart, bool echoOn);
    
private:

    // Disallow copy
    WncControllerK64F operator=(WncControllerK64F lhs);

    // Users must define these functionalities:
    virtual int putc(char c);
    virtual int puts(const char * s);
    virtual char getc(void);
    virtual int charReady(void);
    virtual int dbgWriteChar(char b);
    virtual int dbgWriteChars(const char *b);
    virtual bool initWncModem(uint8_t powerUpTimeoutSecs);
    virtual void waitMs(int t);
    virtual void waitUs(int t);
    
    virtual int  getLogTimerTicks(void);
    virtual void startTimerA(void);
    virtual void stopTimerA(void);
    virtual int  getTimerTicksA_mS(void);
    virtual void startTimerB(void);
    virtual void stopTimerB(void);
    virtual int  getTimerTicksB_mS(void);

    WNCDebug * m_pDbgUart;
    BufferedSerial * m_pWncUart;
    WncGpioPinListK64F m_gpioPinList;
    Timer m_logTimer;
    Timer m_timerA;
    Timer m_timerB;
};

};  // End namespace WncController_fk

#endif

