#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <strings.h>
#include <string>
#include <vector>
#include <netinet/in.h>

#ifndef _WIN32
inline char * my_strupr( char * value )
{
        if ( value == nullptr )
        {
                return nullptr;
        }

        for ( char * ch = value; *ch != '\0'; ++ch )
        {
                *ch = static_cast<char>( std::toupper( static_cast<unsigned char>( *ch )));
        }
        return value;
}

#define strcmpi strcasecmp
#define strupr my_strupr
#endif

#include "cstring.h"

constexpr WORD LOGM_INIT = 0x0100;
constexpr WORD LOGM_SAVE = 0x0200;

struct LogEventEntry
{
        WORD m_mask;
        std::string m_message;
};

class CLog
{
public:
        void Event( WORD mask, const char * fmt, ... )
        {
                va_list args;
                va_start( args, fmt );

                char buffer[1024];
                int count = std::vsnprintf( buffer, sizeof( buffer ), fmt, args );
                if ( count < 0 )
                {
                        buffer[0] = '\0';
                }
                va_end( args );

                m_events.push_back( LogEventEntry{ mask, std::string( buffer ) } );
        }

        void Clear()
        {
                m_events.clear();
        }

        const std::vector<LogEventEntry> & Events() const
        {
                return m_events;
        }

private:
        std::vector<LogEventEntry> m_events;
};

struct CRealTime
{
        int m_Year;
        int m_Month;
        int m_Day;
        int m_Hour;
        int m_Min;
        int m_Sec;

        bool IsValid() const
        {
                return true;
        }
};

class CServer
{
public:
        CServer() : m_loading( false ) {}

        bool IsLoading() const
        {
                return m_loading;
        }

        void SetLoading( bool loading )
        {
                m_loading = loading;
        }

private:
        bool m_loading;
};

struct CServerMySQLConfig
{
        bool m_fEnable;
        CGString m_sHost;
        int m_iPort;
        CGString m_sDatabase;
        CGString m_sUser;
        CGString m_sPassword;
        CGString m_sTablePrefix;
        CGString m_sCharset;
        bool m_fAutoReconnect;
        int m_iReconnectTries;
        int m_iReconnectDelay;

        CServerMySQLConfig() :
                m_fEnable( false ),
                m_sHost( "localhost" ),
                m_iPort( 3306 ),
                m_fAutoReconnect( true ),
                m_iReconnectTries( 3 ),
                m_iReconnectDelay( 5 )
        {
                m_sDatabase.Empty();
                m_sUser.Empty();
                m_sPassword.Empty();
                m_sTablePrefix.Empty();
                m_sCharset = "utf8mb4";
        }
};

enum StorageDirtyType : int
{
        StorageDirtyType_None = 0,
        StorageDirtyType_Save,
        StorageDirtyType_Delete,
};

extern CLog g_Log;
extern CServer g_Serv;

