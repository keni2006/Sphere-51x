#include "graysvr.h"
#include "CWorldStorageMySQL.h"
#include "mysql_stub.h"

#include <cassert>
#include <iostream>
#include <string>

int main()
{
        ResetMysqlQueryFlag();
        g_Log.Clear();

        CWorldStorageMySQL storage;
        CServerMySQLConfig config;
        config.m_fEnable = true;
        std::string badPrefix( 1, static_cast<char>( 0xE1 ));
        badPrefix += "bad_prefix";
        config.m_sTablePrefix = badPrefix.c_str();

        bool connected = storage.Connect( config );
        if ( connected )
        {
                std::cerr << "Connect unexpectedly succeeded with invalid prefix" << std::endl;
                return 1;
        }

        if ( !storage.GetTablePrefix().IsEmpty())
        {
                std::cerr << "Table prefix was not cleared after failed normalization" << std::endl;
                return 1;
        }

        if ( WasMysqlQueryCalled())
        {
                std::cerr << "Schema queries were executed despite invalid prefix" << std::endl;
                return 1;
        }

        bool logged = false;
        for ( const auto & entry : g_Log.Events())
        {
                if ( entry.m_message.find( "Invalid MySQL table prefix" ) != std::string::npos )
                {
                        logged = true;
                        break;
                }
        }

        if ( !logged )
        {
                std::cerr << "Expected error log was not emitted" << std::endl;
                return 1;
        }

        return 0;
}

