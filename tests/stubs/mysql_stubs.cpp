#include "mysql_stub.h"
#include "graysvr.h"

#include <mysql/mysql.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <vector>

CLog g_Log;
CServer g_Serv;
StubWorld g_World;

namespace
{
        struct StubConnection
        {
                bool owns_handle = false;
                unsigned int last_errno = 0;
                std::string last_error;
                std::string charset = "utf8mb4";
        };

        struct StubResultSet
        {
                std::vector<std::vector<std::string>> rows;
                std::vector<std::vector<std::unique_ptr<char[]>>> storage;
                std::vector<std::vector<char*>> pointers;
                size_t cursor = 0;
                unsigned int fieldCount = 0;
        };

        struct StatementData
        {
                std::string query;
                unsigned int last_error = 0;
                unsigned long param_count = 0;
                std::vector<MYSQL_BIND> binds;
        };

        bool g_query_called = false;
        char g_error_message[256] = "stub";
        std::vector<std::string> g_executed_queries;
        std::vector<ExecutedPreparedStatement> g_executed_statements;
        std::deque<std::vector<std::vector<std::string>>> g_pending_results;
        std::string g_last_query;

        bool ShouldReturnResultMetadata()
        {
                if ( g_last_query.empty())
                {
                        return false;
                }

                const char * whitespace = " \t\n\r\f\v";
                size_t first = g_last_query.find_first_not_of( whitespace );
                if ( first == std::string::npos )
                {
                        return false;
                }

                size_t last = g_last_query.find_first_of( whitespace, first );
                std::string keyword = g_last_query.substr( first, last == std::string::npos ? std::string::npos : last - first );
                for ( char & ch : keyword )
                {
                        ch = static_cast<char>( std::toupper( static_cast<unsigned char>( ch )));
                }

                if ( keyword == "SELECT" || keyword == "WITH" || keyword == "SHOW" || keyword == "DESCRIBE" )
                {
                        return true;
                }

                return false;
        }

        StubResultSet * CreateResultSet( const std::vector<std::vector<std::string>> & rows )
        {
                auto * result = new StubResultSet();
                result->rows = rows;
                result->fieldCount = 0;

                for ( const auto & row : rows )
                {
                        if ( row.size() > result->fieldCount )
                        {
                                result->fieldCount = static_cast<unsigned int>( row.size());
                        }
                }

                result->storage.resize( rows.size());
                result->pointers.resize( rows.size());

                for ( size_t i = 0; i < rows.size(); ++i )
                {
                        const auto & row = rows[i];
                        auto & storage = result->storage[i];
                        auto & pointers = result->pointers[i];

                        storage.resize( result->fieldCount );
                        pointers.resize( result->fieldCount, nullptr );

                        for ( unsigned int j = 0; j < result->fieldCount; ++j )
                        {
                                std::string value;
                                if ( j < row.size())
                                {
                                        value = row[j];
                                }

                                std::unique_ptr<char[]> buffer( new char[value.size() + 1] );
                                std::memcpy( buffer.get(), value.c_str(), value.size() + 1 );
                                pointers[j] = buffer.get();
                                storage[j] = std::move( buffer );
                        }
                }

                return result;
        }

        std::string ConvertBindValue( const MYSQL_BIND & bind )
        {
                if ( bind.is_null != nullptr && bind.is_null[0] )
                {
                        return "NULL";
                }

                switch ( bind.buffer_type )
                {
                        case MYSQL_TYPE_NULL:
                                return "NULL";

                        case MYSQL_TYPE_STRING:
                        case MYSQL_TYPE_VAR_STRING:
                        case MYSQL_TYPE_BLOB:
                        {
                                unsigned long length = bind.length ? *bind.length : bind.buffer_length;
                                const char * data = static_cast<const char *>( bind.buffer );
                                return std::string( data, data + length );
                        }

                        case MYSQL_TYPE_TINY:
                        {
                                if ( bind.buffer == nullptr )
                                {
                                        return "0";
                                }
                                unsigned char value = *static_cast<unsigned char *>( bind.buffer );
                                return std::to_string( static_cast<unsigned int>( value ));
                        }

                        case MYSQL_TYPE_LONGLONG:
                        {
                                if ( bind.buffer == nullptr )
                                {
                                        return "0";
                                }

                                if ( bind.is_unsigned )
                                {
                                        unsigned long long value = 0;
                                        std::memcpy( &value, bind.buffer, sizeof( unsigned long long ));
                                        return std::to_string( value );
                                }

                                long long value = 0;
                                std::memcpy( &value, bind.buffer, sizeof( long long ));
                                return std::to_string( value );
                        }

                        default:
                        {
                                unsigned long length = bind.length ? *bind.length : bind.buffer_length;
                                const char * data = static_cast<const char *>( bind.buffer );
                                return std::string( data, data + length );
                        }
                }
        }

        StubConnection * RequireConnection( MYSQL * handle )
        {
                if ( handle == nullptr )
                {
                        return nullptr;
                }
                return static_cast<StubConnection*>( handle->internal );
        }
}

bool WasMysqlQueryCalled()
{
        return g_query_called;
}

void ResetMysqlQueryFlag()
{
        g_query_called = false;
        g_executed_queries.clear();
        g_executed_statements.clear();
        g_pending_results.clear();
        g_last_query.clear();
}

const std::vector<std::string> & GetExecutedMysqlQueries()
{
        return g_executed_queries;
}

const std::vector<ExecutedPreparedStatement> & GetExecutedPreparedStatements()
{
        return g_executed_statements;
}

void PushMysqlResultSet( const std::vector<std::vector<std::string>> & rows )
{
        g_pending_results.push_back( rows );
}

void ClearMysqlResults()
{
        g_pending_results.clear();
        g_last_query.clear();
}

void Assert_CheckFail( const char *, const char *, unsigned )
{
        std::abort();
}

extern "C"
{
        unsigned int mysql_num_fields( MYSQL_RES * result )
        {
                if ( result == nullptr || result->internal == nullptr )
                {
                        return 0;
                }

                StubResultSet * stub = static_cast<StubResultSet*>( result->internal );
                return stub->fieldCount;
        }

        MYSQL_ROW mysql_fetch_row( MYSQL_RES * result )
        {
                if ( result == nullptr || result->internal == nullptr )
                {
                        return nullptr;
                }

                StubResultSet * stub = static_cast<StubResultSet*>( result->internal );
                if ( stub->cursor >= stub->pointers.size())
                {
                        return nullptr;
                }

                MYSQL_ROW row = stub->pointers[stub->cursor].data();
                ++stub->cursor;
                return row;
        }

        void mysql_free_result( MYSQL_RES * result )
        {
                if ( result == nullptr )
                {
                        return;
                }

                StubResultSet * stub = static_cast<StubResultSet*>( result->internal );
                delete stub;
                delete result;
        }

        MYSQL * mysql_init( MYSQL * mysql )
        {
                MYSQL * handle = mysql;
                bool owns_handle = false;
                if ( handle == nullptr )
                {
                        handle = new MYSQL();
                        owns_handle = true;
                }

                StubConnection * connection = new StubConnection();
                connection->owns_handle = owns_handle;
                handle->internal = connection;
                return handle;
        }

        int mysql_options( MYSQL *, enum mysql_option, const void * )
        {
                return 0;
        }

        unsigned int mysql_errno( MYSQL * mysql )
        {
                StubConnection * connection = RequireConnection( mysql );
                return connection ? connection->last_errno : 0;
        }

        MYSQL * mysql_real_connect( MYSQL * mysql, const char *, const char *, const char *, const char *, unsigned int, const char *, unsigned long )
        {
                return mysql;
        }

        int mysql_set_character_set( MYSQL * mysql, const char * name )
        {
                StubConnection * connection = RequireConnection( mysql );
                if ( connection != nullptr && name != nullptr )
                {
                        connection->charset = name;
                }
                return 0;
        }

        unsigned long mysql_real_escape_string( MYSQL *, char * to, const char * from, unsigned long length )
        {
                if ( to != nullptr && from != nullptr )
                {
                        std::memcpy( to, from, length );
                }
                return length;
        }

        const char * mysql_character_set_name( MYSQL * mysql )
        {
                StubConnection * connection = RequireConnection( mysql );
                if ( connection != nullptr )
                {
                        return connection->charset.c_str();
                }
                return "utf8mb4";
        }

        void mysql_get_character_set_info( MYSQL * mysql, MY_CHARSET_INFO * info )
        {
                if ( info == nullptr )
                {
                        return;
                }

                StubConnection * connection = RequireConnection( mysql );
                info->number = 0;
                info->name = connection ? connection->charset.c_str() : "utf8mb4";
                info->csname = info->name;
                info->collation = "utf8mb4_general_ci";
                info->state = 0;
        }

        const char * mysql_error( MYSQL * mysql )
        {
                StubConnection * connection = RequireConnection( mysql );
                if ( connection != nullptr && !connection->last_error.empty())
                {
                        return connection->last_error.c_str();
                }
                return g_error_message;
        }

        int mysql_query( MYSQL *, const char * query )
        {
                g_query_called = true;
                if ( query != nullptr )
                {
                        g_last_query = query;
                        g_executed_queries.emplace_back( query );
                }
                else
                {
                        g_last_query.clear();
                }
                return 0;
        }

        unsigned int mysql_field_count( MYSQL * )
        {
                if ( !ShouldReturnResultMetadata())
                {
                        return 0;
                }
                if ( g_pending_results.empty())
                {
                        return 0;
                }
                const auto & rows = g_pending_results.front();
                unsigned int count = 0;
                for ( const auto & row : rows )
                {
                        if ( row.size() > count )
                        {
                                count = static_cast<unsigned int>( row.size());
                        }
                }
                return count;
        }

        MYSQL_RES * mysql_store_result( MYSQL * )
        {
                if ( g_pending_results.empty())
                {
                        return nullptr;
                }

                std::vector<std::vector<std::string>> rows = std::move( g_pending_results.front());
                g_pending_results.pop_front();
                g_last_query.clear();

                StubResultSet * stub = CreateResultSet( rows );
                MYSQL_RES * result = new MYSQL_RES();
                result->internal = stub;
                return result;
        }

        void mysql_close( MYSQL * mysql )
        {
                if ( mysql == nullptr )
                {
                        return;
                }

                StubConnection * connection = RequireConnection( mysql );
                bool owns_handle = connection ? connection->owns_handle : false;
                delete connection;
                mysql->internal = nullptr;
                if ( owns_handle )
                {
                        delete mysql;
                }
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

                data->param_count = static_cast<unsigned long>( std::count( data->query.begin(), data->query.end(), '?' ));
                data->binds.resize( data->param_count );
                data->last_error = 0;
                return 0;
        }

        unsigned long mysql_stmt_param_count( MYSQL_STMT * stmt )
        {
                if ( stmt == nullptr || stmt->internal == nullptr )
                {
                        return 0;
                }

                StatementData * data = static_cast<StatementData*>( stmt->internal );
                return data->param_count;
        }

        int mysql_stmt_bind_param( MYSQL_STMT * stmt, MYSQL_BIND * binds )
        {
                if ( stmt == nullptr || stmt->internal == nullptr )
                {
                        return 1;
                }

                StatementData * data = static_cast<StatementData*>( stmt->internal );
                if ( binds == nullptr )
                {
                        data->binds.assign( data->param_count, MYSQL_BIND{} );
                        return 0;
                }

                data->binds.assign( binds, binds + data->param_count );
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

                                ExecutedPreparedStatement execution;
                                execution.query = data->query;
                                execution.parameters.reserve( data->param_count );
                                for ( unsigned long i = 0; i < data->param_count; ++i )
                                {
                                        execution.parameters.push_back( ConvertBindValue( data->binds[i] ));
                                }
                                g_executed_statements.push_back( std::move( execution ));
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

