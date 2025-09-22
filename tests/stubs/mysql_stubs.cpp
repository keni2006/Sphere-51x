#include "mysql_stub.h"
#include "graysvr.h"

#include <mysql/mysql.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

CLog g_Log;
CServer g_Serv;

namespace
{
        MYSQL g_stub_mysql;
        MY_CHARSET_INFO g_stub_charset_info;
        char g_error_message[256] = "stub";
        unsigned long g_escape_written = 0;
        bool g_query_called = false;
        std::vector<std::string> g_executed_queries;

        struct StatementData
        {
                std::string query;
                unsigned int last_error = 0;
        };
}

bool WasMysqlQueryCalled()
{
        return g_query_called;
}

void ResetMysqlQueryFlag()
{
        g_query_called = false;
        g_executed_queries.clear();
}

const std::vector<std::string> & GetExecutedMysqlQueries()
{
        return g_executed_queries;
}

void Assert_CheckFail( const char *, const char *, unsigned )
{
        std::abort();
}

extern "C"
{
        unsigned int mysql_num_fields( MYSQL_RES * )
        {
                return 0;
        }

        MYSQL_ROW mysql_fetch_row( MYSQL_RES * )
        {
                return nullptr;
        }

        void mysql_free_result( MYSQL_RES * )
        {
        }

        MYSQL * mysql_init( MYSQL * )
        {
                std::memset( &g_stub_mysql, 0, sizeof( g_stub_mysql ));
                return &g_stub_mysql;
        }

        int mysql_options( MYSQL *, enum mysql_option, const void * )
        {
                return 0;
        }

        unsigned int mysql_errno( MYSQL * )
        {
                return 0;
        }

        MYSQL * mysql_real_connect( MYSQL * mysql, const char *, const char *, const char *, const char *, unsigned int, const char *, unsigned long )
        {
                return mysql;
        }

        int mysql_set_character_set( MYSQL *, const char * )
        {
                return 0;
        }

        unsigned long mysql_real_escape_string( MYSQL *, char * to, const char * from, unsigned long length )
        {
                if ( to != nullptr && from != nullptr )
                {
                        std::memcpy( to, from, length );
                        g_escape_written = length;
                }
                return length;
        }

        const char * mysql_character_set_name( MYSQL * )
        {
                return "utf8mb4";
        }

        void mysql_get_character_set_info( MYSQL *, MY_CHARSET_INFO * cs )
        {
                if ( cs != nullptr )
                {
                        std::memset( cs, 0, sizeof( *cs ));
                }
        }

        const char * mysql_error( MYSQL * )
        {
                return g_error_message;
        }

        int mysql_query( MYSQL *, const char * query )
        {
                g_query_called = true;
                if ( query != nullptr )
                {
                        g_executed_queries.emplace_back( query );
                }
                return 0;
        }

        unsigned int mysql_field_count( MYSQL * )
        {
                return 0;
        }

        MYSQL_RES * mysql_store_result( MYSQL * )
        {
                return nullptr;
        }

        void mysql_close( MYSQL * )
        {
        }

        MYSQL_STMT * mysql_stmt_init( MYSQL * )
        {
                MYSQL_STMT * stmt = new MYSQL_STMT();
                stmt->internal = new StatementData();
                return stmt;
        }

        int mysql_stmt_prepare( MYSQL_STMT * stmt, const char * query, unsigned long length )
        {
                if ( stmt == nullptr )
                {
                        return 1;
                }

                StatementData * data = static_cast<StatementData*>( stmt->internal );
                if ( data == nullptr )
                {
                        return 1;
                }

                if ( query != nullptr )
                {
                        if ( length > 0 )
                        {
                                data->query.assign( query, length );
                        }
                        else
                        {
                                data->query.assign( query );
                        }
                }
                else
                {
                        data->query.clear();
                }
                data->last_error = 0;
                return 0;
        }

        unsigned long mysql_stmt_param_count( MYSQL_STMT * )
        {
                return 0;
        }

        int mysql_stmt_bind_param( MYSQL_STMT *, MYSQL_BIND * )
        {
                        return 0;
        }

        int mysql_stmt_execute( MYSQL_STMT * stmt )
        {
                g_query_called = true;
                if ( stmt != nullptr )
                {
                        StatementData * data = static_cast<StatementData*>( stmt->internal );
                        if ( data != nullptr )
                        {
                                g_executed_queries.emplace_back( data->query );
                        }
                }
                return 0;
        }

        int mysql_stmt_reset( MYSQL_STMT * stmt )
        {
                if ( stmt != nullptr )
                {
                        StatementData * data = static_cast<StatementData*>( stmt->internal );
                        if ( data != nullptr )
                        {
                                data->last_error = 0;
                        }
                }
                return 0;
        }

        int mysql_stmt_close( MYSQL_STMT * stmt )
        {
                if ( stmt != nullptr )
                {
                        StatementData * data = static_cast<StatementData*>( stmt->internal );
                        delete data;
                        delete stmt;
                }
                return 0;
        }

        unsigned int mysql_stmt_errno( MYSQL_STMT * stmt )
        {
                if ( stmt == nullptr )
                {
                        return 0;
                }
                StatementData * data = static_cast<StatementData*>( stmt->internal );
                return ( data != nullptr ) ? data->last_error : 0;
        }

        const char * mysql_stmt_error( MYSQL_STMT * )
        {
                return g_error_message;
        }
}

