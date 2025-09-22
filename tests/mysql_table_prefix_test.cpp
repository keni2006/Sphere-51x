#include "graysvr.h"
#include "MySqlStorageService.h"
#include "mysql_stub.h"

#include <iostream>
#include <string>

namespace
{
        std::string BuildRawCp1251Prefix()
        {
                std::string prefix = "\xEC\xE8\xF0_"; // "мир_" in Windows-1251
                prefix += "data";
                return prefix;
        }

        std::string ExpectedUtf8Prefix()
        {
                return std::string( u8"мир_data" );
        }

        bool RunNormalizationTest()
        {
                ResetMysqlQueryFlag();
                g_Log.Clear();

                MySqlStorageService storage;
                CServerMySQLConfig config;
                config.m_fEnable = true;
                std::string cp1251Prefix = BuildRawCp1251Prefix();
                config.m_sTablePrefix = cp1251Prefix.c_str();

                if ( !storage.Connect( config ))
                {
                        std::cerr << "Connect failed when using raw prefix value" << std::endl;
                        return false;
                }

                const std::string expectedPrefix = ExpectedUtf8Prefix();
                const std::string actualPrefix = (const char *) storage.GetTablePrefix();
                if ( actualPrefix != expectedPrefix )
                {
                        std::cerr << "Prefix normalization did not produce the expected UTF-8 value" << std::endl;
                        return false;
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
                        std::cerr << "Normalization log message was not emitted" << std::endl;
                        return false;
                }

                if ( !reasonLogged )
                {
                        std::cerr << "Normalization reason was not logged" << std::endl;
                        return false;
                }

                return true;
        }

        bool RunSchemaCreationTest()
        {
                ResetMysqlQueryFlag();
                g_Log.Clear();

                MySqlStorageService storage;
                CServerMySQLConfig config;
                config.m_fEnable = true;
                std::string cp1251Prefix = BuildRawCp1251Prefix();
                config.m_sTablePrefix = cp1251Prefix.c_str();

                if ( !storage.Connect( config ))
                {
                        std::cerr << "Connect failed when using raw prefix value" << std::endl;
                        return false;
                }

                ResetMysqlQueryFlag();

                CGString createQueryCg = storage.DebugBuildSchemaVersionCreateQuery();
                std::string createQuery = (const char *) createQueryCg;

                if ( !storage.DebugExecuteQuery( createQueryCg ))
                {
                        std::cerr << "Executing schema creation query failed" << std::endl;
                        return false;
                }

                if ( !WasMysqlQueryCalled())
                {
                        std::cerr << "No MySQL queries were executed during schema creation" << std::endl;
                        return false;
                }

                const auto & queries = GetExecutedMysqlQueries();
                const std::string expectedTableName = ExpectedUtf8Prefix() + "schema_version";

                bool foundTableName = false;
                for ( const auto & query : queries )
                {
                        if ( query.find( "CREATE TABLE IF NOT EXISTS`" ) != std::string::npos ||
                                query.find( "CREATE TABLE IF NOT EXISTS `" ) != std::string::npos )
                        {
                                if ( query.find( expectedTableName ) != std::string::npos )
                                {
                                        foundTableName = true;
                                        break;
                                }
                        }
                }

                if ( createQuery.find( expectedTableName ) == std::string::npos )
                {
                        std::cerr << "Normalized table name was not present in constructed schema query" << std::endl;
                        return false;
                }

                if ( !foundTableName )
                {
                        std::cerr << "Normalized table name was not used in CREATE TABLE statement" << std::endl;
                        return false;
                }

                return true;
        }
}

int main()
{
        if ( !RunNormalizationTest())
        {
                return 1;
        }

        if ( !RunSchemaCreationTest())
        {
                return 1;
        }

        return 0;
}

