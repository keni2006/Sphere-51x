#include "graysvr.h"
#include "CWorldStorageMySQL.h"
#include "CWorldStorageMySQLUtils.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <list>
#include <new>
#include <stdexcept>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#include <errmsg.h>
#include <mysqld_error.h>
#include <mysql.h>
#else
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>
#include <mysql/mysql.h>
#endif

#ifndef MYSQL_OPT_SET_CHARSET_NAME
#define MYSQL_OPT_SET_CHARSET_NAME MYSQL_SET_CHARSET_NAME
#endif

namespace
{
        static const int SCHEMA_VERSION_ROW = 1;      // Stores current schema version
        static const int SCHEMA_IMPORT_ROW = 2;       // Tracks legacy import state
        static const int SCHEMA_WORLD_SAVECOUNT_ROW = 3;
        static const int SCHEMA_WORLD_SAVEFLAG_ROW = 4;
        static const int CURRENT_SCHEMA_VERSION = 3;

        class TempFileGuard
        {
        public:
                TempFileGuard()
                {
                }

                explicit TempFileGuard( std::string path ) :
                        m_Path( std::move( path ))
                {
                }

                ~TempFileGuard()
                {
                        if ( ! m_Path.empty())
                        {
                                ::remove( m_Path.c_str());
                        }
                }

                TempFileGuard( const TempFileGuard & ) = delete;
                TempFileGuard & operator=( const TempFileGuard & ) = delete;

                TempFileGuard( TempFileGuard && other ) noexcept :
                        m_Path( std::move( other.m_Path ))
                {
                        other.m_Path.clear();
                }

                TempFileGuard & operator=( TempFileGuard && other ) noexcept
                {
                        if ( this != &other )
                        {
                                m_Path = std::move( other.m_Path );
                                other.m_Path.clear();
                        }
                        return *this;
                }

                const std::string & GetPath() const
                {
                        return m_Path;
                }

                bool IsValid() const
                {
                        return ! m_Path.empty();
                }

        private:
                std::string m_Path;
        };

bool GenerateTempScriptPath( std::string & outPath )
{
        char szBaseName[L_tmpnam];
        if ( tmpnam( szBaseName ) == NULL )
        {
                return false;
        }

        try
        {
                outPath.assign( szBaseName );
                outPath += ".scp";
        }
        catch ( const std::bad_alloc & )
        {
                return false;
        }

        return true;
}

std::string BuildSetNamesCommand( const std::string & charset, const std::string & collation )
{
        if ( !IsSafeMariaDbIdentifierToken( charset ))
        {
                return std::string();
        }

        std::string command = "SET NAMES '";
        command += charset;
        command += "'";

        if ( !collation.empty() && IsSafeMariaDbIdentifierToken( collation ))
        {
                command += " COLLATE '";
                command += collation;
                command += "'";
        }

        return command;
}

        WORD GetMySQLErrorLogMask( LOGL_TYPE level )
        {
                WORD wMask = static_cast<WORD>( level );
                if ( g_Serv.IsLoading())
                {
                        wMask |= LOGM_INIT;
                }
                return wMask;
        }
}

class MariaDbException : public std::runtime_error
{
public:
        MariaDbException( std::string context, unsigned int code, std::string message ) :
                std::runtime_error( message.empty() ? "Unknown MariaDB client error" : message ),
                m_Context( std::move( context )),
                m_Code( code )
        {
        }

        const std::string & GetContext() const noexcept
        {
                return m_Context;
        }

        unsigned int GetCode() const noexcept
        {
                return m_Code;
        }

private:
        std::string m_Context;
        unsigned int m_Code;
};

class MariaDbResult
{
public:
        using Row = MYSQL_ROW;

        MariaDbResult() noexcept = default;

        explicit MariaDbResult( MYSQL_RES * result ) noexcept :
                m_Result( result )
        {
        }

        MariaDbResult( const MariaDbResult & ) = delete;
        MariaDbResult & operator=( const MariaDbResult & ) = delete;

        MariaDbResult( MariaDbResult && other ) noexcept = default;
        MariaDbResult & operator=( MariaDbResult && other ) noexcept = default;

        ~MariaDbResult()
        {
                Reset();
        }

        void Reset( MYSQL_RES * result = NULL ) noexcept
        {
                m_Result.reset( result );
        }

        bool IsValid() const noexcept
        {
                return m_Result.get() != NULL;
        }

        unsigned int NumFields() const noexcept
        {
                MYSQL_RES * handle = m_Result.get();
                return ( handle != NULL ) ? mysql_num_fields( handle ) : 0;
        }

        Row FetchRow()
        {
                MYSQL_RES * handle = m_Result.get();
                if ( handle == NULL )
                {
                        return NULL;
                }
                return mysql_fetch_row( handle );
        }

private:
        struct MysqlResultDeleter
        {
                void operator()( MYSQL_RES * result ) const noexcept
                {
                        if ( result != NULL )
                        {
                                mysql_free_result( result );
                        }
                }
        };

        std::unique_ptr<MYSQL_RES, MysqlResultDeleter> m_Result;
};

class MariaDbConnection
{
public:
        MariaDbConnection() noexcept = default;

        ~MariaDbConnection()
        {
                Close();
        }

        MariaDbConnection( const MariaDbConnection & ) = delete;
        MariaDbConnection & operator=( const MariaDbConnection & ) = delete;

        void Close() noexcept
        {
                m_Handle.reset();
                m_StringOptions.clear();
        }

        bool IsOpen() const noexcept
        {
                return m_Handle != NULL;
        }

        void Initialize()
        {
                Close();
                MYSQL * handle = mysql_init( NULL );
                if ( handle == NULL )
                {
                        throw MariaDbException( "mysql_init", CR_OUT_OF_MEMORY, "mysql_init failed" );
                }
                m_Handle.reset( handle );
        }

        MYSQL * GetHandle() const noexcept
        {
                return m_Handle.get();
        }

        void SetOption( enum mysql_option option, const void * value )
        {
                MYSQL * handle = RequireHandle( "mysql_options" );
                if ( mysql_options( handle, option, value ) != 0 )
                {
                        throw MariaDbException( "mysql_options", mysql_errno( handle ), LastErrorMessage());
                }
        }

        void SetStringOption( enum mysql_option option, const std::string & value )
        {
                if ( value.empty())
                {
                        return;
                }

                MYSQL * handle = RequireHandle( "mysql_options" );

                m_StringOptions.emplace_back( value );
                const char * pszValue = m_StringOptions.back().c_str();
                if ( mysql_options( handle, option, pszValue ) != 0 )
                {
                        unsigned int code = mysql_errno( handle );
                        std::string message = LastErrorMessage();
                        m_StringOptions.pop_back();
                        throw MariaDbException( "mysql_options", code, message );
                }
        }

        void RealConnect( const char * host, const char * user, const char * password, const char * database, unsigned int port )
        {
                MYSQL * handle = RequireHandle( "mysql_real_connect" );
                MYSQL * result = mysql_real_connect( handle, host, user, password, database, port, NULL, 0 );
                if ( result == NULL )
                {
                        throw MariaDbException( "mysql_real_connect", mysql_errno( handle ), LastErrorMessage());
                }
        }

        void Execute( const CGString & query )
        {
                ExecuteImpl( (const char *) query );
        }

        void Execute( const std::string & query )
        {
                ExecuteImpl( query.c_str());
        }

        MariaDbResult Query( const CGString & query )
        {
                return QueryImpl( (const char *) query );
        }

        MariaDbResult Query( const std::string & query )
        {
                return QueryImpl( query.c_str());
        }

        void BeginTransaction()
        {
                ExecuteImpl( "START TRANSACTION" );
        }

        void Commit()
        {
                ExecuteImpl( "COMMIT" );
        }

        void Rollback()
        {
                ExecuteImpl( "ROLLBACK" );
        }

        void SetCharacterSet( const char * charset )
        {
                if ( charset == NULL || charset[0] == '\0' )
                {
                        return;
                }

                MYSQL * handle = RequireHandle( "mysql_set_character_set" );
                if ( mysql_set_character_set( handle, charset ) != 0 )
                {
                        throw MariaDbException( "mysql_set_character_set", mysql_errno( handle ), LastErrorMessage());
                }
        }

        void ConfigureCharacterSet( const std::string & charset, const std::string & collation )
        {
                if ( charset.empty())
                {
                        return;
                }

                SetCharacterSet( charset.c_str());

                const std::string command = BuildSetNamesCommand( charset, collation );
                if ( !command.empty())
                {
                        Execute( command );
                }
        }

        std::string EscapeString( const char * input, size_t length ) const
        {
                if ( input == NULL )
                {
                        return std::string();
                }

                MYSQL * handle = RequireHandle( "mysql_real_escape_string" );
                std::string buffer;
                buffer.resize( length * 2 + 1 );
                unsigned long written = mysql_real_escape_string( handle, &buffer[0], input, static_cast<unsigned long>( length ));
                buffer.resize( written );
                return buffer;
        }

        const char * CharacterSetName() const noexcept
        {
                MYSQL * handle = m_Handle.get();
                return ( handle != NULL ) ? mysql_character_set_name( handle ) : NULL;
        }

        struct CharacterSetInfo
        {
                std::string m_sCharset;
                std::string m_sCollation;
        };

        CharacterSetInfo GetCharacterSetInfo() const
        {
                CharacterSetInfo info;
                MYSQL * handle = m_Handle.get();
                if ( handle == NULL )
                {
                        return info;
                }

                MY_CHARSET_INFO csInfo;
                mysql_get_character_set_info( handle, &csInfo );
                if ( csInfo.csname != NULL )
                {
                        info.m_sCharset = csInfo.csname;
                }
                if ( csInfo.name != NULL )
                {
                        info.m_sCollation = csInfo.name;
                }
                return info;
        }

        unsigned int LastErrorCode() const noexcept
        {
                MYSQL * handle = m_Handle.get();
                return ( handle != NULL ) ? mysql_errno( handle ) : 0;
        }

        std::string LastErrorMessage() const
        {
                MYSQL * handle = m_Handle.get();
                if ( handle == NULL )
                {
                        return "No active MariaDB connection";
                }

                const char * pszError = mysql_error( handle );
                if ( pszError == NULL || pszError[0] == '\0' )
                {
                        return "Unknown MariaDB client error";
                }
                return pszError;
        }

private:
        MYSQL * RequireHandle( const char * context ) const
        {
                MYSQL * handle = m_Handle.get();
                if ( handle == NULL )
                {
                        throw MariaDbException( context, 0, "No active MariaDB connection" );
                }
                return handle;
        }

        void ExecuteImpl( const char * pszQuery )
        {
                MYSQL * handle = RequireHandle( "mysql_query" );

                if ( mysql_query( handle, pszQuery ) != 0 )
                {
                        throw MariaDbException( "mysql_query", mysql_errno( handle ), LastErrorMessage());
                }

                if ( mysql_field_count( handle ) != 0 )
                {
                        MYSQL_RES * result = mysql_store_result( handle );
                        if ( result != NULL )
                        {
                                mysql_free_result( result );
                        }
                        else if ( mysql_errno( handle ) != 0 )
                        {
                                throw MariaDbException( "mysql_store_result", mysql_errno( handle ), LastErrorMessage());
                        }
                }
        }

        MariaDbResult QueryImpl( const char * pszQuery )
        {
                MYSQL * handle = RequireHandle( "mysql_query" );

                if ( mysql_query( handle, pszQuery ) != 0 )
                {
                        throw MariaDbException( "mysql_query", mysql_errno( handle ), LastErrorMessage());
                }

                MYSQL_RES * result = mysql_store_result( handle );
                if ( result == NULL && mysql_errno( handle ) != 0 )
                {
                        throw MariaDbException( "mysql_store_result", mysql_errno( handle ), LastErrorMessage());
                }
                return MariaDbResult( result );
        }

        struct MysqlHandleDeleter
        {
                void operator()( MYSQL * handle ) const noexcept
                {
                        if ( handle != NULL )
                        {
                                mysql_close( handle );
                        }
                }
        };

        std::unique_ptr<MYSQL, MysqlHandleDeleter> m_Handle;
        std::list<std::string> m_StringOptions;
};

void LogMariaDbException( const MariaDbException & ex, LOGL_TYPE level )
{
        g_Log.Event( GetMySQLErrorLogMask( level ), "MySQL %s error (%u): %s\n", ex.GetContext().c_str(), ex.GetCode(), ex.what());
}

CWorldStorageMySQL::Transaction::Transaction( CWorldStorageMySQL & storage, bool fAutoBegin ) :
        m_Storage( storage ),
        m_fActive( false ),
        m_fCommitted( false )
{
        if ( fAutoBegin )
        {
                m_fActive = m_Storage.BeginTransaction();
        }
}

CWorldStorageMySQL::Transaction::~Transaction()
{
        if ( m_fActive && ! m_fCommitted )
        {
                m_Storage.RollbackTransaction();
        }
}

bool CWorldStorageMySQL::Transaction::Begin()
{
        if ( m_fActive )
        {
                return true;
        }
        m_fActive = m_Storage.BeginTransaction();
        return m_fActive;
}

bool CWorldStorageMySQL::Transaction::Commit()
{
        if ( ! m_fActive )
        {
                return false;
        }
        if ( ! m_Storage.CommitTransaction())
        {
                return false;
        }
        m_fCommitted = true;
        m_fActive = false;
        return true;
}

void CWorldStorageMySQL::Transaction::Rollback()
{
        if ( m_fActive )
        {
                m_Storage.RollbackTransaction();
                m_fActive = false;
        }
        m_fCommitted = false;
}

CWorldStorageMySQL::UniversalRecord::UniversalRecord( CWorldStorageMySQL & storage ) :
        m_Storage( storage )
{
}

CWorldStorageMySQL::UniversalRecord::UniversalRecord( CWorldStorageMySQL & storage, const CGString & table ) :
        m_Storage( storage ),
        m_sTable( table )
{
}

void CWorldStorageMySQL::UniversalRecord::SetTable( const CGString & table )
{
        m_sTable = table;
}

void CWorldStorageMySQL::UniversalRecord::Clear()
{
        m_Fields.clear();
}

bool CWorldStorageMySQL::UniversalRecord::Empty() const
{
        return m_Fields.empty();
}

CWorldStorageMySQL::UniversalRecord::FieldEntry * CWorldStorageMySQL::UniversalRecord::FindField( const char * field )
{
        if ( field == NULL )
        {
                return NULL;
        }
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( m_Fields[i].m_sName.CompareNoCase( field ) == 0 )
                {
                        return &m_Fields[i];
                }
        }
        return NULL;
}

const CWorldStorageMySQL::UniversalRecord::FieldEntry * CWorldStorageMySQL::UniversalRecord::FindField( const char * field ) const
{
        if ( field == NULL )
        {
                return NULL;
        }
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( m_Fields[i].m_sName.CompareNoCase( field ) == 0 )
                {
                        return &m_Fields[i];
                }
        }
        return NULL;
}

void CWorldStorageMySQL::UniversalRecord::AddOrReplaceField( const char * field, const CGString & value )
{
        if ( field == NULL )
        {
                return;
        }
        FieldEntry * pEntry = FindField( field );
        if ( pEntry != NULL )
        {
                pEntry->m_sValue = value;
                return;
        }

        FieldEntry entry;
        entry.m_sName = field;
        entry.m_sValue = value;
        m_Fields.push_back( entry );
}

void CWorldStorageMySQL::UniversalRecord::SetRaw( const char * field, const CGString & expression )
{
        AddOrReplaceField( field, expression );
}

void CWorldStorageMySQL::UniversalRecord::SetNull( const char * field )
{
        AddOrReplaceField( field, CGString( "NULL" ));
}

void CWorldStorageMySQL::UniversalRecord::SetString( const char * field, const CGString & value )
{
        AddOrReplaceField( field, m_Storage.FormatStringValue( value ));
}

void CWorldStorageMySQL::UniversalRecord::SetOptionalString( const char * field, const CGString & value )
{
        AddOrReplaceField( field, m_Storage.FormatOptionalStringValue( value ));
}

void CWorldStorageMySQL::UniversalRecord::SetDateTime( const char * field, const CGString & value )
{
        AddOrReplaceField( field, m_Storage.FormatDateTimeValue( value ));
}

void CWorldStorageMySQL::UniversalRecord::SetDateTime( const char * field, const CRealTime & value )
{
        AddOrReplaceField( field, m_Storage.FormatDateTimeValue( value ));
}

void CWorldStorageMySQL::UniversalRecord::SetInt( const char * field, long long value )
{
        CGString sValue;
#ifdef _WIN32
        sValue.Format( "%I64d", value );
#else
        sValue.Format( "%lld", value );
#endif
        AddOrReplaceField( field, sValue );
}

void CWorldStorageMySQL::UniversalRecord::SetUInt( const char * field, unsigned long long value )
{
        CGString sValue;
#ifdef _WIN32
        sValue.Format( "%I64u", value );
#else
        sValue.Format( "%llu", value );
#endif
        AddOrReplaceField( field, sValue );
}

void CWorldStorageMySQL::UniversalRecord::SetBool( const char * field, bool value )
{
        AddOrReplaceField( field, CGString( value ? "1" : "0" ));
}

CGString CWorldStorageMySQL::UniversalRecord::BuildInsert( bool fReplace, bool fUpdateOnDuplicate ) const
{
        CGString sQuery;
        if ( m_sTable.IsEmpty() || m_Fields.empty())
        {
                return sQuery;
        }

        CGString sColumns;
        CGString sValues;
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( ! sColumns.IsEmpty())
                {
                        sColumns += ",";
                        sValues += ",";
                }
                CGString sColumn;
                sColumn.Format( "`%s`", (const char *) m_Fields[i].m_sName );
                sColumns += sColumn;
                sValues += m_Fields[i].m_sValue;
        }

        const char * pszVerb = fReplace ? "REPLACE INTO" : "INSERT INTO";
        sQuery.Format( "%s `%s` (%s) VALUES (%s)", pszVerb, (const char *) m_sTable, (const char *) sColumns, (const char *) sValues );

        if ( ! fReplace && fUpdateOnDuplicate && ! m_Fields.empty())
        {
                sQuery += " ON DUPLICATE KEY UPDATE ";
                for ( size_t i = 0; i < m_Fields.size(); ++i )
                {
                        if ( i > 0 )
                        {
                                sQuery += ",";
                        }
                        CGString sUpdate;
                        sUpdate.Format( "`%s`=VALUES(`%s`)", (const char *) m_Fields[i].m_sName, (const char *) m_Fields[i].m_sName );
                        sQuery += sUpdate;
                }
        }

        sQuery += ";";
        return sQuery;
}

CGString CWorldStorageMySQL::UniversalRecord::BuildUpdate( const CGString & whereClause ) const
{
        CGString sQuery;
        if ( m_sTable.IsEmpty() || m_Fields.empty())
        {
                return sQuery;
        }

        CGString sAssignments;
        for ( size_t i = 0; i < m_Fields.size(); ++i )
        {
                if ( ! sAssignments.IsEmpty())
                {
                        sAssignments += ",";
                }
                CGString sAssign;
                sAssign.Format( "`%s`=%s", (const char *) m_Fields[i].m_sName, (const char *) m_Fields[i].m_sValue );
                sAssignments += sAssign;
        }

        sQuery.Format( "UPDATE `%s` SET %s", (const char *) m_sTable, (const char *) sAssignments );
        if ( ! whereClause.IsEmpty())
        {
                sQuery += " ";
                sQuery += whereClause;
        }
        sQuery += ";";
        return sQuery;
}

CWorldStorageMySQL::CWorldStorageMySQL()
{
	m_pConnection.reset();
	m_fAutoReconnect = false;
	m_iReconnectTries = 0;
	m_iReconnectDelay = 0;
	m_sDatabaseName.Empty();
	m_sTablePrefix.Empty();
	m_fLoggedInvalidTablePrefix = false;
	m_sTableCharset.Empty();
        m_sTableCollation.Empty();
        m_tLastAccountSync = 0;
        m_iTransactionDepth = 0;
        m_fDirtyThreadStop = false;
        m_fDirtyThreadRunning = false;
}

CWorldStorageMySQL::~CWorldStorageMySQL()
{
	Disconnect();
}

bool CWorldStorageMySQL::Connect( const CServerMySQLConfig & config )
{
        Disconnect();

        if ( ! config.m_fEnable )
        {
                return false;
        }

        m_fLoggedInvalidTablePrefix = false;

        std::string normalizedPrefix;
        std::string prefixError;
        const char * rawPrefix = config.m_sTablePrefix.IsEmpty() ? NULL : (const char *) config.m_sTablePrefix;
        NormalizeMySQLTablePrefix( rawPrefix, normalizedPrefix, &prefixError, true );

        if ( !prefixError.empty())
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "%s", prefixError.c_str());
        }

        if ( normalizedPrefix.empty())
        {
                m_sTablePrefix.Empty();
        }
        else
        {
                m_sTablePrefix = normalizedPrefix.c_str();
        }

        m_fAutoReconnect = config.m_fAutoReconnect;
        m_iReconnectTries = config.m_iReconnectTries;
        m_iReconnectDelay = config.m_iReconnectDelay;
        m_sDatabaseName = config.m_sDatabase;
        m_sTableCharset.Empty();
        m_sTableCollation.Empty();
        m_tLastAccountSync = 0;
        m_iTransactionDepth = 0;

        const char * whitespace = " \t\n\r\f\v";

        auto trimWhitespace = [whitespace]( std::string & value )
        {
                size_t begin = value.find_first_not_of( whitespace );
                if ( begin == std::string::npos )
                {
                        value.clear();
                        return;
                }
                size_t endPos = value.find_last_not_of( whitespace );
                value = value.substr( begin, endPos - begin + 1 );
        };

        std::string requestedCharset;
        std::string requestedCollation;
        if ( !config.m_sCharset.IsEmpty())
        {
                requestedCharset = (const char *) config.m_sCharset;
                trimWhitespace( requestedCharset );
                size_t separator = requestedCharset.find_first_of( whitespace );
                if ( separator != std::string::npos )
                {
                        requestedCollation = requestedCharset.substr( separator + 1 );
                        requestedCharset.erase( separator );
                        trimWhitespace( requestedCollation );
                }
                trimWhitespace( requestedCharset );
        }

        std::string sessionCharset = requestedCharset;
        std::string sessionCollation = requestedCollation;

        auto toLowerAscii = []( std::string & value )
        {
                std::transform( value.begin(), value.end(), value.begin(), []( unsigned char ch ) -> char
                {
                        return static_cast<char>( std::tolower( ch ));
                });
        };

        toLowerAscii( sessionCharset );
        toLowerAscii( sessionCollation );

        std::string derivedCharset;
        if ( !sessionCollation.empty())
        {
                size_t underscore = sessionCollation.find( '_' );
                if ( underscore != std::string::npos )
                {
                        derivedCharset = sessionCollation.substr( 0, underscore );
                }
        }

        if ( sessionCharset.empty() && !derivedCharset.empty())
        {
                sessionCharset = derivedCharset;
        }
        else if ( !sessionCharset.empty() && !derivedCharset.empty() && sessionCharset != derivedCharset )
        {
                g_Log.Event( LOGM_INIT|LOGL_WARN,
                        "Requested charset '%s' does not match collation '%s'; using charset '%s'.",
                        requestedCharset.empty() ? "(auto)" : requestedCharset.c_str(),
                        requestedCollation.empty() ? "(none)" : requestedCollation.c_str(),
                        derivedCharset.c_str());
                sessionCharset = derivedCharset;
        }

        if ( sessionCharset.empty())
        {
                sessionCharset = "utf8mb4";
        }

        if ( !sessionCharset.empty() && !IsSafeMariaDbIdentifierToken( sessionCharset ))
        {
                std::string invalidCharset = sessionCharset;
                sessionCharset = "utf8mb4";
                g_Log.Event( LOGM_INIT|LOGL_WARN, "Invalid charset token '%s'; forcing 'utf8mb4'.", invalidCharset.c_str());
        }

        if ( !sessionCollation.empty() && !IsSafeMariaDbIdentifierToken( sessionCollation ))
        {
                std::string invalidCollation = sessionCollation;
                sessionCollation.clear();
                g_Log.Event( LOGM_INIT|LOGL_WARN, "Invalid collation token '%s'; ignoring requested value.", invalidCollation.c_str());
        }

        const std::string setNamesCommand = BuildSetNamesCommand( sessionCharset, sessionCollation );

        const char * pszHost = config.m_sHost.IsEmpty() ? NULL : (const char *) config.m_sHost;
        const char * pszUser = config.m_sUser.IsEmpty() ? NULL : (const char *) config.m_sUser;
        const char * pszPassword = config.m_sPassword.IsEmpty() ? NULL : (const char *) config.m_sPassword;
        const char * pszDatabase = config.m_sDatabase.IsEmpty() ? NULL : (const char *) config.m_sDatabase;
        unsigned int uiPort = config.m_iPort > 0 ? static_cast<unsigned int>( config.m_iPort ) : 0;

        const int attempts = std::max( m_fAutoReconnect ? m_iReconnectTries : 1, 1 );

        for ( int attempt = 0; attempt < attempts; ++attempt )
        {
                try
                {
                        std::unique_ptr<MariaDbConnection> connection( new MariaDbConnection() );
                        connection->Initialize();

                        if ( m_fAutoReconnect )
                        {
                                char reconnect = 1;
                                connection->SetOption( MYSQL_OPT_RECONNECT, &reconnect );
                        }

                        if ( !sessionCharset.empty())
                        {
                                connection->SetStringOption( MYSQL_OPT_SET_CHARSET_NAME, sessionCharset );
                        }

                        if ( !setNamesCommand.empty())
                        {
                                connection->SetStringOption( MYSQL_INIT_COMMAND, setNamesCommand );
                        }

                        connection->RealConnect( pszHost, pszUser, pszPassword, pszDatabase, uiPort );

                        try
                        {
                                connection->ConfigureCharacterSet( sessionCharset, sessionCollation );
                        }
                        catch ( const MariaDbException & ex )
                        {
                                LogMariaDbException( ex, LOGL_WARN );
                        }

                        MariaDbConnection::CharacterSetInfo activeInfo = connection->GetCharacterSetInfo();
                        if ( !sessionCharset.empty())
                        {
                                m_sTableCharset = sessionCharset.c_str();
                        }
                        else if ( !activeInfo.m_sCharset.empty())
                        {
                                m_sTableCharset = activeInfo.m_sCharset.c_str();
                        }
                        else
                        {
                                m_sTableCharset.Empty();
                        }

                        if ( !sessionCollation.empty())
                        {
                                m_sTableCollation = sessionCollation.c_str();
                        }
                        else if ( !activeInfo.m_sCollation.empty())
                        {
                                m_sTableCollation = activeInfo.m_sCollation.c_str();
                        }
                        else
                        {
                                m_sTableCollation.Empty();
                        }

                        const char * pszActiveCharset = m_sTableCharset.IsEmpty() ? "unknown" : (const char *) m_sTableCharset;
                        CGString sCollationSuffix;
                        if ( !m_sTableCollation.IsEmpty())
                        {
                                sCollationSuffix.Format( ", collation '%s'", (const char *) m_sTableCollation );
                        }

                        m_pConnection = std::move( connection );

                        g_Log.Event( LOGM_INIT|LOGL_EVENT, "Connected to MySQL server %s:%u using character set '%s'%s.",
                                ( pszHost != NULL && pszHost[0] != '\0' ) ? pszHost : "localhost",
                                uiPort,
                                pszActiveCharset,
                                (const char *) sCollationSuffix );
                        StartDirtyWorker();
                        return true;
                }
                catch ( const MariaDbException & ex )
                {
                        LogMariaDbException( ex, LOGL_ERROR );
                        if ( ( attempt + 1 ) < attempts )
                        {
                                int delay = std::max( m_iReconnectDelay, 1 );
                                g_Log.Event( LOGM_INIT|LOGL_WARN, "Retrying MySQL connection in %d second(s).", delay );
                                std::this_thread::sleep_for( std::chrono::seconds( delay ));
                        }
                }
                catch ( const std::bad_alloc & )
                {
                        g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "Out of memory while preparing MySQL connection." );
                        break;
                }
        }

        g_Log.Event( LOGM_INIT|LOGL_ERROR, "Unable to connect to MySQL server after %d attempt(s).", std::max( attempts, 1 ) );
        return false;
}

void CWorldStorageMySQL::Disconnect()
{
        StopDirtyWorker();
        m_pConnection.reset();

        m_sTablePrefix.Empty();
        m_fLoggedInvalidTablePrefix = false;
        m_sDatabaseName.Empty();
        m_sTableCharset.Empty();
        m_sTableCollation.Empty();
        m_fAutoReconnect = false;
        m_iReconnectTries = 0;
        m_iReconnectDelay = 0;
        m_tLastAccountSync = 0;
        m_iTransactionDepth = 0;
}

bool CWorldStorageMySQL::IsConnected() const
{
        return m_pConnection && m_pConnection->IsOpen();
}

bool CWorldStorageMySQL::IsEnabled() const
{
        return IsConnected();
}

CGString CWorldStorageMySQL::GetPrefixedTableName( const char * name ) const
{
        CGString sName;
        if ( name == NULL )
        {
                return sName;
        }

        std::string prefixedName;
        std::string errorMessage;
        const char * rawPrefix = m_sTablePrefix.IsEmpty() ? NULL : (const char *) m_sTablePrefix;
        if ( !BuildMySQLPrefixedTableName( rawPrefix, name, prefixedName, &errorMessage ))
        {
                if ( !m_fLoggedInvalidTablePrefix )
                {
                        if ( errorMessage.empty())
                        {
                                g_Log.Event( LOGM_INIT|LOGL_ERROR, "MySQL table prefix validation failed while building table name; ignoring configured prefix." );
                        }
                        else
                        {
                                g_Log.Event( LOGM_INIT|LOGL_ERROR, "%s Ignoring configured MySQL table prefix.", errorMessage.c_str());
                        }
                        m_fLoggedInvalidTablePrefix = true;
                }

                prefixedName.assign( name );
        }

        if ( prefixedName.empty())
        {
                prefixedName.assign( name );
        }

        sName = prefixedName.c_str();
        return sName;
}

const char * CWorldStorageMySQL::GetDefaultTableCharset() const
{
        if ( ! m_sTableCharset.IsEmpty())
        {
                return m_sTableCharset;
        }
        return "utf8mb4";
}

const char * CWorldStorageMySQL::GetDefaultTableCollation() const
{
        if ( ! m_sTableCollation.IsEmpty())
        {
                return m_sTableCollation;
        }
        return NULL;
}

CGString CWorldStorageMySQL::GetDefaultTableCollationSuffix() const
{
        CGString sSuffix;
        const char * pszCollation = GetDefaultTableCollation();
        if ( pszCollation != NULL && pszCollation[0] != '\0' )
        {
                sSuffix.Format( " COLLATE=%s", pszCollation );
        }
        return sSuffix;
}

bool CWorldStorageMySQL::Query( const CGString & query, MariaDbResult * pResult )
{
        if ( ! IsConnected())
        {
                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "MySQL query attempted without an active connection." );
                return false;
        }

        try
        {
                if ( pResult != NULL )
                {
                        *pResult = m_pConnection->Query( query );
                }
                else
                {
                        m_pConnection->Execute( query );
                }
                return true;
        }
        catch ( const MariaDbException & ex )
        {
                LogMariaDbException( ex, LOGL_ERROR );
                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "Failed query: %s", (const char *) query );
        }
        return false;
}

bool CWorldStorageMySQL::ExecuteQuery( const CGString & query )
{
        return Query( query, NULL );
}

bool CWorldStorageMySQL::ExecuteRecordsInsert( const std::vector<UniversalRecord> & records )
{
        if ( records.empty())
        {
                return true;
        }

        for ( size_t i = 0; i < records.size(); ++i )
        {
                CGString sQuery = records[i].BuildInsert( false, false );
                if ( sQuery.IsEmpty())
                {
                        return false;
                }
                if ( ! ExecuteQuery( sQuery ))
                {
                        return false;
                }
        }

        return true;
}

bool CWorldStorageMySQL::ClearTable( const CGString & table )
{
        if ( table.IsEmpty())
        {
                return false;
        }

        CGString sQuery;
        sQuery.Format( "DELETE FROM `%s`;", (const char *) table );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::BeginTransaction()
{
        if ( ! IsConnected())
        {
                return false;
        }

        if ( m_iTransactionDepth == 0 )
        {
                try
                {
                        m_pConnection->BeginTransaction();
                }
                catch ( const MariaDbException & ex )
                {
                        LogMariaDbException( ex, LOGL_ERROR );
                        return false;
                }
        }

        ++m_iTransactionDepth;
        return true;
}

bool CWorldStorageMySQL::CommitTransaction()
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( m_iTransactionDepth <= 0 )
        {
                return false;
        }

        --m_iTransactionDepth;
        if ( m_iTransactionDepth == 0 )
        {
                try
                {
                        m_pConnection->Commit();
                }
                catch ( const MariaDbException & ex )
                {
                        LogMariaDbException( ex, LOGL_ERROR );
                        m_iTransactionDepth = 0;
                        return false;
                }
        }
        return true;
}

bool CWorldStorageMySQL::RollbackTransaction()
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( m_iTransactionDepth <= 0 )
        {
                return true;
        }

        try
        {
                m_pConnection->Rollback();
        }
        catch ( const MariaDbException & ex )
        {
                LogMariaDbException( ex, LOGL_ERROR );
                m_iTransactionDepth = 0;
                return false;
        }

        m_iTransactionDepth = 0;
        return true;
}

bool CWorldStorageMySQL::WithTransaction( const std::function<bool()> & callback )
{
        if ( ! callback )
        {
                return false;
        }

        Transaction transaction( *this );
        if ( ! transaction.IsActive())
        {
                if ( ! transaction.Begin())
                {
                        return false;
                }
        }

        bool fResult = false;
        try
        {
                fResult = callback();
        }
        catch (...)
        {
                transaction.Rollback();
                throw;
        }

        if ( fResult )
        {
                if ( transaction.Commit())
                {
                        return true;
                }
                transaction.Rollback();
                return false;
        }

        transaction.Rollback();
        return false;
}

bool CWorldStorageMySQL::EnsureSchemaVersionTable()
{
        const CGString sTableName = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        CGString sCollationSuffix = GetDefaultTableCollationSuffix();
        sQuery.Format(
"CREATE TABLE IF NOT EXISTS `%s` ("
"`id` INT NOT NULL,"
"`version` INT NOT NULL,"
"PRIMARY KEY (`id`)"
") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
(const char *) sTableName,
GetDefaultTableCharset(),
(const char *) sCollationSuffix);
	if ( ! ExecuteQuery( sQuery ))
	{
		return false;
	}

	sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
		(const char *) sTableName, SCHEMA_VERSION_ROW );
	if ( ! ExecuteQuery( sQuery ))
	{
		return false;
	}

	sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
		(const char *) sTableName, SCHEMA_IMPORT_ROW );
	if ( ! ExecuteQuery( sQuery ))
	{
		return false;
	}

	return true;
}

int CWorldStorageMySQL::GetSchemaVersion()
{
	const CGString sTableName = GetPrefixedTableName( "schema_version" );

	CGString sQuery;
	sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;",
		(const char *) sTableName, SCHEMA_VERSION_ROW );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return -1;
        }

        int iVersion = 0;
        if ( result.IsValid())
        {
                MariaDbResult::Row pRow = result.FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        iVersion = atoi( pRow[0] );
                }
        }

	return iVersion;
}

bool CWorldStorageMySQL::SetSchemaVersion( int version )
{
	const CGString sTableName = GetPrefixedTableName( "schema_version" );

	CGString sQuery;
	sQuery.Format( "UPDATE `%s` SET `version` = %d WHERE `id` = %d;",
		(const char *) sTableName, version, SCHEMA_VERSION_ROW );
	return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::ApplyMigration_0_1()
{
        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        const CGString sAccountEmails = GetPrefixedTableName( "account_emails" );
        const CGString sCharacters = GetPrefixedTableName( "characters" );
	const CGString sItems = GetPrefixedTableName( "items" );
	const CGString sItemProps = GetPrefixedTableName( "item_props" );
	const CGString sSectors = GetPrefixedTableName( "sectors" );
	const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
	const CGString sServers = GetPrefixedTableName( "servers" );
	const CGString sTimers = GetPrefixedTableName( "timers" );

	std::vector<CGString> vQueries;
	CGString sQuery;
	CGString sCollationSuffix = GetDefaultTableCollationSuffix();

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`name` VARCHAR(32) NOT NULL,"
                "`password` VARCHAR(64) NOT NULL,"
                "`plevel` INT NOT NULL DEFAULT 0,"
                "`priv_flags` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`status` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`comment` TEXT NULL,"
                "`email` VARCHAR(128) NULL,"
                "`chat_name` VARCHAR(64) NULL,"
                "`language` CHAR(3) NULL,"
                "`total_connect_time` INT NOT NULL DEFAULT 0,"
                "`last_connect_time` INT NOT NULL DEFAULT 0,"
                "`last_ip` VARCHAR(45) NULL,"
                "`last_login` DATETIME NULL,"
                "`first_ip` VARCHAR(45) NULL,"
                "`first_login` DATETIME NULL,"
                "`last_char_uid` BIGINT UNSIGNED NULL,"
                "`email_failures` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_accounts_name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sAccounts,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`account_id` INT UNSIGNED NOT NULL,"
                "`sequence` SMALLINT UNSIGNED NOT NULL,"
                "`message_id` SMALLINT UNSIGNED NOT NULL,"
                "PRIMARY KEY (`account_id`, `sequence`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sAccountEmails, (const char *) sAccounts, GetDefaultTableCharset(), (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`uid` BIGINT UNSIGNED NOT NULL,"
                "`account_id` INT UNSIGNED NULL,"
                "`name` VARCHAR(64) NULL,"
                "`body_id` INT NULL,"
                "`position_x` INT NULL,"
                "`position_y` INT NULL,"
                "`position_z` INT NULL,"
                "`map_plane` INT NULL,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`uid`),"
                "KEY `ix_characters_account` (`account_id`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sCharacters, (const char *) sAccounts, GetDefaultTableCharset(), (const char *) sCollationSuffix);
	vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`uid` BIGINT UNSIGNED NOT NULL,"
                "`container_uid` BIGINT UNSIGNED NULL,"
                "`owner_uid` BIGINT UNSIGNED NULL,"
                "`type` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`amount` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`color` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`position_x` INT NULL,"
                "`position_y` INT NULL,"
                "`position_z` INT NULL,"
                "`map_plane` INT NULL,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`uid`),"
                "KEY `ix_items_container` (`container_uid`),"
                "KEY `ix_items_owner` (`owner_uid`),"
                "FOREIGN KEY (`container_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,"
                "FOREIGN KEY (`owner_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sItems, (const char *) sItems, (const char *) sCharacters, GetDefaultTableCharset(), (const char *) sCollationSuffix);
	vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`item_uid` BIGINT UNSIGNED NOT NULL,"
                "`prop` VARCHAR(64) NOT NULL,"
                "`value` TEXT NULL,"
                "PRIMARY KEY (`item_uid`, `prop`),"
                "FOREIGN KEY (`item_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sItemProps, (const char *) sItems, GetDefaultTableCharset(), (const char *) sCollationSuffix);
	vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`map_plane` INT NOT NULL,"
                "`x1` INT NOT NULL,"
                "`y1` INT NOT NULL,"
                "`x2` INT NOT NULL,"
                "`y2` INT NOT NULL,"
                "`last_update` DATETIME NULL,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_sectors_bounds` (`map_plane`, `x1`, `y1`, `x2`, `y2`)",
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sSectors, GetDefaultTableCharset(), (const char *) sCollationSuffix);
	vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`account_id` INT UNSIGNED NULL,"
                "`character_uid` BIGINT UNSIGNED NULL,"
                "`reason` TEXT NULL,"
                "`status` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`id`),"
                "KEY `ix_gm_pages_account` (`account_id`),"
                "KEY `ix_gm_pages_character` (`character_uid`),"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL,"
                "FOREIGN KEY (`character_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sGMPages, (const char *) sAccounts, (const char *) sCharacters, GetDefaultTableCharset(), (const char *) sCollationSuffix);
	vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`name` VARCHAR(64) NOT NULL,"
                "`address` VARCHAR(128) NULL,"
                "`port` INT NULL,"
                "`status` INT UNSIGNED NOT NULL DEFAULT 0,"
                "`last_seen` DATETIME NULL,"
                "PRIMARY KEY (`id`),"
                "UNIQUE KEY `ux_servers_name` (`name`)"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sServers, GetDefaultTableCharset(), (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` ("
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                "`character_uid` BIGINT UNSIGNED NULL,"
                "`item_uid` BIGINT UNSIGNED NULL,"
                "`expires_at` BIGINT NOT NULL,"
                "`type` INT UNSIGNED NOT NULL,"
                "`data` TEXT NULL,"
                "PRIMARY KEY (`id`),"
                "KEY `ix_timers_character` (`character_uid`),"
                "KEY `ix_timers_item` (`item_uid`),"
                "FOREIGN KEY (`character_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,"
                "FOREIGN KEY (`item_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sTimers, (const char *) sCharacters, (const char *) sItems, GetDefaultTableCharset(), (const char *) sCollationSuffix);
	vQueries.push_back( sQuery );

	for ( size_t i = 0; i < vQueries.size(); ++i )
	{
		if ( ! ExecuteQuery( vQueries[i] ))
		{
			return false;
		}
        }

        return true;
}

bool CWorldStorageMySQL::ApplyMigration_1_2()
{
        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        const CGString sAccountEmails = GetPrefixedTableName( "account_emails" );
        const CGString sTableName = GetPrefixedTableName( "schema_version" );

        if ( ! EnsureColumnExists( sAccounts, "priv_flags", "`priv_flags` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `plevel`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "comment", "`comment` TEXT NULL AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "chat_name", "`chat_name` VARCHAR(64) NULL AFTER `email`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "language", "`language` CHAR(3) NULL AFTER `chat_name`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "total_connect_time", "`total_connect_time` INT NOT NULL DEFAULT 0 AFTER `language`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "last_connect_time", "`last_connect_time` INT NOT NULL DEFAULT 0 AFTER `total_connect_time`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "first_ip", "`first_ip` VARCHAR(45) NULL AFTER `last_ip`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "first_login", "`first_login` DATETIME NULL AFTER `first_ip`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "last_char_uid", "`last_char_uid` BIGINT UNSIGNED NULL AFTER `first_login`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "email_failures", "`email_failures` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `last_char_uid`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sAccounts, "updated_at", "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP AFTER `created_at`" ))
        {
                return false;
        }

        CGString sQuery;
        CGString sCollationSuffix = GetDefaultTableCollationSuffix();
        sQuery.Format(
"CREATE TABLE IF NOT EXISTS `%s` ("
"`account_id` INT UNSIGNED NOT NULL,"
"`sequence` SMALLINT UNSIGNED NOT NULL,"
"`message_id` SMALLINT UNSIGNED NOT NULL,"
"PRIMARY KEY (`account_id`, `sequence`),"
"FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE CASCADE"
") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
(const char *) sAccountEmails, (const char *) sAccounts, GetDefaultTableCharset(), (const char *) sCollationSuffix);
        if ( ! ExecuteQuery( sQuery ))
        {
                return false;
        }

        sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
                (const char *) sTableName, SCHEMA_WORLD_SAVECOUNT_ROW );
        if ( ! ExecuteQuery( sQuery ))
        {
                return false;
        }

        sQuery.Format( "INSERT IGNORE INTO `%s` (`id`, `version`) VALUES (%d, 0);",
                (const char *) sTableName, SCHEMA_WORLD_SAVEFLAG_ROW );
        if ( ! ExecuteQuery( sQuery ))
        {
                return false;
        }

        return true;
}

bool CWorldStorageMySQL::ApplyMigration_2_3()
{
        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        const CGString sWorldObjects = GetPrefixedTableName( "world_objects" );
        const CGString sWorldObjectData = GetPrefixedTableName( "world_object_data" );
        const CGString sWorldObjectComponents = GetPrefixedTableName( "world_object_components" );
        const CGString sWorldObjectRelations = GetPrefixedTableName( "world_object_relations" );
        const CGString sWorldSavepoints = GetPrefixedTableName( "world_savepoints" );
        const CGString sWorldObjectAudit = GetPrefixedTableName( "world_object_audit" );

        std::vector<CGString> vQueries;
        CGString sQuery;
        CGString sCollationSuffix = GetDefaultTableCollationSuffix();

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`uid` BIGINT UNSIGNED NOT NULL,\n"
                "`object_type` VARCHAR(32) NOT NULL,\n"
                "`object_subtype` VARCHAR(64) NULL,\n"
                "`name` VARCHAR(128) NULL,\n"
                "`account_id` INT UNSIGNED NULL,\n"
                "`container_uid` BIGINT UNSIGNED NULL,\n"
                "`top_level_uid` BIGINT UNSIGNED NULL,\n"
                "`position_x` INT NULL,\n"
                "`position_y` INT NULL,\n"
                "`position_z` INT NULL,\n"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,\n"
                "PRIMARY KEY (`uid`),\n"
                "KEY `ix_world_objects_type` (`object_type`),\n"
                "KEY `ix_world_objects_account` (`account_id`),\n"
                "KEY `ix_world_objects_container` (`container_uid`),\n"
                "KEY `ix_world_objects_top` (`top_level_uid`),\n"
                "FOREIGN KEY (`account_id`) REFERENCES `%s`(`id`) ON DELETE SET NULL,\n"
                "FOREIGN KEY (`container_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL,\n"
                "FOREIGN KEY (`top_level_uid`) REFERENCES `%s`(`uid`) ON DELETE SET NULL\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjects,
                (const char *) sAccounts,
                (const char *) sWorldObjects,
                (const char *) sWorldObjects,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`object_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`data` LONGTEXT NOT NULL,\n"
                "`checksum` VARCHAR(64) NULL,\n"
                "PRIMARY KEY (`object_uid`),\n"
                "FOREIGN KEY (`object_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectData,
                (const char *) sWorldObjects,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`object_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`component` VARCHAR(32) NOT NULL,\n"
                "`name` VARCHAR(128) NOT NULL,\n"
                "`sequence` INT NOT NULL DEFAULT 0,\n"
                "`value` LONGTEXT NULL,\n"
                "PRIMARY KEY (`object_uid`,`component`,`name`,`sequence`),\n"
                "KEY `ix_world_components_component` (`component`),\n"
                "FOREIGN KEY (`object_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectComponents,
                (const char *) sWorldObjects,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`parent_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`child_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`relation` VARCHAR(32) NOT NULL,\n"
                "`sequence` INT NOT NULL DEFAULT 0,\n"
                "PRIMARY KEY (`parent_uid`,`child_uid`,`relation`,`sequence`),\n"
                "KEY `ix_world_relations_child` (`child_uid`),\n"
                "FOREIGN KEY (`parent_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE,\n"
                "FOREIGN KEY (`child_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectRelations,
                (const char *) sWorldObjects,
                (const char *) sWorldObjects,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,\n"
                "`label` VARCHAR(64) NULL,\n"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`objects_count` INT NOT NULL DEFAULT 0,\n"
                "`checksum` VARCHAR(64) NULL,\n"
                "PRIMARY KEY (`id`)\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldSavepoints,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        sQuery.Format(
                "CREATE TABLE IF NOT EXISTS `%s` (\n"
                "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,\n"
                "`object_uid` BIGINT UNSIGNED NOT NULL,\n"
                "`changed_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
                "`change_type` VARCHAR(32) NOT NULL,\n"
                "`data_before` LONGTEXT NULL,\n"
                "`data_after` LONGTEXT NULL,\n"
                "PRIMARY KEY (`id`),\n"
                "KEY `ix_world_audit_object` (`object_uid`),\n"
                "FOREIGN KEY (`object_uid`) REFERENCES `%s`(`uid`) ON DELETE CASCADE\n"
                ") ENGINE=InnoDB DEFAULT CHARSET=%s%s;",
                (const char *) sWorldObjectAudit,
                (const char *) sWorldObjects,
                GetDefaultTableCharset(),
                (const char *) sCollationSuffix);
        vQueries.push_back( sQuery );

        for ( size_t i = 0; i < vQueries.size(); ++i )
        {
                if ( ! ExecuteQuery( vQueries[i] ))
                {
                        return false;
                }
        }

        return true;
}

bool CWorldStorageMySQL::ColumnExists( const CGString & table, const char * column ) const
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( m_sDatabaseName.IsEmpty())
        {
                return false;
        }

        CGString sEscDatabase = EscapeString( (const TCHAR *) m_sDatabaseName );
        CGString sEscTable = EscapeString( (const TCHAR *) table );
        CGString sEscColumn = EscapeString( column );

        CGString sQuery;
        sQuery.Format(
                "SELECT COUNT(*) FROM information_schema.columns "
                "WHERE table_schema = '%s' AND table_name = '%s' AND column_name = '%s';",
                (const char *) sEscDatabase, (const char *) sEscTable, (const char *) sEscColumn );

        MariaDbResult result;
        if ( ! const_cast<CWorldStorageMySQL*>(this)->Query( sQuery, &result ))
        {
                return false;
        }

        bool fExists = false;
        if ( result.IsValid())
        {
                MariaDbResult::Row pRow = result.FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        fExists = ( atoi( pRow[0] ) > 0 );
                }
        }

        return fExists;
}

bool CWorldStorageMySQL::EnsureColumnExists( const CGString & table, const char * column, const char * definition )
{
        if ( ColumnExists( table, column ))
        {
                return true;
        }

        CGString sQuery;
        sQuery.Format( "ALTER TABLE `%s` ADD COLUMN %s;", (const char *) table, definition );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::EnsureSectorColumns()
{
        static bool s_fEnsured = false;
        if ( s_fEnsured )
        {
                return true;
        }

        const CGString sSectors = GetPrefixedTableName( "sectors" );
        if ( ! EnsureColumnExists( sSectors, "has_light_override", "`has_light_override` TINYINT(1) NOT NULL DEFAULT 0 AFTER `last_update`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sSectors, "local_light", "`local_light` INT NULL AFTER `has_light_override`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sSectors, "has_rain_override", "`has_rain_override` TINYINT(1) NOT NULL DEFAULT 0 AFTER `local_light`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sSectors, "rain_chance", "`rain_chance` INT NULL AFTER `has_rain_override`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sSectors, "has_cold_override", "`has_cold_override` TINYINT(1) NOT NULL DEFAULT 0 AFTER `rain_chance`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sSectors, "cold_chance", "`cold_chance` INT NULL AFTER `has_cold_override`" ))
        {
                return false;
        }

        s_fEnsured = true;
        return true;
}

bool CWorldStorageMySQL::EnsureGMPageColumns()
{
        static bool s_fEnsured = false;
        if ( s_fEnsured )
        {
                return true;
        }

        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        if ( ! EnsureColumnExists( sGMPages, "account_name", "`account_name` VARCHAR(32) NULL AFTER `account_id`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sGMPages, "page_time", "`page_time` BIGINT NULL AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sGMPages, "pos_x", "`pos_x` INT NULL AFTER `page_time`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sGMPages, "pos_y", "`pos_y` INT NULL AFTER `pos_x`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sGMPages, "pos_z", "`pos_z` INT NULL AFTER `pos_y`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sGMPages, "map_plane", "`map_plane` INT NULL AFTER `pos_z`" ))
        {
                return false;
        }

        s_fEnsured = true;
        return true;
}

bool CWorldStorageMySQL::EnsureServerColumns()
{
        static bool s_fEnsured = false;
        if ( s_fEnsured )
        {
                return true;
        }

        const CGString sServers = GetPrefixedTableName( "servers" );
        if ( ! EnsureColumnExists( sServers, "time_zone", "`time_zone` INT NOT NULL DEFAULT 0 AFTER `status`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "clients_avg", "`clients_avg` INT NOT NULL DEFAULT 0 AFTER `time_zone`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "url", "`url` VARCHAR(255) NULL AFTER `clients_avg`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "email", "`email` VARCHAR(128) NULL AFTER `url`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "register_password", "`register_password` VARCHAR(64) NULL AFTER `email`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "notes", "`notes` TEXT NULL AFTER `register_password`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "language", "`language` VARCHAR(16) NULL AFTER `notes`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "version", "`version` VARCHAR(64) NULL AFTER `language`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "acc_app", "`acc_app` INT NOT NULL DEFAULT 0 AFTER `version`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "last_valid_seconds", "`last_valid_seconds` INT NULL AFTER `acc_app`" ))
        {
                return false;
        }
        if ( ! EnsureColumnExists( sServers, "age_hours", "`age_hours` INT NULL AFTER `last_valid_seconds`" ))
        {
                return false;
        }

        s_fEnsured = true;
        return true;
}

CGString CWorldStorageMySQL::EscapeString( const TCHAR * pszInput ) const
{
        CGString sResult;
        if ( pszInput == NULL )
        {
                return sResult;
        }

        size_t uiLength = strlen( pszInput );
        if ( uiLength == 0 )
        {
                return sResult;
        }

        if ( ! IsConnected())
        {
                sResult = pszInput;
                return sResult;
        }

        std::string escaped = m_pConnection->EscapeString( pszInput, uiLength );
        sResult = escaped.c_str();
        return sResult;
}

CGString CWorldStorageMySQL::FormatStringValue( const CGString & value ) const
{
        CGString sEscaped = EscapeString( (const TCHAR *) value );
        CGString sResult;
        sResult.Format( "'%s'", (const char *) sEscaped );
        return sResult;
}

CGString CWorldStorageMySQL::FormatOptionalStringValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString CWorldStorageMySQL::FormatDateTimeValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString CWorldStorageMySQL::FormatDateTimeValue( const CRealTime & value ) const
{
        if ( ! value.IsValid())
        {
                return CGString( "NULL" );
        }

        CGString sTemp;
        sTemp.Format( "%04d-%02d-%02d %02d:%02d:%02d",
                1900 + value.m_Year, value.m_Month + 1, value.m_Day,
                value.m_Hour, value.m_Min, value.m_Sec );
        return FormatStringValue( sTemp );
}

CGString CWorldStorageMySQL::FormatIPAddressValue( const CGString & value ) const
{
        if ( value.IsEmpty())
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( value );
}

CGString CWorldStorageMySQL::FormatIPAddressValue( const struct in_addr & value ) const
{
        if ( value.s_addr == 0 )
        {
                return CGString( "NULL" );
        }
        const char * pszIP = inet_ntoa( value );
        if ( pszIP == NULL )
        {
                return CGString( "NULL" );
        }
        return FormatStringValue( CGString( pszIP ));
}

CGString CWorldStorageMySQL::ComputeSerializedChecksum( const CGString & serialized ) const
{
        const char * pData = (const char *) serialized;
        size_t len = (size_t) serialized.GetLength();

        unsigned long long hash = 1469598103934665603ULL;
        for ( size_t i = 0; i < len; ++i )
        {
                hash ^= (unsigned char) pData[i];
                hash *= 1099511628211ULL;
        }

        CGString sHash;
#ifdef _WIN32
        sHash.Format( "%016I64x", hash );
#else
        sHash.Format( "%016llx", hash );
#endif
        return sHash;
}

void CWorldStorageMySQL::AppendVarDefComponents( const CGString & table, unsigned long long uid, const CVarDefMap * pMap, const TCHAR * pszComp, std::vector<UniversalRecord> & outRecords )
{
        if ( table.IsEmpty() || pMap == NULL || pszComp == NULL || pszComp[0] == '\0' )
        {
                return;
        }

        const CGString sComponent = CGString( pszComp );
        const size_t count = pMap->GetCount();
        for ( size_t i = 0; i < count; ++i )
        {
                const CVarDefCont * pVar = pMap->GetAt( i );
                if ( pVar == NULL )
                {
                        continue;
                }

                UniversalRecord record( *this, table );
                record.SetUInt( "object_uid", uid );
                record.SetString( "component", sComponent );
                record.SetString( "name", CGString( pVar->GetKey()));
                record.SetInt( "sequence", (int) i );

                const TCHAR * pszVal = pVar->GetValStr();
                if ( pszVal != NULL && pszVal[0] != '\0' )
                {
                        record.SetString( "value", CGString( pszVal ));
                }
                else
                {
                        record.SetNull( "value" );
                }

                outRecords.push_back( record );
        }
}

unsigned int CWorldStorageMySQL::GetAccountId( const CGString & name )
{
        if ( ! IsConnected())
        {
                return 0;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sEscName = EscapeString( (const TCHAR *) name );

        CGString sQuery;
        sQuery.Format( "SELECT `id` FROM `%s` WHERE `name` = '%s' LIMIT 1;", (const char *) sAccounts, (const char *) sEscName );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return 0;
        }

        unsigned int uiId = 0;
        if ( result.IsValid())
        {
                MariaDbResult::Row pRow = result.FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        uiId = (unsigned int) strtoul( pRow[0], NULL, 10 );
                }
        }
        return uiId;
}

bool CWorldStorageMySQL::FetchAccounts( std::vector<AccountData> & accounts, const CGString & whereClause )
{
        accounts.clear();

        if ( ! IsConnected())
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );

        CGString sQuery;
        sQuery.Format(
                "SELECT `id`,`name`,`password`,`plevel`,`priv_flags`,`status`,"
                "IFNULL(`comment`, ''),IFNULL(`email`, ''),IFNULL(`chat_name`, ''),"
                "IFNULL(`language`, ''),`total_connect_time`,`last_connect_time`,"
                "IFNULL(`last_ip`, ''),IFNULL(DATE_FORMAT(`last_login`, '%%Y-%%m-%%d %%H:%%i:%%s'), ''),"
                "IFNULL(`first_ip`, ''),IFNULL(DATE_FORMAT(`first_login`, '%%Y-%%m-%%d %%H:%%i:%%s'), ''),"
                "IFNULL(`last_char_uid`, 0),`email_failures`,UNIX_TIMESTAMP(`updated_at`)"
                " FROM `%s` %s;",
                (const char *) sAccounts,
                whereClause.IsEmpty() ? "" : (const char *) whereClause );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result.IsValid())
        {
            return true;
        }

        MariaDbResult::Row pRow;
        while (( pRow = result.FetchRow()) != NULL )
        {
                AccountData data;
                data.m_id = pRow[0] ? (unsigned int) strtoul( pRow[0], NULL, 10 ) : 0;
                data.m_sName = pRow[1] ? pRow[1] : "";
                data.m_sPassword = pRow[2] ? pRow[2] : "";
                data.m_iPrivLevel = pRow[3] ? atoi( pRow[3] ) : 0;
                data.m_uPrivFlags = pRow[4] ? (unsigned int) strtoul( pRow[4], NULL, 10 ) : 0;
                data.m_uStatus = pRow[5] ? (unsigned int) strtoul( pRow[5], NULL, 10 ) : 0;
                data.m_sComment = pRow[6] ? pRow[6] : "";
                data.m_sEmail = pRow[7] ? pRow[7] : "";
                data.m_sChatName = pRow[8] ? pRow[8] : "";
                data.m_sLanguage = pRow[9] ? pRow[9] : "";
                data.m_iTotalConnectTime = pRow[10] ? atoi( pRow[10] ) : 0;
                data.m_iLastConnectTime = pRow[11] ? atoi( pRow[11] ) : 0;
                data.m_sLastIP = pRow[12] ? pRow[12] : "";
                data.m_sLastLogin = pRow[13] ? pRow[13] : "";
                data.m_sFirstIP = pRow[14] ? pRow[14] : "";
                data.m_sFirstLogin = pRow[15] ? pRow[15] : "";
#ifdef _WIN32
                data.m_uLastCharUID = pRow[16] ? (unsigned long long) _strtoui64( pRow[16], NULL, 10 ) : 0;
                data.m_tUpdatedAt = pRow[18] ? (time_t) _strtoi64( pRow[18], NULL, 10 ) : 0;
#else
                data.m_uLastCharUID = pRow[16] ? (unsigned long long) strtoull( pRow[16], NULL, 10 ) : 0;
                data.m_tUpdatedAt = pRow[18] ? (time_t) strtoll( pRow[18], NULL, 10 ) : 0;
#endif
                data.m_uEmailFailures = pRow[17] ? (unsigned int) strtoul( pRow[17], NULL, 10 ) : 0;
                data.m_EmailSchedule.clear();

                accounts.push_back( data );
        }

        return true;
}

void CWorldStorageMySQL::LoadAccountEmailSchedule( std::vector<AccountData> & accounts )
{
        if ( accounts.empty())
        {
                return;
        }

        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sQuery;
        sQuery.Format( "SELECT `account_id`,`sequence`,`message_id` FROM `%s` ORDER BY `account_id`,`sequence`;", (const char *) sEmails );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return;
        }

        if ( !result.IsValid())
        {
                return;
        }

        std::unordered_map<unsigned int, AccountData*> mapAccounts;
        for ( size_t i = 0; i < accounts.size(); ++i )
        {
                accounts[i].m_EmailSchedule.clear();
                mapAccounts[ accounts[i].m_id ] = &accounts[i];
        }

        MariaDbResult::Row pRow;
        while (( pRow = result.FetchRow()) != NULL )
        {
                unsigned int accountId = pRow[0] ? (unsigned int) strtoul( pRow[0], NULL, 10 ) : 0;
                auto it = mapAccounts.find( accountId );
                if ( it == mapAccounts.end())
                {
                        continue;
                }

                unsigned int messageId = pRow[2] ? (unsigned int) strtoul( pRow[2], NULL, 10 ) : 0;
                it->second->m_EmailSchedule.push_back( (WORD) messageId );
        }

}

bool CWorldStorageMySQL::InsertOrUpdateSchemaValue( int id, int value )
{
        const CGString sTableName = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format(
                "INSERT INTO `%s` (`id`, `version`) VALUES (%d, %d) ON DUPLICATE KEY UPDATE `version` = VALUES(`version`);",
                (const char *) sTableName, id, value );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::QuerySchemaValue( int id, int & value )
{
        value = 0;
        const CGString sTableName = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;",
                (const char *) sTableName, id );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( result.IsValid())
        {
                MariaDbResult::Row pRow = result.FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        value = atoi( pRow[0] );
                }
        }

        return true;
}

bool CWorldStorageMySQL::SetWorldSaveCount( int saveCount )
{
        return InsertOrUpdateSchemaValue( SCHEMA_WORLD_SAVECOUNT_ROW, saveCount );
}

bool CWorldStorageMySQL::GetWorldSaveCount( int & saveCount )
{
        if ( ! QuerySchemaValue( SCHEMA_WORLD_SAVECOUNT_ROW, saveCount ))
        {
                return false;
        }
        return true;
}

bool CWorldStorageMySQL::SetWorldSaveCompleted( bool fCompleted )
{
        return InsertOrUpdateSchemaValue( SCHEMA_WORLD_SAVEFLAG_ROW, fCompleted ? 1 : 0 );
}

bool CWorldStorageMySQL::GetWorldSaveCompleted( bool & fCompleted )
{
        int iValue = 0;
        if ( ! QuerySchemaValue( SCHEMA_WORLD_SAVEFLAG_ROW, iValue ))
        {
                return false;
        }
        fCompleted = ( iValue != 0 );
        return true;
}

bool CWorldStorageMySQL::LoadWorldMetadata( int & saveCount, bool & fCompleted )
{
        if ( ! GetWorldSaveCount( saveCount ))
        {
                return false;
        }
        if ( ! GetWorldSaveCompleted( fCompleted ))
        {
                return false;
        }
        return true;
}

void CWorldStorageMySQL::UpdateAccountSyncTimestamp( const std::vector<AccountData> & accounts )
{
        time_t tMax = ( m_tLastAccountSync > 0 ) ? ( m_tLastAccountSync - 1 ) : 0;
        bool fUpdated = false;

        for ( size_t i = 0; i < accounts.size(); ++i )
        {
                if ( accounts[i].m_tUpdatedAt > tMax )
                {
                        tMax = accounts[i].m_tUpdatedAt;
                        fUpdated = true;
                }
        }

        if ( fUpdated )
        {
                m_tLastAccountSync = tMax + 1;
        }
        else if ( m_tLastAccountSync == 0 )
        {
                m_tLastAccountSync = time( NULL );
        }
}

bool CWorldStorageMySQL::LoadAllAccounts( std::vector<AccountData> & accounts )
{
        if ( ! FetchAccounts( accounts, CGString()))
        {
                return false;
        }
        LoadAccountEmailSchedule( accounts );
        UpdateAccountSyncTimestamp( accounts );
        return true;
}

bool CWorldStorageMySQL::LoadChangedAccounts( std::vector<AccountData> & accounts )
{
        if ( m_tLastAccountSync == 0 )
        {
                return LoadAllAccounts( accounts );
        }

        CGString sWhere;
        sWhere.Format( "WHERE `updated_at` >= FROM_UNIXTIME(%lld)", (long long) m_tLastAccountSync );
        if ( ! FetchAccounts( accounts, sWhere ))
        {
                return false;
        }
        LoadAccountEmailSchedule( accounts );
        UpdateAccountSyncTimestamp( accounts );
        return true;
}

bool CWorldStorageMySQL::UpsertAccount( const CAccount & account )
{
        if ( ! IsConnected())
        {
                return false;
        }

        Transaction transaction( *this );
        if ( ! transaction.Begin())
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        UniversalRecord accountRecord( *this, sAccounts );

        CGString sName = CGString( account.GetName());
        accountRecord.SetString( "name", sName );
        accountRecord.SetString( "password", CGString( account.GetPassword()));
        accountRecord.SetInt( "plevel", account.GetPrivLevel());
        accountRecord.SetUInt( "priv_flags", (unsigned int) account.m_PrivFlags );

        int statusValue = 0;
        if ( account.IsPriv( PRIV_BLOCKED ))
        {
                statusValue |= 0x1;
        }
        if ( account.IsPriv( PRIV_JAILED ))
        {
                statusValue |= 0x2;
        }
        accountRecord.SetInt( "status", statusValue );

        accountRecord.SetOptionalString( "comment", account.m_sComment );
        accountRecord.SetOptionalString( "email", account.m_sEMail );
        accountRecord.SetOptionalString( "chat_name", account.m_sChatName );

        CGString sLanguageRaw;
        if ( account.m_lang[0] )
        {
                TCHAR szLang[4];
                szLang[0] = account.m_lang[0];
                szLang[1] = account.m_lang[1];
                szLang[2] = account.m_lang[2];
                szLang[3] = '\0';
                sLanguageRaw = szLang;
        }
        accountRecord.SetOptionalString( "language", sLanguageRaw );
        accountRecord.SetInt( "total_connect_time", account.m_Total_Connect_Time );
        accountRecord.SetInt( "last_connect_time", account.m_Last_Connect_Time );
        accountRecord.SetRaw( "last_ip", FormatIPAddressValue( account.m_Last_IP ));
        accountRecord.SetRaw( "first_ip", FormatIPAddressValue( account.m_First_IP ));
        accountRecord.SetDateTime( "last_login", account.m_Last_Connect_Date );
        accountRecord.SetDateTime( "first_login", account.m_First_Connect_Date );

        if ( account.m_uidLastChar.IsValidUID())
        {
                accountRecord.SetUInt( "last_char_uid", (unsigned int) account.m_uidLastChar );
        }
        else
        {
                accountRecord.SetNull( "last_char_uid" );
        }

        accountRecord.SetUInt( "email_failures", (unsigned int) account.m_iEmailFailures );

        if ( ! ExecuteQuery( accountRecord.BuildInsert( false, true )))
        {
                transaction.Rollback();
                return false;
        }

        unsigned int accountId = GetAccountId( sName );
        if ( accountId == 0 )
        {
                transaction.Rollback();
                return false;
        }

        const CGString sEmails = GetPrefixedTableName( "account_emails" );
        CGString sDelete;
        sDelete.Format( "DELETE FROM `%s` WHERE `account_id` = %u;", (const char *) sEmails, accountId );
        if ( ! ExecuteQuery( sDelete ))
        {
                transaction.Rollback();
                return false;
        }

        std::vector<UniversalRecord> emailRecords;
        for ( int i = 0; i < account.m_EMailSchedule.GetCount(); ++i )
        {
                UniversalRecord emailRecord( *this, sEmails );
                emailRecord.SetUInt( "account_id", accountId );
                emailRecord.SetInt( "sequence", i );
                emailRecord.SetUInt( "message_id", (unsigned int) account.m_EMailSchedule[i] );
                emailRecords.push_back( emailRecord );
        }

        if ( ! emailRecords.empty())
        {
                if ( ! ExecuteRecordsInsert( emailRecords ))
                {
                        transaction.Rollback();
                        return false;
                }
        }

        if ( ! transaction.Commit())
        {
                transaction.Rollback();
                return false;
        }

        return true;
}

bool CWorldStorageMySQL::DeleteAccount( const TCHAR * pszAccountName )
{
        if ( ! IsConnected() || pszAccountName == NULL || pszAccountName[0] == '\0' )
        {
                return false;
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sEscName = EscapeString( pszAccountName );

        CGString sQuery;
        sQuery.Format( "DELETE FROM `%s` WHERE `name` = '%s';", (const char *) sAccounts, (const char *) sEscName );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::SaveWorldObject( CObjBase * pObject )
{
        if ( ! IsConnected() || pObject == NULL )
        {
                return false;
        }

        return WithTransaction( [this, pObject]() -> bool
        {
                return SaveWorldObjectInternal( pObject );
        });
}

bool CWorldStorageMySQL::SaveWorldObjects( const std::vector<CObjBase*> & objects )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( objects.empty())
        {
                        return true;
        }

        return WithTransaction( [this, &objects]() -> bool
        {
                for ( size_t i = 0; i < objects.size(); ++i )
                {
                        CObjBase * pObject = objects[i];
                        if ( pObject == NULL )
                        {
                                continue;
                        }
                        if ( ! SaveWorldObjectInternal( pObject ))
                        {
                                return false;
                        }
                }
                return true;
        });
}

bool CWorldStorageMySQL::DeleteWorldObject( const CObjBase * pObject )
{
        if ( ! IsConnected() || pObject == NULL )
        {
                return false;
        }

        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
        const CGString sWorldObjects = GetPrefixedTableName( "world_objects" );

        return WithTransaction( [this, uid, sWorldObjects]() -> bool
        {
                CGString sQuery;
#ifdef _WIN32
                sQuery.Format( "DELETE FROM `%s` WHERE `uid` = %I64u;", (const char *) sWorldObjects, uid );
#else
                sQuery.Format( "DELETE FROM `%s` WHERE `uid` = %llu;", (const char *) sWorldObjects, uid );
#endif
                return ExecuteQuery( sQuery );
        });
}

bool CWorldStorageMySQL::DeleteObject( const CObjBase * pObject )
{
        return DeleteWorldObject( pObject );
}

void CWorldStorageMySQL::MarkObjectDirty( const CObjBase & object, StorageDirtyType type )
{
        if ( ! IsEnabled())
                return;

        const unsigned long long uid = (unsigned long long) (UINT) object.GetUID();

        std::unique_lock<std::mutex> lock( m_DirtyMutex );

        if ( type == StorageDirtyType_Delete )
        {
                m_DirtyObjects.erase( uid );
                return;
        }

        auto it = m_DirtyObjects.find( uid );
        if ( it == m_DirtyObjects.end())
        {
                m_DirtyObjects.emplace( uid, StorageDirtyType_Save );
                m_DirtyQueue.push_back( uid );
                lock.unlock();
                m_DirtyCondition.notify_one();
                return;
        }

        if ( it->second != StorageDirtyType_Delete )
        {
                it->second = StorageDirtyType_Save;
        }
}

void CWorldStorageMySQL::StartDirtyWorker()
{
        std::lock_guard<std::mutex> lock( m_DirtyMutex );
        if ( m_fDirtyThreadRunning )
                return;
        m_fDirtyThreadStop = false;
        m_DirtyThread = std::thread( &CWorldStorageMySQL::DirtyWorkerLoop, this );
        m_fDirtyThreadRunning = true;
}

void CWorldStorageMySQL::StopDirtyWorker()
{
        {
                std::lock_guard<std::mutex> lock( m_DirtyMutex );
                if ( ! m_fDirtyThreadRunning )
                        return;
                m_fDirtyThreadStop = true;
        }
        m_DirtyCondition.notify_all();
        if ( m_DirtyThread.joinable())
        {
                m_DirtyThread.join();
        }
        {
                std::lock_guard<std::mutex> lock( m_DirtyMutex );
                m_fDirtyThreadRunning = false;
                m_fDirtyThreadStop = false;
                m_DirtyQueue.clear();
                m_DirtyObjects.clear();
        }
}

void CWorldStorageMySQL::DirtyWorkerLoop()
{
        std::unique_lock<std::mutex> lock( m_DirtyMutex );
        while ( true )
        {
                m_DirtyCondition.wait( lock, [this]()
                {
                        return m_fDirtyThreadStop || ! m_DirtyQueue.empty();
                });

                if ( m_fDirtyThreadStop && m_DirtyQueue.empty())
                        break;

                std::vector<std::pair<unsigned long long, StorageDirtyType>> batch;
                while ( ! m_DirtyQueue.empty())
                {
                        unsigned long long uid = m_DirtyQueue.front();
                        m_DirtyQueue.pop_front();
                        auto it = m_DirtyObjects.find( uid );
                        if ( it == m_DirtyObjects.end())
                                continue;
                        batch.emplace_back( uid, it->second );
                        m_DirtyObjects.erase( it );
                }

                lock.unlock();
                for ( const auto & entry : batch )
                {
                        if ( ! ProcessDirtyObject( entry.first, entry.second ))
                        {
                                g_Log.Event( LOGM_SAVE|LOGL_WARN, "Failed to persist object 0%llx to MySQL.\n", entry.first );
                        }
                }
                lock.lock();
        }
}

bool CWorldStorageMySQL::ProcessDirtyObject( unsigned long long uid, StorageDirtyType type )
{
        if ( type != StorageDirtyType_Save )
                return true;
        if ( ! IsEnabled())
                return false;

        CObjUID objUid( (UINT) uid );
        CObjBase * pObject = objUid.ObjFind();
        if ( pObject == NULL )
                return true;

        if ( pObject->IsChar())
        {
                CChar * pChar = dynamic_cast<CChar*>( pObject );
                return ( pChar != NULL ) && SaveChar( *pChar );
        }
        if ( pObject->IsItem())
        {
                CItem * pItem = dynamic_cast<CItem*>( pObject );
                return ( pItem != NULL ) && SaveItem( *pItem );
        }
        return false;
}

bool CWorldStorageMySQL::SaveSector( const CSector & sector )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureSectorColumns())
        {
                return false;
        }

        const CGString sSectors = GetPrefixedTableName( "sectors" );
        UniversalRecord record( *this, sSectors );

        const CPointMap base = sector.GetBase();
        record.SetInt( "map_plane", 0 );
        record.SetInt( "x1", base.m_x );
        record.SetInt( "y1", base.m_y );
        record.SetInt( "x2", base.m_x + SECTOR_SIZE_X );
        record.SetInt( "y2", base.m_y + SECTOR_SIZE_Y );
        record.SetRaw( "last_update", "CURRENT_TIMESTAMP" );

        if ( sector.IsLightOverriden())
        {
                record.SetBool( "has_light_override", true );
                record.SetInt( "local_light", sector.GetLight());
        }
        else
        {
                record.SetBool( "has_light_override", false );
                record.SetNull( "local_light" );
        }

        if ( sector.IsRainOverriden())
        {
                record.SetBool( "has_rain_override", true );
                record.SetInt( "rain_chance", sector.GetRainChance());
        }
        else
        {
                record.SetBool( "has_rain_override", false );
                record.SetNull( "rain_chance" );
        }

        if ( sector.IsColdOverriden())
        {
                record.SetBool( "has_cold_override", true );
                record.SetInt( "cold_chance", sector.GetColdChance());
        }
        else
        {
                record.SetBool( "has_cold_override", false );
                record.SetNull( "cold_chance" );
        }

        return ExecuteQuery( record.BuildInsert( false, true ));
}

bool CWorldStorageMySQL::SaveChar( CChar & character )
{
        return SaveWorldObject( &character );
}

bool CWorldStorageMySQL::SaveItem( CItem & item )
{
        return SaveWorldObject( &item );
}

bool CWorldStorageMySQL::SaveGMPage( const CGMPage & page )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureGMPageColumns())
        {
                return false;
        }

        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        UniversalRecord record( *this, sGMPages );

        record.SetNull( "account_id" );
        record.SetNull( "account_name" );

        CAccount * pAccount = page.FindAccount();
        if ( pAccount != NULL )
        {
                CGString sName = pAccount->GetName();
                if ( ! sName.IsEmpty())
                {
                        unsigned int accountId = GetAccountId( sName );
                        if ( accountId > 0 )
                        {
                                record.SetUInt( "account_id", accountId );
                        }
                        record.SetOptionalString( "account_name", sName );
                }
        }

        record.SetNull( "character_uid" );
        record.SetOptionalString( "reason", CGString( page.GetReason()));
        record.SetUInt( "status", 0 );
        record.SetInt( "page_time", (long long) page.m_lTime );
        record.SetInt( "pos_x", page.m_p.m_x );
        record.SetInt( "pos_y", page.m_p.m_y );
        record.SetInt( "pos_z", page.m_p.m_z );
        record.SetInt( "map_plane", 0 );

        return ExecuteQuery( record.BuildInsert( false, true ));
}

bool CWorldStorageMySQL::SaveServer( const CServRef & server )
{
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureServerColumns())
        {
                return false;
        }

        const TCHAR * pszName = server.GetName();
        if ( pszName == NULL || pszName[0] == '\0' )
        {
                return false;
        }

        const CGString sServers = GetPrefixedTableName( "servers" );
        UniversalRecord record( *this, sServers );

        record.SetString( "name", CGString( pszName ));
        record.SetOptionalString( "address", CGString( server.m_ip.GetAddrStr()));
        record.SetInt( "port", server.m_ip.GetPort());
        record.SetInt( "status", server.IsValidStatus() ? 1 : 0 );
        record.SetInt( "time_zone", server.m_TimeZone );
        record.SetInt( "clients_avg", server.GetClientsAvg());
        record.SetOptionalString( "url", server.m_sURL );
        record.SetOptionalString( "email", server.m_sEMail );
        record.SetOptionalString( "register_password", server.m_sRegisterPassword );
        record.SetOptionalString( "notes", server.m_sNotes );
        record.SetOptionalString( "language", server.m_sLang );
        record.SetOptionalString( "version", server.m_sVersion );
        record.SetInt( "acc_app", server.m_eAccApp );
        record.SetInt( "last_valid_seconds", server.GetTimeSinceLastValid());
        record.SetInt( "age_hours", server.GetAgeHours());
        record.SetRaw( "last_seen", "CURRENT_TIMESTAMP" );

        return ExecuteQuery( record.BuildInsert( false, true ));
}

bool CWorldStorageMySQL::ClearGMPages()
{
        if ( ! IsConnected())
        {
                return false;
        }
        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        return ClearTable( sGMPages );
}

bool CWorldStorageMySQL::ClearServers()
{
        if ( ! IsConnected())
        {
                return false;
        }
        const CGString sServers = GetPrefixedTableName( "servers" );
        return ClearTable( sServers );
}

bool CWorldStorageMySQL::ClearWorldData()
{
        if ( ! IsConnected())
        {
                return false;
        }

        return WithTransaction( [this]() -> bool
        {
                const CGString sAudit = GetPrefixedTableName( "world_object_audit" );
                const CGString sRelations = GetPrefixedTableName( "world_object_relations" );
                const CGString sComponents = GetPrefixedTableName( "world_object_components" );
                const CGString sData = GetPrefixedTableName( "world_object_data" );
                const CGString sObjects = GetPrefixedTableName( "world_objects" );
                const CGString sSavepoints = GetPrefixedTableName( "world_savepoints" );

                if ( ! ClearTable( sAudit ))
                {
                        return false;
                }
                if ( ! ClearTable( sRelations ))
                {
                        return false;
                }
                if ( ! ClearTable( sComponents ))
                {
                        return false;
                }
                if ( ! ClearTable( sData ))
                {
                        return false;
                }
                if ( ! ClearTable( sObjects ))
                {
                        return false;
                }
                if ( ! ClearTable( sSavepoints ))
                {
                        return false;
                }
                return true;
        });
}

bool CWorldStorageMySQL::LoadSectors( std::vector<SectorData> & sectors )
{
        sectors.clear();
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureSectorColumns())
        {
                return false;
        }

        const CGString sSectors = GetPrefixedTableName( "sectors" );
        CGString sQuery;
        sQuery.Format( "SELECT `map_plane`,`x1`,`y1`,`x2`,`y2`,`has_light_override`,`local_light`,`has_rain_override`,`rain_chance`,`has_cold_override`,`cold_chance` FROM `%s`;",
                (const char *) sSectors );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result.IsValid())
        {
                return true;
        }

        MariaDbResult::Row pRow;
        while (( pRow = result.FetchRow()) != NULL )
        {
                SectorData data;
                data.m_iMapPlane = pRow[0] ? atoi( pRow[0] ) : 0;
                data.m_iX1 = pRow[1] ? atoi( pRow[1] ) : 0;
                data.m_iY1 = pRow[2] ? atoi( pRow[2] ) : 0;
                data.m_iX2 = pRow[3] ? atoi( pRow[3] ) : 0;
                data.m_iY2 = pRow[4] ? atoi( pRow[4] ) : 0;
                data.m_fHasLightOverride = pRow[5] ? ( atoi( pRow[5] ) != 0 ) : false;
                data.m_iLocalLight = pRow[6] ? atoi( pRow[6] ) : 0;
                data.m_fHasRainOverride = pRow[7] ? ( atoi( pRow[7] ) != 0 ) : false;
                data.m_iRainChance = pRow[8] ? atoi( pRow[8] ) : 0;
                data.m_fHasColdOverride = pRow[9] ? ( atoi( pRow[9] ) != 0 ) : false;
                data.m_iColdChance = pRow[10] ? atoi( pRow[10] ) : 0;
                sectors.push_back( data );
        }

        return true;
}

bool CWorldStorageMySQL::LoadWorldObjects( std::vector<WorldObjectRecord> & objects )
{
        objects.clear();
        if ( ! IsConnected())
        {
                return false;
        }

        const CGString sObjects = GetPrefixedTableName( "world_objects" );
        const CGString sData = GetPrefixedTableName( "world_object_data" );

        CGString sQuery;
        sQuery.Format(
                "SELECT o.`uid`,o.`object_type`,o.`object_subtype`,d.`data` FROM `%s` o INNER JOIN `%s` d ON o.`uid` = d.`object_uid` ORDER BY o.`uid`;",
                (const char *) sObjects, (const char *) sData );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result.IsValid())
        {
                return true;
        }

        MariaDbResult::Row pRow;
        while (( pRow = result.FetchRow()) != NULL )
        {
                WorldObjectRecord record;
#ifdef _WIN32
                record.m_uid = pRow[0] ? (unsigned long long) _strtoui64( pRow[0], NULL, 10 ) : 0;
#else
                record.m_uid = pRow[0] ? (unsigned long long) strtoull( pRow[0], NULL, 10 ) : 0;
#endif
                const char * pszType = pRow[1] ? pRow[1] : "";
                record.m_fIsChar = ( strcmpi( pszType, "char" ) == 0 );
                const char * pszSubtype = pRow[2] ? pRow[2] : "";
                record.m_iBaseId = (int) strtol( pszSubtype, NULL, 0 );
                record.m_sSerialized = pRow[3] ? pRow[3] : "";
                objects.push_back( record );
        }

        return true;
}

bool CWorldStorageMySQL::LoadGMPages( std::vector<GMPageRecord> & pages )
{
        pages.clear();
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureGMPageColumns())
        {
                return false;
        }

        const CGString sGMPages = GetPrefixedTableName( "gm_pages" );
        CGString sQuery;
        sQuery.Format( "SELECT `account_id`,`account_name`,`reason`,`page_time`,`pos_x`,`pos_y`,`pos_z`,`map_plane` FROM `%s` ORDER BY `id`;",
                (const char *) sGMPages );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result.IsValid())
        {
                return true;
        }

        MariaDbResult::Row pRow;
        while (( pRow = result.FetchRow()) != NULL )
        {
                GMPageRecord record;
                unsigned int accountId = pRow[0] ? (unsigned int) strtoul( pRow[0], NULL, 10 ) : 0;
                record.m_sAccount = pRow[1] ? pRow[1] : "";
                if ( record.m_sAccount.IsEmpty() && accountId > 0 )
                {
                        record.m_sAccount = GetAccountNameById( accountId );
                }
                record.m_sReason = pRow[2] ? pRow[2] : "";
#ifdef _WIN32
                record.m_lTime = pRow[3] ? (long) _strtoi64( pRow[3], NULL, 10 ) : 0;
#else
                record.m_lTime = pRow[3] ? (long) strtoll( pRow[3], NULL, 10 ) : 0;
#endif
                record.m_iPosX = pRow[4] ? atoi( pRow[4] ) : 0;
                record.m_iPosY = pRow[5] ? atoi( pRow[5] ) : 0;
                record.m_iPosZ = pRow[6] ? atoi( pRow[6] ) : 0;
                record.m_iMapPlane = pRow[7] ? atoi( pRow[7] ) : 0;
                pages.push_back( record );
        }
        return true;
}

bool CWorldStorageMySQL::LoadServers( std::vector<ServerRecord> & servers )
{
        servers.clear();
        if ( ! IsConnected())
        {
                return false;
        }
        if ( ! EnsureServerColumns())
        {
                return false;
        }

        const CGString sServers = GetPrefixedTableName( "servers" );
        CGString sQuery;
        sQuery.Format( "SELECT `name`,`address`,`port`,`status`,`time_zone`,`clients_avg`,`url`,`email`,`register_password`,`notes`,`language`,`version`,`acc_app`,`last_valid_seconds`,`age_hours` FROM `%s` ORDER BY `name`;",
                (const char *) sServers );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        if ( !result.IsValid())
        {
                return true;
        }

        MariaDbResult::Row pRow;
        while (( pRow = result.FetchRow()) != NULL )
        {
                ServerRecord record;
                record.m_sName = pRow[0] ? pRow[0] : "";
                record.m_sAddress = pRow[1] ? pRow[1] : "";
                record.m_iPort = pRow[2] ? atoi( pRow[2] ) : 0;
                record.m_iStatus = pRow[3] ? atoi( pRow[3] ) : 0;
                record.m_iTimeZone = pRow[4] ? atoi( pRow[4] ) : 0;
                record.m_iClientsAvg = pRow[5] ? atoi( pRow[5] ) : 0;
                record.m_sURL = pRow[6] ? pRow[6] : "";
                record.m_sEmail = pRow[7] ? pRow[7] : "";
                record.m_sRegisterPassword = pRow[8] ? pRow[8] : "";
                record.m_sNotes = pRow[9] ? pRow[9] : "";
                record.m_sLanguage = pRow[10] ? pRow[10] : "";
                record.m_sVersion = pRow[11] ? pRow[11] : "";
                record.m_iAccApp = pRow[12] ? atoi( pRow[12] ) : 0;
                record.m_iLastValidSeconds = pRow[13] ? atoi( pRow[13] ) : 0;
                record.m_iAgeHours = pRow[14] ? atoi( pRow[14] ) : 0;
                servers.push_back( record );
        }
        return true;
}

CGString CWorldStorageMySQL::GetAccountNameById( unsigned int accountId )
{
        if ( accountId == 0 )
        {
                return CGString();
        }

        const CGString sAccounts = GetPrefixedTableName( "accounts" );
        CGString sQuery;
        sQuery.Format( "SELECT `name` FROM `%s` WHERE `id` = %u LIMIT 1;", (const char *) sAccounts, accountId );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return CGString();
        }

        CGString sName;
        if ( result.IsValid())
        {
                MariaDbResult::Row pRow = result.FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        sName = pRow[0];
                }
        }

        return sName;
}

bool CWorldStorageMySQL::SaveWorldObjectInternal( CObjBase * pObject )
{
        if ( pObject == NULL )
        {
                return false;
        }

        CGString sSerialized;
        if ( ! SerializeWorldObject( pObject, sSerialized ))
        {
                return false;
        }

        if ( ! UpsertWorldObjectMeta( pObject, sSerialized ))
        {
                return false;
        }
        if ( ! UpsertWorldObjectData( pObject, sSerialized ))
        {
                return false;
        }
        if ( ! RefreshWorldObjectComponents( pObject ))
        {
                return false;
        }
        if ( ! RefreshWorldObjectRelations( pObject ))
        {
                return false;
        }

        return true;
}

bool CWorldStorageMySQL::SerializeWorldObject( CObjBase * pObject, CGString & outSerialized ) const
{
        if ( pObject == NULL )
        {
                return false;
        }

        std::string sTempPath;
        if ( ! GenerateTempScriptPath( sTempPath ))
        {
                return false;
        }

        TempFileGuard tempFile( sTempPath );
        if ( ! tempFile.IsValid())
        {
                return false;
        }

        CScript script;
        if ( ! script.Open( tempFile.GetPath().c_str(), OF_WRITE | OF_TEXT ))
        {
                return false;
        }

        pObject->r_Write( script );
        script.Close();

        std::ifstream input( tempFile.GetPath().c_str(), std::ios::in | std::ios::binary );
        if ( ! input.is_open())
        {
                return false;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        input.close();

        const std::string serialized = buffer.str();
        outSerialized = serialized.c_str();

        return true;
}

bool CWorldStorageMySQL::ApplyWorldObjectData( CObjBase & object, const CGString & serialized ) const
{
        std::string sTempPath;
        if ( ! GenerateTempScriptPath( sTempPath ))
        {
                return false;
        }

        TempFileGuard tempFile( sTempPath );
        if ( ! tempFile.IsValid())
        {
                return false;
        }

        {
                std::ofstream output( tempFile.GetPath().c_str(), std::ios::out | std::ios::binary );
                if ( ! output.is_open())
                {
                        return false;
                }
                output << (const char *) serialized;
        }

        CScript script;
        if ( ! script.Open( tempFile.GetPath().c_str(), OF_READ | OF_TEXT ))
        {
                return false;
        }

        bool fResult = false;
        if ( script.FindNextSection())
        {
                fResult = object.r_Load( script );
        }

        script.Close();
        return fResult;
}

bool CWorldStorageMySQL::UpsertWorldObjectMeta( CObjBase * pObject, const CGString & serialized )
{
        (void) serialized;

        if ( pObject == NULL )
        {
                return false;
        }

        const CGString sWorldObjects = GetPrefixedTableName( "world_objects" );
        UniversalRecord record( *this, sWorldObjects );

        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
        record.SetUInt( "uid", uid );

        if ( pObject->IsChar())
        {
                record.SetString( "object_type", CGString( "char" ));
        }
        else
        {
                record.SetString( "object_type", CGString( "item" ));
        }

        CGString sSubtype;
        sSubtype.Format( "0x%x", (unsigned int) pObject->GetBaseID());
        record.SetString( "object_subtype", sSubtype );

        const TCHAR * pszName = pObject->GetName();
        if ( pszName != NULL )
        {
                record.SetOptionalString( "name", CGString( pszName ));
        }
        else
        {
                record.SetNull( "name" );
        }

        record.SetNull( "account_id" );
        if ( pObject->IsChar())
        {
                CChar * pChar = dynamic_cast<CChar*>( pObject );
                if ( pChar != NULL && pChar->m_pPlayer != NULL )
                {
                        CAccount * pAccount = pChar->m_pPlayer->GetAccount();
                        if ( pAccount != NULL )
                        {
                                CGString sAccountName = CGString( pAccount->GetName());
                                if ( ! sAccountName.IsEmpty())
                                {
                                        unsigned int accountId = GetAccountId( sAccountName );
                                        if ( accountId > 0 )
                                        {
                                                record.SetUInt( "account_id", accountId );
                                        }
                                }
                        }
                }
        }

        record.SetNull( "container_uid" );
        if ( pObject->IsItem())
        {
                const CItem * pItem = dynamic_cast<const CItem*>( pObject );
                if ( pItem != NULL )
                {
                        const CObjBase * pContainer = pItem->GetContainer();
                        if ( pContainer != NULL )
                        {
                                const unsigned long long containerUid = (unsigned long long) (UINT) pContainer->GetUID();
                                record.SetUInt( "container_uid", containerUid );
                        }
                }
        }

        record.SetNull( "top_level_uid" );
        CObjBaseTemplate * pTop = pObject->GetTopLevelObj();
        if ( pTop != NULL )
        {
                CObjBase * pTopObj = dynamic_cast<CObjBase*>( pTop );
                if ( pTopObj != NULL )
                {
                        const unsigned long long topUid = (unsigned long long) (UINT) pTopObj->GetUID();
                        record.SetUInt( "top_level_uid", topUid );
                }
        }

        record.SetNull( "position_x" );
        record.SetNull( "position_y" );
        record.SetNull( "position_z" );
        if ( pObject->IsTopLevel())
        {
                const CPointMap & pt = pObject->GetTopPoint();
                record.SetInt( "position_x", pt.m_x );
                record.SetInt( "position_y", pt.m_y );
                record.SetInt( "position_z", pt.m_z );
        }
        else if ( pObject->IsItem() && pObject->IsInContainer())
        {
                const CItem * pItem = dynamic_cast<const CItem*>( pObject );
                if ( pItem != NULL )
                {
                        const CPointMap & pt = pItem->GetContainedPoint();
                        record.SetInt( "position_x", pt.m_x );
                        record.SetInt( "position_y", pt.m_y );
                        record.SetInt( "position_z", pt.m_z );
                }
        }

        return ExecuteQuery( record.BuildInsert( false, true ));
}

bool CWorldStorageMySQL::UpsertWorldObjectData( const CObjBase * pObject, const CGString & serialized )
{
        if ( pObject == NULL )
        {
                return false;
        }

        const CGString sWorldObjectData = GetPrefixedTableName( "world_object_data" );
        UniversalRecord record( *this, sWorldObjectData );

        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();
        record.SetUInt( "object_uid", uid );
        record.SetString( "data", serialized );

        CGString sChecksum = ComputeSerializedChecksum( serialized );
        if ( ! sChecksum.IsEmpty())
        {
                record.SetString( "checksum", sChecksum );
        }
        else
        {
                record.SetNull( "checksum" );
        }

        return ExecuteQuery( record.BuildInsert( true, false ));
}

bool CWorldStorageMySQL::RefreshWorldObjectComponents( const CObjBase * pObject )
{
        if ( pObject == NULL )
        {
                return false;
        }

        const CGString sComponents = GetPrefixedTableName( "world_object_components" );
        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();

        CGString sDelete;
#ifdef _WIN32
        sDelete.Format( "DELETE FROM `%s` WHERE `object_uid` = %I64u;", (const char *) sComponents, uid );
#else
        sDelete.Format( "DELETE FROM `%s` WHERE `object_uid` = %llu;", (const char *) sComponents, uid );
#endif
        if ( ! ExecuteQuery( sDelete ))
        {
                return false;
        }

        std::vector<UniversalRecord> records;
        AppendVarDefComponents( sComponents, uid, pObject->GetTagDefs(), "TAG", records );
        AppendVarDefComponents( sComponents, uid, pObject->GetBaseDefs(), "VAR", records );

        if ( records.empty())
        {
                return true;
        }

        return ExecuteRecordsInsert( records );
}

bool CWorldStorageMySQL::RefreshWorldObjectRelations( const CObjBase * pObject )
{
        if ( pObject == NULL )
        {
                return false;
        }

        const CGString sRelations = GetPrefixedTableName( "world_object_relations" );
        const unsigned long long uid = (unsigned long long) (UINT) pObject->GetUID();

        CGString sDelete;
#ifdef _WIN32
        sDelete.Format( "DELETE FROM `%s` WHERE `parent_uid` = %I64u OR `child_uid` = %I64u;", (const char *) sRelations, uid, uid );
#else
        sDelete.Format( "DELETE FROM `%s` WHERE `parent_uid` = %llu OR `child_uid` = %llu;", (const char *) sRelations, uid, uid );
#endif
        if ( ! ExecuteQuery( sDelete ))
        {
                return false;
        }

        std::vector<UniversalRecord> records;

        if ( pObject->IsItem())
        {
                const CItem * pItem = dynamic_cast<const CItem*>( pObject );
                if ( pItem != NULL )
                {
                        const CObjBase * pContainer = pItem->GetContainer();
                        if ( pContainer != NULL )
                        {
                                UniversalRecord record( *this, sRelations );
                                record.SetUInt( "parent_uid", (unsigned long long) (UINT) pContainer->GetUID());
                                record.SetUInt( "child_uid", uid );
                                record.SetString( "relation", CGString( pItem->IsEquipped() ? "equipped" : "container" ));
                                record.SetInt( "sequence", 0 );
                                records.push_back( record );
                        }
                }
        }

        if ( records.empty())
        {
                return true;
        }

        return ExecuteRecordsInsert( records );
}

bool CWorldStorageMySQL::IsLegacyImportCompleted()
{
        const CGString sTable = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "SELECT `version` FROM `%s` WHERE `id` = %d LIMIT 1;", (const char *) sTable, SCHEMA_IMPORT_ROW );

        MariaDbResult result;
        if ( ! Query( sQuery, &result ))
        {
                return false;
        }

        bool fCompleted = false;
        if ( result.IsValid())
        {
                MariaDbResult::Row pRow = result.FetchRow();
                if ( pRow != NULL && pRow[0] != NULL )
                {
                        fCompleted = ( atoi( pRow[0] ) != 0 );
                }
        }
        return fCompleted;
}

bool CWorldStorageMySQL::SetLegacyImportCompleted()
{
        const CGString sTable = GetPrefixedTableName( "schema_version" );

        CGString sQuery;
        sQuery.Format( "UPDATE `%s` SET `version` = 1 WHERE `id` = %d;", (const char *) sTable, SCHEMA_IMPORT_ROW );
        return ExecuteQuery( sQuery );
}

bool CWorldStorageMySQL::ApplyMigration( int fromVersion )
{
        switch ( fromVersion )
        {
        case 0:
                if ( ! ApplyMigration_0_1())
                {
                        return false;
                }
                if ( ! SetSchemaVersion( 1 ))
                {
                        return false;
                }
                break;

        case 1:
                if ( ! ApplyMigration_1_2())
                {
                        return false;
                }
                if ( ! SetSchemaVersion( 2 ))
                {
                        return false;
                }
                break;

        case 2:
                if ( ! ApplyMigration_2_3())
                {
                        return false;
                }
                if ( ! SetSchemaVersion( 3 ))
                {
                        return false;
                }
                break;

        default:
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Unknown MySQL schema migration from version %d.\n", fromVersion );
                return false;
        }

	return true;
}

bool CWorldStorageMySQL::EnsureSchema()
{
        if ( ! IsConnected())
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Cannot ensure schema without an active MySQL connection.\n" );
                return false;
        }

        if ( ! EnsureSchemaVersionTable())
        {
                return false;
        }

        int iVersion = GetSchemaVersion();
        if ( iVersion < 0 )
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to read MySQL schema version.\n" );
                return false;
        }

        while ( iVersion < CURRENT_SCHEMA_VERSION )
        {
                if ( ! ApplyMigration( iVersion ))
                {
                        return false;
                }
                iVersion = GetSchemaVersion();
                if ( iVersion < 0 )
                {
                        g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to read MySQL schema version after migration.\n" );
                        return false;
                }
        }

        return true;
}

