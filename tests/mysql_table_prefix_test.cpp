#include "graysvr.h"
#include "CWorldStorageMySQL.h"
#include "mysql_stub.h"

#include <iostream>
#include <string>

int main()
{
        ResetMysqlQueryFlag();
        g_Log.Clear();

        CWorldStorageMySQL storage;
        CServerMySQLConfig config;
        config.m_fEnable = true;
        std::string cp1251Prefix = "\xEC\xE8\xF0_"; // "мир_" in Windows-1251
        cp1251Prefix += "data";
        config.m_sTablePrefix = cp1251Prefix.c_str();

        bool connected = storage.Connect( config );
        if ( !connected )
        {
                std::cerr << "Connect failed when using raw prefix value" << std::endl;
                return 1;
        }

        std::string expectedPrefix = cp1251Prefix;
        std::string actualPrefix = (const char *) storage.GetTablePrefix();
        if ( actualPrefix != expectedPrefix )
        {
                std::cerr << "Prefix was altered unexpectedly" << std::endl;
                return 1;
        }

        bool normalizationLogged = false;
        for ( const auto & entry : g_Log.Events())
        {
                if ( entry.m_message.find( "Normalized MySQL table prefix bytes" ) != std::string::npos )
                {
                        normalizationLogged = true;
                        break;
                }
        }

        if ( normalizationLogged )
        {
                std::cerr << "Normalization log message was unexpectedly emitted" << std::endl;
                return 1;
        }

        return 0;
}

