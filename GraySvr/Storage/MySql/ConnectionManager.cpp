#include "ConnectionManager.h"

#include "MySqlLogging.h"
#if defined(UNIT_TEST)
#include "../../../tests/stubs/graysvr.h"
#else
#include "../../graysvr.h"
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <new>
#include <string>
#include <thread>

namespace Storage
{
namespace MySql
{
        namespace
        {
                void TrimWhitespace( std::string & value )
                {
                        const char * whitespace = " \t\n\r\f\v";
                        size_t begin = value.find_first_not_of( whitespace );
                        if ( begin == std::string::npos )
                        {
                                value.clear();
                                return;
                        }
                        size_t endPos = value.find_last_not_of( whitespace );
                        value = value.substr( begin, endPos - begin + 1 );
                }

                void ToLowerAscii( std::string & value )
                {
                        std::transform( value.begin(), value.end(), value.begin(), []( unsigned char ch ) -> char
                        {
                                return static_cast<char>( std::tolower( ch ));
                        });
                }
        }

        ConnectionManager::ConnectionManager() :
                m_fConnected( false ),
                m_fAutoReconnect( false ),
                m_iReconnectTries( 0 ),
                m_iReconnectDelay( 0 ),
                m_iTransactionDepth( 0 )
        {
        }

        ConnectionManager::~ConnectionManager()
        {
                Disconnect();
        }

        bool ConnectionManager::Connect( const CServerMySQLConfig & config, ConnectionDetails & outDetails )
        {
                Disconnect();

                if ( ! config.m_fEnable )
                {
                        return false;
                }

                m_fAutoReconnect = config.m_fAutoReconnect;
                m_iReconnectTries = config.m_iReconnectTries;
                m_iReconnectDelay = config.m_iReconnectDelay;
                m_iTransactionDepth = 0;

                std::string requestedCharset;
                std::string requestedCollation;
                if ( !config.m_sCharset.IsEmpty())
                {
                        requestedCharset = static_cast<const char *>( config.m_sCharset );
                        TrimWhitespace( requestedCharset );
                        size_t separator = requestedCharset.find_first_of( " \t\n\r\f\v" );
                        if ( separator != std::string::npos )
                        {
                                requestedCollation = requestedCharset.substr( separator + 1 );
                                requestedCharset.erase( separator );
                                TrimWhitespace( requestedCollation );
                        }
                        TrimWhitespace( requestedCharset );
                }

                std::string sessionCharset = requestedCharset;
                std::string sessionCollation = requestedCollation;

                ToLowerAscii( sessionCharset );
                ToLowerAscii( sessionCollation );

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

                const char * pszHost = config.m_sHost.IsEmpty() ? NULL : static_cast<const char *>( config.m_sHost );
                const char * pszUser = config.m_sUser.IsEmpty() ? NULL : static_cast<const char *>( config.m_sUser );
                const char * pszPassword = config.m_sPassword.IsEmpty() ? NULL : static_cast<const char *>( config.m_sPassword );
                const char * pszDatabase = config.m_sDatabase.IsEmpty() ? NULL : static_cast<const char *>( config.m_sDatabase );
                unsigned int uiPort = ( config.m_iPort > 0 ) ? static_cast<unsigned int>( config.m_iPort ) : 0u;

                Storage::DatabaseConfig dbConfig;
                dbConfig.m_Enable = true;
                dbConfig.m_Host = ( pszHost != NULL ) ? pszHost : std::string();
                dbConfig.m_Port = uiPort;
                dbConfig.m_Database = ( pszDatabase != NULL ) ? pszDatabase : std::string();
                dbConfig.m_Username = ( pszUser != NULL ) ? pszUser : std::string();
                dbConfig.m_Password = ( pszPassword != NULL ) ? pszPassword : std::string();
                dbConfig.m_Charset = sessionCharset;
                dbConfig.m_Collation = sessionCollation;
                dbConfig.m_AutoReconnect = m_fAutoReconnect;
                dbConfig.m_ReconnectTries = ( m_iReconnectTries > 0 ) ? static_cast<unsigned int>( m_iReconnectTries ) : 1u;
                dbConfig.m_ReconnectDelaySeconds = ( m_iReconnectDelay > 0 ) ? static_cast<unsigned int>( m_iReconnectDelay ) : 1u;

                const int attempts = std::max( m_fAutoReconnect ? m_iReconnectTries : 1, 1 );
                for ( int attempt = 0; attempt < attempts; ++attempt )
                {
                        try
                        {
                                if ( AttemptConnection( dbConfig, outDetails ))
                                {
                                        const char * pszActiveCharset = outDetails.m_TableCharset.empty() ? "unknown" : outDetails.m_TableCharset.c_str();
                                        std::string collationSuffix;
                                        if ( !outDetails.m_TableCollation.empty())
                                        {
                                                collationSuffix = ", collation '";
                                                collationSuffix += outDetails.m_TableCollation;
                                                collationSuffix += "'";
                                        }

                                        const char * pszEffectiveHost = ( pszHost != NULL && pszHost[0] != '\0' ) ? pszHost : "localhost";
                                        g_Log.Event( LOGM_INIT|LOGL_EVENT, "Connected to MySQL server %s:%u using character set '%s'%s.",
                                                pszEffectiveHost,
                                                uiPort,
                                                pszActiveCharset,
                                                collationSuffix.c_str());
                                        return true;
                                }
                        }
                        catch ( const Storage::DatabaseError & ex )
                        {
                                LogDatabaseError( ex, LOGL_ERROR );
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

        bool ConnectionManager::AttemptConnection( const Storage::DatabaseConfig & dbConfig, ConnectionDetails & outDetails )
        {
                std::unique_ptr<MySqlConnection> connection( new MySqlConnection() );
                connection->Open( dbConfig );

                Storage::DatabaseCharacterSetInfo activeInfo = connection->GetCharacterSetInfo();
                if ( !dbConfig.m_Charset.empty())
                {
                        outDetails.m_TableCharset = dbConfig.m_Charset;
                }
                else
                {
                        outDetails.m_TableCharset = activeInfo.m_Charset;
                }

                if ( !dbConfig.m_Collation.empty())
                {
                        outDetails.m_TableCollation = dbConfig.m_Collation;
                }
                else
                {
                        outDetails.m_TableCollation = activeInfo.m_Collation;
                }

                if ( !m_ConnectionPool )
                {
                        m_ConnectionPool.reset( new MySqlConnectionPool( dbConfig, 1 ));
                }
                else
                {
                        m_ConnectionPool->Configure( dbConfig, 1 );
                }
                m_ConnectionPool->AddConnection( std::move( connection ));

                m_DatabaseConfig = dbConfig;
                outDetails.m_Config = dbConfig;
                m_fConnected = true;
                return true;
        }

        bool ConnectionManager::Reconnect( ConnectionDetails & outDetails )
        {
                if ( !m_DatabaseConfig.m_Enable )
                {
                        return false;
                }

                Storage::DatabaseConfig dbConfig = m_DatabaseConfig;

                m_TransactionGuard.reset();
                m_TransactionConnection.Reset();
                if ( m_ConnectionPool )
                {
                        m_ConnectionPool->Shutdown();
                        m_ConnectionPool.reset();
                }

                m_fConnected = false;
                m_iTransactionDepth = 0;

                const unsigned int attempts = std::max<unsigned int>( dbConfig.m_ReconnectTries > 0 ? dbConfig.m_ReconnectTries : 1u, 1u );
                for ( unsigned int attempt = 0; attempt < attempts; ++attempt )
                {
                        try
                        {
                                if ( AttemptConnection( dbConfig, outDetails ))
                                {
                                        return true;
                                }
                        }
                        catch ( const Storage::DatabaseError & ex )
                        {
                                LogDatabaseError( ex, LOGL_ERROR );
                                if ( ( attempt + 1 ) < attempts )
                                {
                                        unsigned int delay = std::max<unsigned int>( dbConfig.m_ReconnectDelaySeconds > 0 ? dbConfig.m_ReconnectDelaySeconds : 1u, 1u );
                                        g_Log.Event( GetMySQLErrorLogMask( LOGL_WARN ), "Retrying MySQL reconnection in %u second(s).", delay );
                                        std::this_thread::sleep_for( std::chrono::seconds( delay ));
                                }
                        }
                        catch ( const std::bad_alloc & )
                        {
                                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "Out of memory while attempting MySQL reconnection." );
                                break;
                        }
                }

                g_Log.Event( GetMySQLErrorLogMask( LOGL_ERROR ), "Unable to reconnect to MySQL server after %u attempt(s).", attempts );
                return false;
        }

        void ConnectionManager::Disconnect()
        {
                m_TransactionGuard.reset();
                m_TransactionConnection.Reset();
                if ( m_ConnectionPool )
                {
                        m_ConnectionPool->Shutdown();
                        m_ConnectionPool.reset();
                }
                m_fConnected = false;
                m_DatabaseConfig = Storage::DatabaseConfig();
                m_iTransactionDepth = 0;
        }

        bool ConnectionManager::IsConnected() const
        {
                return m_fConnected;
        }

        MySqlConnection * ConnectionManager::GetActiveConnection( MySqlConnectionPool::ScopedConnection & scoped ) const
        {
                if ( m_TransactionConnection.IsValid())
                {
                        return &m_TransactionConnection.Get();
                }

                if ( !m_ConnectionPool )
                {
                        return NULL;
                }

                scoped = m_ConnectionPool->Acquire();
                if ( !scoped.IsValid())
                {
                        return NULL;
                }

                return &scoped.Get();
        }

        bool ConnectionManager::BeginTransaction()
        {
                if ( !IsConnected())
                {
                        return false;
                }

                if ( m_iTransactionDepth == 0 )
                {
                        try
                        {
                                if ( !m_ConnectionPool )
                                {
                                        return false;
                                }

                                m_TransactionConnection.Reset();
                                m_TransactionConnection = m_ConnectionPool->Acquire();
                                if ( !m_TransactionConnection.IsValid())
                                {
                                        return false;
                                }
                                m_TransactionGuard = m_TransactionConnection->BeginTransaction();
                        }
                        catch ( const Storage::DatabaseError & ex )
                        {
                                LogDatabaseError( ex, LOGL_ERROR );
                                m_TransactionConnection.Reset();
                                m_TransactionGuard.reset();
                                return false;
                        }
                        catch ( ... )
                        {
                                m_TransactionConnection.Reset();
                                m_TransactionGuard.reset();
                                throw;
                        }
                }

                ++m_iTransactionDepth;
                return true;
        }

        bool ConnectionManager::CommitTransaction()
        {
                if ( !IsConnected())
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
                                if ( m_TransactionGuard )
                                {
                                        m_TransactionGuard->Commit();
                                }
                        }
                        catch ( const Storage::DatabaseError & ex )
                        {
                                LogDatabaseError( ex, LOGL_ERROR );
                                m_iTransactionDepth = 0;
                                m_TransactionGuard.reset();
                                m_TransactionConnection.Reset();
                                return false;
                        }

                        m_TransactionGuard.reset();
                        m_TransactionConnection.Reset();
                }
                return true;
        }

        bool ConnectionManager::RollbackTransaction()
        {
                if ( !IsConnected())
                {
                        return false;
                }
                if ( m_iTransactionDepth <= 0 )
                {
                        return true;
                }

                try
                {
                        if ( m_TransactionGuard )
                        {
                                m_TransactionGuard->Rollback();
                        }
                }
                catch ( const Storage::DatabaseError & ex )
                {
                        LogDatabaseError( ex, LOGL_ERROR );
                        m_iTransactionDepth = 0;
                        m_TransactionGuard.reset();
                        m_TransactionConnection.Reset();
                        return false;
                }

                m_iTransactionDepth = 0;
                m_TransactionGuard.reset();
                m_TransactionConnection.Reset();
                return true;
        }
}
}

