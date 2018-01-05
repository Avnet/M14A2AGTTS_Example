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
    
    @file          WncControllerK64F.cpp
    @purpose       Contains K64F and mbed specifics to control the WNC modem using the WncController base class.
    @version       1.0
    @date          July 2016
    @author        Fred Kellerman
*/

#include "WncControllerK64F.h"

using namespace WncControllerK64F_fk;

WncControllerK64F::WncControllerK64F(struct WncGpioPinListK64F * pPins, BufferedSerial * wnc_uart, WNCDebug * debug_uart)
{
    m_logTimer.start(); // Start the log timer now!    
    m_pDbgUart = debug_uart;
    m_pWncUart = wnc_uart;
    m_gpioPinList = *pPins;
}

bool WncControllerK64F::enterWncTerminalMode(BufferedSerial * pUart, bool echoOn)
{
    if (pUart == NULL)
        return (false);  // Need a uart!
        
    string * resp;
    AtCmdErr_e r = sendWncCmd("AT", &resp, 500);
    if (r == WNC_AT_CMD_TIMEOUT)
        return (false);
    
    pUart->puts("\r\nEntering WNC Terminal Mode - press <CTRL>-Q to exit!\r\n");
    
    while (1) {
        if (pUart->readable()) {
            char c = pUart->getc();
            if (c == '\x11') {
                pUart->puts("\r\nExiting WNC Terminal Mode!\r\n");
                // Cleanup in case user doesn't finish command:
                sendWncCmd("AT", &resp, 300);
                // Above AT may fail but should get WNC back in sync
                return (sendWncCmd("AT", &resp, 500) == WNC_AT_CMD_OK);
            }
            if (echoOn == true) {
                pUart->putc(c);
            }
            m_pWncUart->putc(c);
        }
        if (m_pWncUart->readable())
            pUart->putc(m_pWncUart->getc());
    }
}

int WncControllerK64F::putc(char c)
{
    return (m_pWncUart->putc(c));
}

int WncControllerK64F::puts(const char * s)
{
    return (m_pWncUart->puts(s));
}

char WncControllerK64F::getc(void)
{
    return (m_pWncUart->getc());
}

int WncControllerK64F::charReady(void)
{
    return (m_pWncUart->readable());
}

int WncControllerK64F::dbgWriteChar(char b)
{
    if (m_pDbgUart != NULL)
        return (m_pDbgUart->putc(b));
    else
        return (0);
}

int WncControllerK64F::dbgWriteChars(const char * b)
{
    if (m_pDbgUart != NULL)
        return (m_pDbgUart->puts(b));
    else
        return (0);
}

bool WncControllerK64F::initWncModem(uint8_t powerUpTimeoutSecs)
{
    // Hard reset the modem (doesn't go through
    // the signal level translator)
    *m_gpioPinList.mdm_reset = 0;

    // disable signal level translator (necessary
    // for the modem to boot properly).  All signals
    // except mdm_reset go through the level translator
    // and have internal pull-up/down in the module. While
    // the level translator is disabled, these pins will
    // be in the correct state.
    *m_gpioPinList.shield_3v3_1v8_sig_trans_ena = 0;

    // While the level translator is disabled and ouptut pins
    // are tristated, make sure the inputs are in the same state
    // as the WNC Module pins so that when the level translator is
    // enabled, there are no differences.
    *m_gpioPinList.mdm_uart2_rx_boot_mode_sel = 1;   // UART2_RX should be high
    *m_gpioPinList.mdm_power_on = 0;                 // powr_on should be low
    *m_gpioPinList.mdm_wakeup_in = 1;                // wake-up should be high
    *m_gpioPinList.mdm_uart1_cts = 0;                // indicate that it is ok to send

    // Now, wait for the WNC Module to perform its initial boot correctly
    waitMs(1000);

    // The WNC module initializes comms at 115200 8N1 so set it up
    m_pWncUart->baud(115200);

    //Now, enable the level translator, the input pins should now be the
    //same as how the M14A module is driving them with internal pull ups/downs.
    //When enabled, there will be no changes in these 4 pins...
    *m_gpioPinList.shield_3v3_1v8_sig_trans_ena = 1;
    
    bool res = waitForPowerOnModemToRespond(powerUpTimeoutSecs);
    
    // Toggle wakeup to prevent future dropped 'A' of "AT", this was
    //  suggested by ATT.
    if (res == true) {
        dbgPuts("\r\nToggling Wakeup...");
        waitMs(20);
        *m_gpioPinList.mdm_wakeup_in = 0;
        waitMs(2000);
        *m_gpioPinList.mdm_wakeup_in = 1;
        waitMs(20);
        dbgPuts("Toggling complete.");
    }

    return (res);
}

void WncControllerK64F::waitMs(int t)
{
    wait_ms(t);
}

void WncControllerK64F::waitUs(int t)
{
    wait_ms(t);
}

int  WncControllerK64F::getLogTimerTicks(void)
{
    return (m_logTimer.read_us());
}

void WncControllerK64F::startTimerA(void)
{
    m_timerA.start();
    m_timerA.reset();
}

void WncControllerK64F::stopTimerA(void)
{
    m_timerA.stop();
}

int  WncControllerK64F::getTimerTicksA_mS(void)
{
    return (m_timerA.read_ms());
}

void WncControllerK64F::startTimerB(void)
{
    m_timerB.start();
    m_timerB.reset();
}

void WncControllerK64F::stopTimerB(void)
{
    m_timerB.stop();
}

int  WncControllerK64F::getTimerTicksB_mS(void)
{
    return (m_timerB.read_ms());
}

