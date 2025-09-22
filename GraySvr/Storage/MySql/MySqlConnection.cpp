#include "MySqlConnection.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace
{
        bool IsSafeIdentifierToken( const std::string & token )
        {
                if ( token.empty())
                {
                        return false;
                }

                for ( char ch : token )
                {
                        unsigned char uch = static_cast<unsigned char>( ch );
                        if (( uch != '_' ) && !std::isalnum( uch ))
                        {
                                return false;
                        }
                }

                return true;
        }

        std::string BuildSetNamesCommand( const std::string & charset, const std::string & collation )
        {
                if ( charset.empty() || !IsSafeIdentifierToken( charset ))
                {
                        return std::string();
                }

                std::string command = "SET NAMES '";
                command += charset;
                command += "'";

                if ( !collation.empty() && IsSafeIdentifierToken( collation ))
                {
                        command += " COLLATE '";
                        command += collation;
                        command += "'";
                }

                return command;
        }

        Storage::DatabaseError MakeDatabaseError( const char * context, MYSQL * handle )
        {
                unsigned int code = 0;
                std::string message;

                if ( handle != NULL )
                {
                        code = mysql_errno( handle );
                        const char * pszError = mysql_error( handle );
                        if ( pszError != NULL && pszError[0] != '\0' )
                        {
                                message = pszError;
                        }
                }

                if ( message.empty())
                {
                        message = ( handle == NULL ) ? "No active MySQL connection" : "Unknown MySQL client error";
                }

                return Storage::DatabaseError( context != NULL ? context : "mysql", code, message );
        }

        Storage::DatabaseError MakeStatementError( const char * context, MYSQL_STMT * stmt )
        {
                unsigned int code = 0;
                std::string message;

                if ( stmt != NULL )
                {
                        code = mysql_stmt_errno( stmt );
                        const char * pszError = mysql_stmt_error( stmt );
                        if ( pszError != NULL && pszError[0] != '\0' )
                        {
                                message = pszError;
                        }
                }

                if ( message.empty())
                {
                        message = ( stmt == NULL ) ? "No active MySQL statement" : "Unknown MySQL statement error";
                }

                return Storage::DatabaseError( context != NULL ? context : "mysql_stmt", code, message );
        }
}

namespace Storage
{
namespace MySql
{
        void MySqlResult::ResultDeleter::operator()( MYSQL_RES * result ) const noexcept
        {
                if ( result != NULL )
                {
                        mysql_free_result( result );
                }
        }

        MySqlResult::MySqlResult( MYSQL_RES * result ) noexcept :
                m_Handle( result )
        {
        }

        bool MySqlResult::IsValid() const noexcept
        {
                return m_Handle.get() != NULL;
        }

        unsigned int MySqlResult::GetFieldCount() const noexcept
        {
                MYSQL_RES * handle = m_Handle.get();
                return ( handle != NULL ) ? mysql_num_fields( handle ) : 0;
        }

        IDatabaseResult::Row MySqlResult::FetchRow()
        {
                MYSQL_RES * handle = m_Handle.get();
                if ( handle == NULL )
                {
                        return NULL;
                }

                MYSQL_ROW row = mysql_fetch_row( handle );
                return reinterpret_cast<IDatabaseResult::Row>( row );
        }

        void MySqlStatement::StatementDeleter::operator()( MYSQL_STMT * stmt ) const noexcept
        {
                if ( stmt != NULL )
                {
                        mysql_stmt_close( stmt );
                }
        }

        MySqlStatement::MySqlStatement( MYSQL * connection, const std::string & query )
        {
                if ( connection == NULL )
                {
                        throw Storage::DatabaseError( "mysql_stmt_init", 0, "Connection handle is null" );
                }

                MYSQL_STMT * stmt = mysql_stmt_init( connection );
                if ( stmt == NULL )
                {
                        throw MakeDatabaseError( "mysql_stmt_init", connection );
                }

                m_Statement.reset( stmt );

                if ( mysql_stmt_prepare( stmt, query.c_str(), static_cast<unsigned long>( query.size())) != 0 )
                {
                        throw MakeStatementError( "mysql_stmt_prepare", stmt );
                }

                unsigned long parameterCount = mysql_stmt_param_count( stmt );
                m_Binds.resize( parameterCount );
                m_Buffers.resize( parameterCount );
                m_Lengths.resize( parameterCount );
                m_IsNull.resize( parameterCount );

                for ( unsigned long i = 0; i < parameterCount; ++i )
                {
                        std::memset( &m_Binds[i], 0, sizeof( MYSQL_BIND ));
                        m_Lengths[i] = 0;
                        m_IsNull[i] = 0;
                }
        }

        MySqlStatement::~MySqlStatement() = default;

        void MySqlStatement::EnsureIndex( size_t index ) const
        {
                if ( index >= m_Binds.size())
                {
                        throw Storage::DatabaseError( "mysql_stmt_bind_param", 0, "Parameter index out of range" );
                }
        }

        void MySqlStatement::EnsureStorage( size_t index )
        {
                EnsureIndex( index );
                if ( index >= m_Buffers.size())
                {
                        m_Buffers.resize( index + 1 );
                }
                if ( index >= m_Lengths.size())
                {
                        m_Lengths.resize( index + 1 );
                }
                if ( index >= m_IsNull.size())
                {
                        m_IsNull.resize( index + 1 );
                }
        }

        void MySqlStatement::BindRawData( size_t index, enum enum_field_types type, const void * data, size_t length, bool isUnsigned )
        {
                EnsureStorage( index );

                MYSQL_BIND & bind = m_Binds[index];
                std::memset( &bind, 0, sizeof( MYSQL_BIND ));
                bind.buffer_type = type;
                bind.is_unsigned = isUnsigned ? 1 : 0;
                bind.is_null = &m_IsNull[index];
                bind.length = &m_Lengths[index];

                if ( data != NULL && length > 0 )
                {
                        const unsigned char * bytes = static_cast<const unsigned char *>( data );
                        m_Buffers[index].assign( bytes, bytes + length );
                        bind.buffer = m_Buffers[index].data();
                        bind.buffer_length = static_cast<unsigned long>( m_Buffers[index].size());
                        m_Lengths[index] = static_cast<unsigned long>( m_Buffers[index].size());
                        m_IsNull[index] = 0;
                }
                else
                {
                        m_Buffers[index].clear();
                        bind.buffer = NULL;
                        bind.buffer_length = 0;
                        m_Lengths[index] = 0;
                        m_IsNull[index] = 0;
                }
        }

        void MySqlStatement::BindIntegral( size_t index, long long value, bool isUnsigned )
        {
                EnsureStorage( index );

                MYSQL_BIND & bind = m_Binds[index];
                std::memset( &bind, 0, sizeof( MYSQL_BIND ));
                bind.buffer_type = MYSQL_TYPE_LONGLONG;
                bind.is_unsigned = isUnsigned ? 1 : 0;
                bind.is_null = &m_IsNull[index];
                bind.length = &m_Lengths[index];

                m_Buffers[index].resize( sizeof( long long ));
                std::memcpy( m_Buffers[index].data(), &value, sizeof( long long ));

                bind.buffer = m_Buffers[index].data();
                bind.buffer_length = static_cast<unsigned long>( m_Buffers[index].size());
                m_Lengths[index] = static_cast<unsigned long>( m_Buffers[index].size());
                m_IsNull[index] = 0;
        }

        void MySqlStatement::Reset()
        {
                MYSQL_STMT * stmt = m_Statement.get();
                if ( stmt == NULL )
                {
                        return;
                }

                if ( mysql_stmt_reset( stmt ) != 0 )
                {
                        throw MakeStatementError( "mysql_stmt_reset", stmt );
                }

                for ( size_t i = 0; i < m_Binds.size(); ++i )
                {
                        m_Lengths[i] = 0;
                        m_IsNull[i] = 0;
                }
        }

        void MySqlStatement::BindNull( size_t index )
        {
                EnsureStorage( index );

                MYSQL_BIND & bind = m_Binds[index];
                std::memset( &bind, 0, sizeof( MYSQL_BIND ));
                bind.buffer_type = MYSQL_TYPE_NULL;
                bind.buffer = NULL;
                bind.buffer_length = 0;
                bind.is_unsigned = 0;
                bind.is_null = &m_IsNull[index];
                bind.length = &m_Lengths[index];
                m_IsNull[index] = 1;
                m_Lengths[index] = 0;
                m_Buffers[index].clear();
        }

        void MySqlStatement::BindBool( size_t index, bool value )
        {
                unsigned char byte = value ? 1 : 0;
                BindRawData( index, MYSQL_TYPE_TINY, &byte, sizeof( byte ), false );
        }

        void MySqlStatement::BindInt64( size_t index, long long value )
        {
                BindIntegral( index, value, false );
        }

        void MySqlStatement::BindUInt64( size_t index, unsigned long long value )
        {
                long long stored = 0;
                std::memcpy( &stored, &value, sizeof( unsigned long long ));
                BindIntegral( index, stored, true );
        }

        void MySqlStatement::BindString( size_t index, const std::string & value )
        {
                BindRawData( index, MYSQL_TYPE_STRING, value.data(), value.size(), false );
        }

        void MySqlStatement::BindBinary( size_t index, const void * data, size_t length )
        {
                BindRawData( index, MYSQL_TYPE_BLOB, data, length, true );
        }

        void MySqlStatement::Execute()
        {
                MYSQL_STMT * stmt = m_Statement.get();
                if ( stmt == NULL )
                {
                        throw Storage::DatabaseError( "mysql_stmt_execute", 0, "Statement is not prepared" );
                }

                if ( !m_Binds.empty())
                {
                        if ( mysql_stmt_bind_param( stmt, &m_Binds[0] ) != 0 )
                        {
                                throw MakeStatementError( "mysql_stmt_bind_param", stmt );
                        }
                }

                if ( mysql_stmt_execute( stmt ) != 0 )
                {
                        throw MakeStatementError( "mysql_stmt_execute", stmt );
                }
        }

        MySqlTransaction::MySqlTransaction( MySqlConnection & connection ) :
                m_Connection( connection ),
                m_Active( true )
        {
        }

        MySqlTransaction::~MySqlTransaction()
        {
                if ( m_Active )
                {
                        try
                        {
                                m_Connection.Execute( "ROLLBACK" );
                        }
                        catch ( const std::exception & )
                        {
                        }
                }
        }

        void MySqlTransaction::Commit()
        {
                if ( !m_Active )
                {
                        return;
                }

                m_Connection.Execute( "COMMIT" );
                m_Active = false;
        }

        void MySqlTransaction::Rollback()
        {
                if ( !m_Active )
                {
                        return;
                }

                m_Connection.Execute( "ROLLBACK" );
                m_Active = false;
        }

        MySqlConnection::MySqlConnection() = default;

        MySqlConnection::~MySqlConnection()
        {
                Close();
        }

        MYSQL * MySqlConnection::RequireHandle( const char * context ) const
        {
                MYSQL * handle = m_Handle.get();
                if ( handle == NULL )
                {
                        throw Storage::DatabaseError( context != NULL ? context : "mysql", 0, "No active MySQL connection" );
                }
                return handle;
        }

        void MySqlConnection::SetOption( enum mysql_option option, const void * value )
        {
                MYSQL * handle = RequireHandle( "mysql_options" );
                if ( mysql_options( handle, option, value ) != 0 )
                {
                        throw MakeDatabaseError( "mysql_options", handle );
                }
        }

        void MySqlConnection::SetStringOption( enum mysql_option option, const std::string & value )
        {
                if ( value.empty())
                {
                        return;
                }

                MYSQL * handle = RequireHandle( "mysql_options" );

                m_StringOptions.push_back( value );
                const char * pszValue = m_StringOptions.back().c_str();
                if ( mysql_options( handle, option, pszValue ) != 0 )
                {
                        m_StringOptions.pop_back();
                        throw MakeDatabaseError( "mysql_options", handle );
                }
        }

        void MySqlConnection::ConfigureCharacterSet( const std::string & charset, const std::string & collation )
        {
                if ( charset.empty())
                {
                        return;
                }

                MYSQL * handle = RequireHandle( "mysql_set_character_set" );
                if ( mysql_set_character_set( handle, charset.c_str()) != 0 )
                {
                        throw MakeDatabaseError( "mysql_set_character_set", handle );
                }

                const std::string command = BuildSetNamesCommand( charset, collation );
                if ( !command.empty())
                {
                        Execute( command );
                }
        }

        void MySqlConnection::Open( const DatabaseConfig & config )
        {
                m_Config = config;

                Close();

                MYSQL * handle = mysql_init( NULL );
                if ( handle == NULL )
                {
                        throw Storage::DatabaseError( "mysql_init", CR_OUT_OF_MEMORY, "mysql_init failed" );
                }

                m_Handle.reset( handle );
                m_StringOptions.clear();

                if ( config.m_ConnectTimeoutSeconds > 0 )
                {
                        unsigned int timeout = config.m_ConnectTimeoutSeconds;
                        SetOption( MYSQL_OPT_CONNECT_TIMEOUT, &timeout );
                }

                if ( config.m_AutoReconnect )
                {
                        char reconnect = 1;
                        SetOption( MYSQL_OPT_RECONNECT, &reconnect );
                }

                if ( !config.m_Charset.empty())
                {
                        SetStringOption( MYSQL_OPT_SET_CHARSET_NAME, config.m_Charset );
                }

                const std::string initCommand = BuildSetNamesCommand( config.m_Charset, config.m_Collation );
                if ( !initCommand.empty())
                {
                        SetStringOption( MYSQL_INIT_COMMAND, initCommand );
                }

                const char * host = config.m_Host.empty() ? NULL : config.m_Host.c_str();
                const char * user = config.m_Username.empty() ? NULL : config.m_Username.c_str();
                const char * password = config.m_Password.empty() ? NULL : config.m_Password.c_str();
                const char * database = config.m_Database.empty() ? NULL : config.m_Database.c_str();
                unsigned int port = config.m_Port;

                MYSQL * result = mysql_real_connect( handle, host, user, password, database, port, NULL, 0 );
                if ( result == NULL )
                {
                        throw MakeDatabaseError( "mysql_real_connect", handle );
                }

                if ( !config.m_Charset.empty())
                {
                        try
                        {
                                ConfigureCharacterSet( config.m_Charset, config.m_Collation );
                        }
                        catch ( const Storage::DatabaseError & )
                        {
                        }
                }
        }

        void MySqlConnection::Close() noexcept
        {
                m_StringOptions.clear();
                m_Handle.reset();
        }

        bool MySqlConnection::IsOpen() const noexcept
        {
                return m_Handle.get() != NULL;
        }

        void MySqlConnection::Execute( const std::string & query )
        {
                if ( query.empty())
                {
                        return;
                }

                MYSQL * handle = RequireHandle( "mysql_query" );
                if ( mysql_query( handle, query.c_str()) != 0 )
                {
                        throw MakeDatabaseError( "mysql_query", handle );
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
                                throw MakeDatabaseError( "mysql_store_result", handle );
                        }
                }
        }

        std::unique_ptr<IDatabaseResult> MySqlConnection::Query( const std::string & query )
        {
                MYSQL * handle = RequireHandle( "mysql_query" );
                if ( mysql_query( handle, query.c_str()) != 0 )
                {
                        throw MakeDatabaseError( "mysql_query", handle );
                }

                MYSQL_RES * result = mysql_store_result( handle );
                if ( result == NULL && mysql_errno( handle ) != 0 )
                {
                        throw MakeDatabaseError( "mysql_store_result", handle );
                }

                return std::unique_ptr<IDatabaseResult>( new MySqlResult( result ));
        }

        std::unique_ptr<IDatabaseStatement> MySqlConnection::Prepare( const std::string & query )
        {
                MYSQL * handle = RequireHandle( "mysql_stmt_init" );
                return std::unique_ptr<IDatabaseStatement>( new MySqlStatement( handle, query ));
        }

        std::unique_ptr<IDatabaseTransaction> MySqlConnection::BeginTransaction()
        {
                Execute( "START TRANSACTION" );
                return std::unique_ptr<IDatabaseTransaction>( new MySqlTransaction( *this ));
        }

        std::string MySqlConnection::EscapeString( const char * input, size_t length )
        {
                if ( input == NULL || length == 0 )
                {
                        return std::string();
                }

                MYSQL * handle = RequireHandle( "mysql_real_escape_string" );

                std::string buffer;
                buffer.resize( length * 2 + 1 );
                unsigned long written = mysql_real_escape_string( handle, &buffer[0], input, static_cast<unsigned long>( length ) );
                buffer.resize( written );
                return buffer;
        }

        DatabaseCharacterSetInfo MySqlConnection::GetCharacterSetInfo() const
        {
                DatabaseCharacterSetInfo info;
                MYSQL * handle = m_Handle.get();
                if ( handle == NULL )
                {
                        return info;
                }

                MY_CHARSET_INFO csInfo;
                mysql_get_character_set_info( handle, &csInfo );
                if ( csInfo.csname != NULL )
                {
                        info.m_Charset = csInfo.csname;
                }
                if ( csInfo.name != NULL )
                {
                        info.m_Collation = csInfo.name;
                }

                return info;
        }

        void MySqlConnection::HandleDeleter::operator()( MYSQL * handle ) const noexcept
        {
                if ( handle != NULL )
                {
                        mysql_close( handle );
                }
        }

        MySqlConnectionPool::ScopedConnection::ScopedConnection() noexcept :
                m_Pool( NULL )
        {
        }

        MySqlConnectionPool::ScopedConnection::ScopedConnection( MySqlConnectionPool & pool, std::unique_ptr<MySqlConnection> connection ) noexcept :
                m_Pool( &pool ),
                m_Connection( std::move( connection ))
        {
        }

        MySqlConnectionPool::ScopedConnection::~ScopedConnection()
        {
                Reset();
        }

        MySqlConnectionPool::ScopedConnection::ScopedConnection( ScopedConnection && other ) noexcept :
                m_Pool( other.m_Pool ),
                m_Connection( std::move( other.m_Connection ))
        {
                other.m_Pool = NULL;
        }

        MySqlConnectionPool::ScopedConnection & MySqlConnectionPool::ScopedConnection::operator=( ScopedConnection && other ) noexcept
        {
                if ( this != &other )
                {
                        Reset();
                        m_Pool = other.m_Pool;
                        other.m_Pool = NULL;
                        m_Connection = std::move( other.m_Connection );
                }
                return *this;
        }

        MySqlConnection & MySqlConnectionPool::ScopedConnection::Get() const
        {
                if ( m_Connection == NULL )
                {
                        throw std::runtime_error( "Connection is not available" );
                }
                return *m_Connection;
        }

        bool MySqlConnectionPool::ScopedConnection::IsValid() const noexcept
        {
                return m_Connection != NULL;
        }

        void MySqlConnectionPool::ScopedConnection::Reset()
        {
                if ( m_Pool != NULL && m_Connection != NULL )
                {
                        m_Pool->Release( std::move( m_Connection ));
                        m_Pool = NULL;
                }
                else
                {
                        m_Connection.reset();
                        m_Pool = NULL;
                }
        }

        MySqlConnectionPool::MySqlConnectionPool() :
                m_MaxConnections( 1 ),
                m_ActiveConnections( 0 ),
                m_ShuttingDown( false )
        {
        }

        MySqlConnectionPool::MySqlConnectionPool( const DatabaseConfig & config, size_t maxConnections ) :
                m_Config( config ),
                m_MaxConnections( std::max<size_t>( maxConnections, 1 ) ),
                m_ActiveConnections( 0 ),
                m_ShuttingDown( false )
        {
        }

        MySqlConnectionPool::~MySqlConnectionPool()
        {
                Shutdown();
        }

        void MySqlConnectionPool::Configure( const DatabaseConfig & config, size_t maxConnections )
        {
                std::unique_lock<std::mutex> lock( m_Mutex );
                m_Config = config;
                m_MaxConnections = std::max<size_t>( maxConnections, 1 );
                m_IdleConnections.clear();
                m_ActiveConnections = 0;
                m_ShuttingDown = false;
        }

        std::unique_ptr<MySqlConnection> MySqlConnectionPool::CreateConnection()
        {
                if ( !m_Config.m_Enable )
                {
                        throw Storage::DatabaseError( "mysql_pool", 0, "MySQL connections are disabled" );
                }

                std::unique_ptr<MySqlConnection> connection( new MySqlConnection() );
                connection->Open( m_Config );
                return connection;
        }

        void MySqlConnectionPool::AddConnection( std::unique_ptr<MySqlConnection> connection )
        {
                if ( connection == NULL )
                {
                        return;
                }

                std::unique_lock<std::mutex> lock( m_Mutex );
                if ( m_ShuttingDown )
                {
                        lock.unlock();
                        return;
                }
                m_IdleConnections.push_back( std::move( connection ));
                lock.unlock();
                m_Condition.notify_one();
        }

        MySqlConnectionPool::ScopedConnection MySqlConnectionPool::Acquire()
        {
                std::unique_lock<std::mutex> lock( m_Mutex );

                while ( true )
                {
                        if ( m_ShuttingDown )
                        {
                                throw Storage::DatabaseError( "mysql_pool", 0, "Connection pool is shutting down" );
                        }

                        if ( !m_IdleConnections.empty())
                        {
                                std::unique_ptr<MySqlConnection> connection = std::move( m_IdleConnections.back());
                                m_IdleConnections.pop_back();
                                ++m_ActiveConnections;
                                lock.unlock();
                                return ScopedConnection( *this, std::move( connection ));
                        }

                        size_t totalConnections = m_ActiveConnections + m_IdleConnections.size();
                        if ( totalConnections < m_MaxConnections )
                        {
                                ++m_ActiveConnections;
                                lock.unlock();
                                try
                                {
                                        std::unique_ptr<MySqlConnection> connection = CreateConnection();
                                        return ScopedConnection( *this, std::move( connection ));
                                }
                                catch ( ... )
                                {
                                        lock.lock();
                                        if ( m_ActiveConnections > 0 )
                                        {
                                                --m_ActiveConnections;
                                        }
                                        lock.unlock();
                                        throw;
                                }
                        }

                        m_Condition.wait( lock );
                }
        }

        void MySqlConnectionPool::Release( std::unique_ptr<MySqlConnection> connection )
        {
                std::unique_lock<std::mutex> lock( m_Mutex );

                if ( m_ActiveConnections > 0 )
                {
                        --m_ActiveConnections;
                }

                if ( m_ShuttingDown || connection == NULL )
                {
                        lock.unlock();
                        m_Condition.notify_one();
                        return;
                }

                m_IdleConnections.push_back( std::move( connection ));
                lock.unlock();
                m_Condition.notify_one();
        }

        void MySqlConnectionPool::Shutdown()
        {
                std::unique_lock<std::mutex> lock( m_Mutex );
                m_ShuttingDown = true;
                m_IdleConnections.clear();
                lock.unlock();
                m_Condition.notify_all();
        }
}
}

