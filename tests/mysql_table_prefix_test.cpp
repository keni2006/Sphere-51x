#include "graysvr.h"
#include "CWorldStorageMySQL.h"
#include "mysql_stub.h"

#include <iostream>
#include <iomanip>
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
                std::cerr << "Connect failed when prefix required recoding" << std::endl;
                return 1;
        }

        std::string expectedPrefix = "\xD0\xBC\xD0\xB8\xD1\x80_data"; // "мир_data" in UTF-8
        std::string actualPrefix = (const char *) storage.GetTablePrefix();
        if ( actualPrefix != expectedPrefix )
        {
                std::cerr << "Prefix was not converted to UTF-8 as expected" << std::endl;
                std::cerr << "Actual bytes: ";
                std::cerr << std::setfill( '0' );
                for ( unsigned char ch : actualPrefix )
                {
                        std::cerr << std::hex << std::setw( 2 ) << static_cast<int>( ch ) << ' ';
                }
                std::cerr << std::setfill( ' ' ) << std::dec << std::endl;
                return 1;
        }

        bool conversionLogged = false;
        for ( const auto & entry : g_Log.Events())
        {
                if ( entry.m_message.find( "Normalized MySQL table prefix bytes" ) != std::string::npos )
                {
                        conversionLogged = true;
                        break;
                }
        }

        if ( !conversionLogged )
        {
                std::cerr << "Expected normalization log message was not emitted" << std::endl;
                return 1;
        }

        return 0;
}

