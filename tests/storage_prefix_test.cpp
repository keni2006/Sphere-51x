#include "storage_test_facade.h"
#include "mysql_stub.h"
#include "stubs/graysvr.h"
#include "test_harness.h"

#include <stdexcept>
#include <string>

namespace
{
        std::string BuildRawCp1251Prefix()
        {
                std::string prefix = "\xEC\xE8\xF0_";
                prefix += "data";
                return prefix;
        }

        std::string ExpectedUtf8Prefix()
        {
                return std::string( reinterpret_cast<const char*>( u8"мир_data" ));
        }
}

TEST_CASE( TestTablePrefixNormalization )
{
        g_Log.Clear();

        StorageServiceFacade storage;
        bool connected = storage.Connect([]( CServerMySQLConfig & config )
        {
                config.m_sTablePrefix = BuildRawCp1251Prefix().c_str();
        });

        if ( !connected )
        {
                throw std::runtime_error( "Failed to initialize storage with CP1251 prefix" );
        }

        const std::string expectedPrefix = ExpectedUtf8Prefix();
        const std::string actualPrefix = (const char *) storage.Service().GetTablePrefix();
        if ( actualPrefix != expectedPrefix )
        {
                throw std::runtime_error( "MySQL prefix was not normalized to UTF-8" );
        }

        bool normalizationLogged = false;
        bool reasonLogged = false;
        for ( const auto & entry : g_Log.Events())
        {
                if ( entry.m_message.find( "Normalized MySQL table prefix bytes" ) != std::string::npos )
                {
                        normalizationLogged = true;
                }
                if ( entry.m_message.find( "Converted Windows-1251 bytes to UTF-8" ) != std::string::npos )
                {
                        reasonLogged = true;
                }
        }

        if ( !normalizationLogged )
        {
                throw std::runtime_error( "Normalization log message missing" );
        }

        if ( !reasonLogged )
        {
                throw std::runtime_error( "Normalization reason log missing" );
        }
}

TEST_CASE( TestSchemaCreationUsesNormalizedPrefix )
{
        g_Log.Clear();

        StorageServiceFacade storage;
        bool connected = storage.Connect([]( CServerMySQLConfig & config )
        {
                config.m_sTablePrefix = BuildRawCp1251Prefix().c_str();
        });

        if ( !connected )
        {
                throw std::runtime_error( "Failed to connect storage" );
        }

        storage.ResetQueryLog();

        CGString createQueryCg = storage.Service().DebugBuildSchemaVersionCreateQuery();
        if ( !storage.Service().DebugExecuteQuery( createQueryCg ))
        {
                throw std::runtime_error( "Executing schema creation query failed" );
        }

        if ( !WasMysqlQueryCalled())
        {
                throw std::runtime_error( "Schema creation did not execute any queries" );
        }

        const std::string expectedTableName = ExpectedUtf8Prefix() + "schema_version";
        const std::string createQuery = (const char *) createQueryCg;

        if ( createQuery.find( expectedTableName ) == std::string::npos )
        {
                throw std::runtime_error( "Normalized table name missing from CREATE TABLE statement" );
        }

        bool foundNormalizedQuery = false;
        for ( const auto & query : GetExecutedMysqlQueries())
        {
                if ( query.find( expectedTableName ) != std::string::npos )
                {
                        foundNormalizedQuery = true;
                        break;
                }
        }

        if ( !foundNormalizedQuery )
        {
                throw std::runtime_error( "Normalized table name not used when executing schema query" );
        }
}

