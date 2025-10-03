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
#include <algorithm>
#include <unordered_map>
#include <cstdlib>
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

using ITEMID_TYPE = unsigned int;
using CREID_TYPE = unsigned int;

class CObjBase;
class CItem;

constexpr int TICK_PER_SEC = 1;
constexpr unsigned int STATF_SaveParity = 0x01u;

inline std::unordered_map<unsigned int, CObjBase*> & StubObjectRegistry()
{
        static std::unordered_map<unsigned int, CObjBase*> s_Registry;
        return s_Registry;
}

inline CObjBase * StubFindObject( unsigned int uid )
{
        auto & registry = StubObjectRegistry();
        auto it = registry.find( uid );
        if ( it == registry.end())
        {
                return nullptr;
        }
        return it->second;
}

inline unsigned int StubParseUnsigned( const char * text )
{
        if ( text == nullptr )
        {
                return 0;
        }
        return static_cast<unsigned int>( std::strtoul( text, nullptr, 0 ));
}

void StubAttachObjectToContainer( CObjBase & object, unsigned int containerUid );

inline CGString GetMergedFileName( const CGString & base, const char * name )
{
        std::string result;
        if ( !base.IsEmpty())
        {
                result = (const char *) base;
                if ( !result.empty() && result.back() != '/' )
                {
                        result.push_back( '/' );
                }
        }
        if ( name != nullptr )
        {
                result += name;
        }

        CGString merged;
        merged = result.c_str();
        return merged;
}

struct StubWorld
{
        bool m_fSaveParity;

        StubWorld() : m_fSaveParity( false ) {}
};

extern StubWorld g_World;

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

constexpr unsigned int PRIV_BLOCKED = 0x1u;
constexpr unsigned int PRIV_JAILED = 0x2u;

constexpr unsigned int OF_WRITE   = 0x01u;
constexpr unsigned int OF_READ    = 0x02u;
constexpr unsigned int OF_TEXT    = 0x04u;
constexpr unsigned int OF_NONCRIT = 0x08u;

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
        CScript() :
                m_Path(),
                m_Open( false ),
                m_Sections(),
                m_CurrentSectionIndex( static_cast<size_t>( -1 )),
                m_CurrentKeyIndex( 0 ),
                m_CurrentKey(),
                m_CurrentArg(),
                m_Mode( Mode::None )
        {
        }

        bool Open( const char * path, unsigned int flags = 0 )
        {
                m_Path = ( path != nullptr ) ? path : "";
                m_Open = true;
                m_Sections.clear();
                m_CurrentSectionIndex = static_cast<size_t>( -1 );
                m_CurrentKeyIndex = 0;
                m_CurrentKey.clear();
                m_CurrentArg.clear();
                m_Mode = Mode::None;

                if (( flags & OF_WRITE ) != 0 )
                {
                        std::ios::openmode mode = std::ios::out | std::ios::trunc;
                        if (( flags & OF_TEXT ) == 0 )
                        {
                                mode |= std::ios::binary;
                        }

                        std::ofstream output( m_Path.c_str(), mode );
                        if ( ! output.is_open())
                        {
                                m_Open = false;
                                m_Path.clear();
                                return false;
                        }
                }

                if (( flags & OF_READ ) != 0 )
                {
                        std::ifstream input( m_Path.c_str(), std::ios::in | std::ios::binary );
                        if ( ! input.is_open())
                        {
                                m_Open = false;
                                m_Path.clear();
                                return false;
                        }

                        Section current;
                        std::string line;
                        while ( std::getline( input, line ))
                        {
                                if ( ! line.empty() && line.back() == '\r' )
                                {
                                        line.pop_back();
                                }

                                std::string trimmed = Trim( line );
                                if ( trimmed.empty())
                                {
                                        continue;
                                }
                                if (( trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']' ))
                                {
                                        if ( ! current.m_Name.empty())
                                        {
                                                m_Sections.push_back( current );
                                        }

                                        current = Section();
                                        std::string inside = Trim( trimmed.substr( 1, trimmed.size() - 2 ));
                                        size_t spacePos = inside.find_first_of( " \t" );
                                        if ( spacePos == std::string::npos )
                                        {
                                                current.m_Name = ToUpper( inside );
                                                current.m_HeaderArg.clear();
                                        }
                                        else
                                        {
                                                current.m_Name = ToUpper( inside.substr( 0, spacePos ));
                                                current.m_HeaderArg = Trim( inside.substr( spacePos + 1 ));
                                        }
                                        continue;
                                }

                                if ( trimmed[0] == ';' || ( trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/' ))
                                {
                                        continue;
                                }

                                if ( current.m_Name.empty())
                                {
                                        continue;
                                }

                                size_t equalsPos = trimmed.find( '=' );
                                std::string key;
                                std::string value;
                                if ( equalsPos == std::string::npos )
                                {
                                        key = Trim( trimmed );
                                        value.clear();
                                }
                                else
                                {
                                        key = Trim( trimmed.substr( 0, equalsPos ));
                                        value = Trim( trimmed.substr( equalsPos + 1 ));
                                }

                                current.m_Entries.emplace_back( ToUpper( key ), value );
                        }

                        if ( ! current.m_Name.empty())
                        {
                                m_Sections.push_back( current );
                        }
                }

                return m_Open;
        }

        bool Open( const char * path, int flags )
        {
                return Open( path, static_cast<unsigned int>( flags ));
        }

        void Close()
        {
                m_Open = false;
                m_Sections.clear();
                m_CurrentSectionIndex = static_cast<size_t>( -1 );
                m_CurrentKeyIndex = 0;
                m_CurrentKey.clear();
                m_CurrentArg.clear();
                m_Mode = Mode::None;
        }

        bool FindNextSection()
        {
                if ( m_Sections.empty())
                {
                                m_Mode = Mode::None;
                                return false;
                }

                if ( m_CurrentSectionIndex == static_cast<size_t>( -1 ))
                {
                        m_CurrentSectionIndex = 0;
                }
                else
                {
                        if ( m_CurrentSectionIndex + 1 >= m_Sections.size())
                        {
                                m_Mode = Mode::None;
                                return false;
                        }
                        ++m_CurrentSectionIndex;
                }

                ResetIteration();
                return true;
        }

        bool IsSectionType( const char * name ) const
        {
                if ( name == nullptr || m_CurrentSectionIndex >= m_Sections.size())
                {
                        return false;
                }

                return m_Sections[m_CurrentSectionIndex].m_Name == ToUpper( std::string( name ));
        }

        const char * GetSection() const
        {
                if ( m_CurrentSectionIndex >= m_Sections.size())
                {
                        return "";
                }
                return m_Sections[m_CurrentSectionIndex].m_Name.c_str();
        }

        const char * GetKey() const
        {
                return m_CurrentKey.c_str();
        }

        const char * GetFilePath() const
        {
                return m_Path.c_str();
        }

        const char * GetArgStr() const
        {
                if ( m_Mode == Mode::Key )
                {
                        return m_CurrentArg.c_str();
                }
                if ( m_CurrentSectionIndex >= m_Sections.size())
                {
                        return "";
                }
                return m_Sections[m_CurrentSectionIndex].m_HeaderArg.c_str();
        }

        long GetArgHex() const
        {
                return static_cast<long>( std::strtoul( GetArgStr(), nullptr, 0 ));
        }

        long GetArgVal() const
        {
                return static_cast<long>( std::strtol( GetArgStr(), nullptr, 0 ));
        }

        bool ReadKey()
        {
                if ( m_CurrentSectionIndex >= m_Sections.size())
                {
                        m_Mode = Mode::Section;
                        return false;
                }

                Section & section = m_Sections[m_CurrentSectionIndex];
                if ( m_CurrentKeyIndex >= section.m_Entries.size())
                {
                        m_Mode = Mode::Section;
                        return false;
                }

                m_CurrentKey = section.m_Entries[m_CurrentKeyIndex].first;
                m_CurrentArg = section.m_Entries[m_CurrentKeyIndex].second;
                ++m_CurrentKeyIndex;
                m_Mode = Mode::Key;
                return true;
        }

        bool ReadKeyParse()
        {
                return ReadKey();
        }

        void WriteStr( const char *, ... )
        {
        }

        bool IsOpen() const
        {
                return m_Open;
        }

private:
        enum class Mode
        {
                None,
                Section,
                Key
        };

        struct Section
        {
                std::string m_Name;
                std::string m_HeaderArg;
                std::vector<std::pair<std::string,std::string>> m_Entries;
        };

        static std::string Trim( const std::string & value )
        {
                size_t start = value.find_first_not_of( " \t\r\n" );
                if ( start == std::string::npos )
                {
                        return std::string();
                }
                size_t end = value.find_last_not_of( " \t\r\n" );
                return value.substr( start, end - start + 1 );
        }

        static std::string ToUpper( const std::string & value )
        {
                std::string result = value;
                std::transform( result.begin(), result.end(), result.begin(), []( unsigned char ch )
                {
                        return static_cast<char>( std::toupper( ch ));
                });
                return result;
        }

        void ResetIteration()
        {
                m_CurrentKeyIndex = 0;
                m_CurrentKey.clear();
                m_CurrentArg.clear();
                m_Mode = Mode::Section;
        }

        std::string m_Path;
        bool m_Open;
        std::vector<Section> m_Sections;
        size_t m_CurrentSectionIndex;
        size_t m_CurrentKeyIndex;
        std::string m_CurrentKey;
        std::string m_CurrentArg;
        Mode m_Mode;
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
                m_IsInContainer( false ),
                m_Deleted( false )
        {
        }

        virtual ~CObjBase()
        {
                StubObjectRegistry().erase( m_UID );
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
                auto & registry = StubObjectRegistry();
                registry.erase( m_UID );
                m_UID = uid;
                if ( uid != 0 )
                {
                        registry[m_UID] = this;
                }
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

        virtual bool r_Load( CScript & script )
        {
                bool fHasContainer = false;
                while ( script.ReadKey())
                {
                        const char * pszKey = script.GetKey();
                        const char * pszArg = script.GetArgStr();
                        if ( pszKey == nullptr )
                        {
                                continue;
                        }

                        if ( !strcasecmp( pszKey, "UID" ))
                        {
                                SetUID( StubParseUnsigned( pszArg ));
                        }
                        else if ( !strcasecmp( pszKey, "NAME" ))
                        {
                                SetName( pszArg );
                        }
                        else if ( !strcasecmp( pszKey, "CONT" ))
                        {
                                StubAttachObjectToContainer( *this, StubParseUnsigned( pszArg ));
                                fHasContainer = true;
                        }
                        else if ( !strcasecmp( pszKey, "P" ))
                        {
                                int x = 0;
                                int y = 0;
                                int z = 0;
                                if ( pszArg != nullptr )
                                {
                                        std::sscanf( pszArg, "%d,%d,%d", &x, &y, &z );
                                        SetTopPoint( CPointMap( x, y, z ));
                                }
                        }
                }

                if ( ! fHasContainer )
                {
                        SetInContainer( false );
                        SetContainer( nullptr );
                        SetTopLevel( true );
                        SetTopLevelObj( this );
                }

                return true;
        }

        virtual void Delete()
        {
                m_Deleted = true;
                StubObjectRegistry().erase( m_UID );
        }

        bool IsDeleted() const
        {
                return m_Deleted;
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
        bool m_Deleted;
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

class CContainer
{
public:
        CContainer();

        CItem * GetContentHead() const
        {
                return m_ContentHead;
        }

        const std::vector<CItem*> & GetContents() const
        {
                return m_Contents;
        }

protected:
        void AppendContent( CItem * item );

private:
        CItem * m_ContentHead;
        std::vector<CItem*> m_Contents;
};

class CChar : public CObjBase, public CContainer
{
public:
        CChar() : CContainer(), m_pPlayer( nullptr ) {}

        bool IsChar() const override
        {
                return true;
        }

        static CChar * CreateBasic( CREID_TYPE )
        {
                return new CChar();
        }

        bool IsStat( unsigned int ) const
        {
                return false;
        }

        void SetPlayer( CPlayer * player )
        {
                m_pPlayer = player;
        }

        CPlayer * m_pPlayer;
};

class CItem : public CObjBase, public CContainer
{
public:
        CItem() : CContainer(), m_Equipped( false ), m_Next( nullptr ) {}

        bool IsItem() const override
        {
                return true;
        }

        static CItem * CreateScript( ITEMID_TYPE )
        {
                return new CItem();
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

        void AddContainedItem( CItem * item )
        {
                AppendContent( item );
        }

private:
        bool m_Equipped;
        CItem * m_Next;
};

inline CContainer::CContainer() : m_ContentHead( nullptr ), m_Contents()
{
}

inline void CContainer::AppendContent( CItem * item )
{
        if ( item == nullptr )
        {
                return;
        }

        item->SetNext( m_ContentHead );
        m_ContentHead = item;
        m_Contents.push_back( item );
}

inline void StubAttachObjectToContainer( CObjBase & object, unsigned int containerUid )
{
        CObjBase * pContainer = StubFindObject( containerUid );
        if ( pContainer == nullptr )
        {
                object.SetContainer( nullptr );
                object.SetInContainer( false );
                return;
        }

        object.SetContainer( pContainer );
        object.SetInContainer( true );
        object.SetTopLevel( false );

        CObjBaseTemplate * pTop = pContainer->GetTopLevelObj();
        if ( pTop == nullptr )
        {
                pTop = pContainer;
        }
        object.SetTopLevelObj( pTop );

        if ( CItem * pContainerItem = dynamic_cast<CItem*>( pContainer ))
        {
                if ( CItem * pItem = dynamic_cast<CItem*>( &object ))
                {
                        pContainerItem->AddContainedItem( pItem );
                }
        }
}

class CSector {};
class CGMPage {};
class CServRef {};

#endif // _GRAYSVR_H_
