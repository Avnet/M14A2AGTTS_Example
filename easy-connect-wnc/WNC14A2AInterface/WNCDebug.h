/* WNC14A2A implementation of NetworkInterfaceAPI
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
 

#ifndef __WNCDEBUG__
#define __WNCDEBUG__
#include <stdio.h>
#include "BufferedSerial.h"


class WNCDebug
{
public:
    WNCDebug( FILE * fd = stderr ): m_puart(NULL) {m_stdiofp=fd;};
    WNCDebug( BufferedSerial * uart): m_stdiofp(NULL) {m_puart=uart;};
    ~WNCDebug() {};

    int printf( char * fmt, ...) {
            char buffer[256];
            int ret=0;
            va_list args;
            va_start (args, fmt);
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            prt.lock();
            if( m_stdiofp )
                ret=fputs(buffer,m_stdiofp);
            else
                ret=m_puart->puts(buffer);
            prt.unlock();
            va_end (args);
            return ret;
            }

    int putc( int c ) {
            int ret=0;
            prt.lock();
            if( m_stdiofp )
                ret=fputc(c, m_stdiofp);
            else
                ret=m_puart->putc(c);
            prt.unlock();
            return ret;
            }

    int puts( const char * str ) {
            int ret=0;
            prt.lock();
            if( m_stdiofp )
                ret=fputs(str,m_stdiofp);
            else
                ret=m_puart->puts(str);
            prt.unlock();
            return ret;
            }

private:
    std::FILE *m_stdiofp;
    BufferedSerial *m_puart;
    Mutex prt;
};
#endif
