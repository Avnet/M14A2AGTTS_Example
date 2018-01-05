
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
            if( m_stdiofp )
                ret=fputs(buffer,m_stdiofp);
            else
                ret=m_puart->puts(buffer);
            va_end (args);
            return ret;
            }

    int putc( int c ) {
            int ret=0;
            if( m_stdiofp )
                ret=fputc(c, m_stdiofp);
            else
                ret=m_puart->putc(c);
            return ret;
            }

    int puts( const char * str ) {
            int ret=0;
            if( m_stdiofp )
                ret=fputs(str,m_stdiofp);
            else
                ret=m_puart->puts(str);
            return ret;
            }

private:
    std::FILE *m_stdiofp;
    BufferedSerial *m_puart;
};
#endif