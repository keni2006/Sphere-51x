#pragma once

#ifndef _GRAYSVR_H_
#define _GRAYSVR_H_

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <strings.h>
#include <string>
#include <vector>
#include <fstream>
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

#include "common_stub.h"

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
        CServer() : m_iSavePeriod( 0 ), m_sWorldBaseDir(), m_loading( false ) {}

        bool IsLoading() const
        {
                return m_loading;
        }

        void SetLoading( bool loading )
        {
                m_loading = loading;
        }

        int m_iSavePeriod;
        CGString m_sWorldBaseDir;

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

class CWorld
{
public:
        CWorld() : m_fSaveParity( false ) {}

        bool m_fSaveParity;
};

extern CWorld g_World;

constexpr unsigned int PRIV_BLOCKED = 0x1u;
constexpr unsigned int PRIV_JAILED = 0x2u;

constexpr unsigned int STATF_SaveParity = 0x20000000u;

constexpr unsigned int OF_WRITE   = 0x01u;
constexpr unsigned int OF_READ    = 0x02u;
constexpr unsigned int OF_TEXT    = 0x04u;
constexpr unsigned int OF_NONCRIT = 0x08u;

constexpr int TICK_PER_SEC = 1;

inline CGString GetMergedFileName( const CGString & base, const char * name )
{
        CGString result( base );
        if ( !result.IsEmpty())
        {
                result += '/';
        }
        if ( name != nullptr )
        {
                result += name;
        }
        return result;
}

class CUID
{
public:
        CUID() : m_Value( 0 ), m_Valid( false ) {}
        explicit CUID( unsigned int value ) : m_Value( value ), m_Valid( true ) {}

        bool IsValidUID() const
        {
                return m_Valid;
        }

        void SetValue( unsigned int value )
        {
                m_Value = value;
                m_Valid = true;
        }

        operator unsigned int() const
        {
                return m_Value;
        }

private:
        unsigned int m_Value;
        bool m_Valid;
};

class CScript
{
public:
        CScript() : m_Open( false ) {}

        bool Open( const char * path, unsigned int flags = 0 )
        {
                m_Path = ( path != nullptr ) ? path : "";
                m_Open = true;

                if (( flags & OF_WRITE ) != 0 )
                {
                        std::ios::openmode mode = std::ios::out | std::ios::trunc;
                        if (( flags & OF_TEXT ) == 0 )
                        {
                                mode |= std::ios::binary;
                        }

                        std::ofstream output( m_Path.c_str(), mode );
                        if ( !output.is_open())
                        {
                                m_Open = false;
                                m_Path.clear();
                                return false;
                        }
                }

                return true;
        }

        bool Open( const char * path, int flags )
        {
                return Open( path, static_cast<unsigned int>( flags ));
        }

        void Close()
        {
                m_Open = false;
        }

        bool FindNextSection()
        {
                return false;
        }

        const char * GetKey() const
        {
                return "";
        }

        const char * GetFilePath() const
        {
                return m_Path.c_str();
        }

        void WriteStr( const char *, ... )
        {
        }

        bool IsOpen() const
        {
                return m_Open;
        }

private:
        std::string m_Path;
        bool m_Open;
};

class CVarDefCont
{
public:
        CVarDefCont() = default;
        CVarDefCont( const CGString & key, const CGString & value ) : m_Key( key ), m_Value( value ) {}

        const TCHAR * GetKey() const
        {
                return (const char *) m_Key;
        }

        const TCHAR * GetValStr() const
        {
                return (const char *) m_Value;
        }

private:
        CGString m_Key;
        CGString m_Value;
};

class CVarDefMap
{
public:
        size_t GetCount() const
        {
                return m_Entries.size();
        }

        const CVarDefCont * GetAt( size_t index ) const
        {
                if ( index >= m_Entries.size())
                {
                        return nullptr;
                }
                return &m_Entries[index];
        }

        void Add( const CGString & key, const CGString & value )
        {
                m_Entries.emplace_back( key, value );
        }

private:
        std::vector<CVarDefCont> m_Entries;
};

struct CPointMap
{
        int m_x;
        int m_y;
        int m_z;

        CPointMap() : m_x( 0 ), m_y( 0 ), m_z( 0 ) {}
        CPointMap( int x, int y, int z ) : m_x( x ), m_y( y ), m_z( z ) {}
};

class CObjBaseTemplate
{
public:
        virtual ~CObjBaseTemplate() = default;
};

class CObjBase : public CObjBaseTemplate
{
public:
        CObjBase() :
                m_UID( 0 ),
                m_BaseID( 0 ),
                m_Top( nullptr ),
                m_Container( nullptr ),
                m_TagDefs( nullptr ),
                m_BaseDefs( nullptr ),
                m_TopPoint(),
                m_ContainedPoint(),
                m_IsTopLevel( false ),
                m_IsInContainer( false )
        {
        }

        virtual bool IsChar() const
        {
                return false;
        }

        virtual bool IsItem() const
        {
                return false;
        }

        void SetUID( unsigned int uid )
        {
                m_UID = uid;
        }

        unsigned int GetUID() const
        {
                return m_UID;
        }

        void SetBaseID( int id )
        {
                m_BaseID = id;
        }

        int GetBaseID() const
        {
                return m_BaseID;
        }

        void SetName( const CGString & name )
        {
                m_Name = name;
        }

        const TCHAR * GetName() const
        {
                return (const char *) m_Name;
        }

        void SetTopLevel( bool value )
        {
                m_IsTopLevel = value;
        }

        bool IsTopLevel() const
        {
                return m_IsTopLevel;
        }

        void SetTopPoint( const CPointMap & pt )
        {
                m_TopPoint = pt;
        }

        const CPointMap & GetTopPoint() const
        {
                return m_TopPoint;
        }

        void SetInContainer( bool value )
        {
                m_IsInContainer = value;
        }

        bool IsInContainer() const
        {
                return m_IsInContainer;
        }

        void SetContainedPoint( const CPointMap & pt )
        {
                m_ContainedPoint = pt;
        }

        const CPointMap & GetContainedPoint() const
        {
                return m_ContainedPoint;
        }

        void SetContainer( const CObjBase * container )
        {
                m_Container = container;
        }

        const CObjBase * GetContainer() const
        {
                return m_Container;
        }

        void SetTopLevelObj( CObjBaseTemplate * top )
        {
                m_Top = top;
        }

        CObjBaseTemplate * GetTopLevelObj() const
        {
                return m_Top;
        }

        void SetTagDefs( const CVarDefMap * defs )
        {
                m_TagDefs = defs;
        }

        const CVarDefMap * GetTagDefs() const
        {
                return m_TagDefs;
        }

        void SetBaseDefs( const CVarDefMap * defs )
        {
                m_BaseDefs = defs;
        }

        const CVarDefMap * GetBaseDefs() const
        {
                return m_BaseDefs;
        }

        virtual void r_Write( CScript & script )
        {
                std::ofstream output( script.GetFilePath(), std::ios::app );
                if ( !output.is_open())
                {
                        return;
                }

                output << "UID=" << m_UID << '\n';
        }

        virtual bool r_Load( CScript & )
        {
                return true;
        }

private:
        unsigned int m_UID;
        int m_BaseID;
        CGString m_Name;
        CObjBaseTemplate * m_Top;
        const CObjBase * m_Container;
        const CVarDefMap * m_TagDefs;
        const CVarDefMap * m_BaseDefs;
        CPointMap m_TopPoint;
        CPointMap m_ContainedPoint;
        bool m_IsTopLevel;
        bool m_IsInContainer;
};

class CAccount
{
public:
        CAccount() :
                m_PrivFlags( 0 ),
                m_PrivLevel( 0 ),
                m_Total_Connect_Time( 0 ),
                m_Last_Connect_Time( 0 ),
                m_iEmailFailures( 0 )
        {
                m_lang[0] = '\0';
                m_lang[1] = '\0';
                m_lang[2] = '\0';
        }

        void SetName( const CGString & name )
        {
                m_sName = name;
        }

        const TCHAR * GetName() const
        {
                return (const char *) m_sName;
        }

        void SetPassword( const CGString & password )
        {
                m_sPassword = password;
        }

        const TCHAR * GetPassword() const
        {
                return (const char *) m_sPassword;
        }

        void SetPrivLevel( int level )
        {
                m_PrivLevel = level;
        }

        int GetPrivLevel() const
        {
                return m_PrivLevel;
        }

        bool IsPriv( unsigned int flag ) const
        {
                return ( m_PrivFlags & flag ) != 0;
        }

        unsigned int m_PrivFlags;
        int m_PrivLevel;
        CGString m_sName;
        CGString m_sPassword;
        CGString m_sComment;
        CGString m_sEMail;
        CGString m_sChatName;
        char m_lang[3];
        int m_Total_Connect_Time;
        int m_Last_Connect_Time;
        CGString m_Last_IP;
        CGString m_First_IP;
        CRealTime m_Last_Connect_Date;
        CRealTime m_First_Connect_Date;
        CUID m_uidLastChar;
        int m_iEmailFailures;
        struct EmailSchedule
        {
                std::vector<WORD> m_Entries;

                size_t GetCount() const
                {
                        return m_Entries.size();
                }

                WORD operator[]( size_t index ) const
                {
                        return m_Entries[index];
                }

                void push_back( WORD value )
                {
                        m_Entries.push_back( value );
                }

                void Clear()
                {
                        m_Entries.clear();
                }
        };
        EmailSchedule m_EMailSchedule;
};

class CPlayer
{
public:
        explicit CPlayer( CAccount * account = nullptr ) : m_pAccount( account ) {}

        CAccount * GetAccount() const
        {
                return m_pAccount;
        }

        void SetAccount( CAccount * account )
        {
                m_pAccount = account;
        }

private:
        CAccount * m_pAccount;
};

class CItem : public CObjBase
{
public:
        CItem() : m_Equipped( false ), m_Next( nullptr ) {}

        bool IsItem() const override
        {
                return true;
        }

        bool IsEquipped() const
        {
                return m_Equipped;
        }

        void SetEquipped( bool equipped )
        {
                m_Equipped = equipped;
        }

        CItem * GetNext() const
        {
                return m_Next;
        }

        void SetNext( CItem * next )
        {
                m_Next = next;
        }

private:
        bool m_Equipped;
        CItem * m_Next;
};

class CContainer
{
public:
        CContainer() : m_ContentHead( nullptr ) {}
        virtual ~CContainer() = default;

        CItem * GetContentHead() const
        {
                return m_ContentHead;
        }

        void AddContent( CItem * item )
        {
                if ( item == nullptr )
                {
                        return;
                }

                item->SetNext( m_ContentHead );
                m_ContentHead = item;

                if ( const CObjBase * owner = dynamic_cast<const CObjBase *>( this ))
                {
                        item->SetContainer( owner );
                        item->SetInContainer( true );
                }
        }

protected:
        CItem * m_ContentHead;
};

class CChar : public CObjBase, public CContainer
{
public:
        CChar() : m_pPlayer( nullptr ) {}

        bool IsChar() const override
        {
                return true;
        }

        void SetPlayer( CPlayer * player )
        {
                m_pPlayer = player;
        }

        bool IsStat( unsigned int ) const
        {
                return false;
        }

        CPlayer * m_pPlayer;
};

class CSector {};
class CGMPage {};
class CServRef {};

#endif // _GRAYSVR_H_
