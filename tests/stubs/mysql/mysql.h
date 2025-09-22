#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct st_mysql
{
        int unused;
} MYSQL;

typedef struct st_mysql_res
{
        int unused;
} MYSQL_RES;

typedef char ** MYSQL_ROW;

typedef unsigned char my_bool;

typedef struct st_mysql_stmt
{
        void * internal;
} MYSQL_STMT;

typedef struct st_mysql_bind
{
        unsigned int buffer_type;
        void * buffer;
        unsigned long buffer_length;
        unsigned long * length;
        my_bool * is_null;
        my_bool is_unsigned;
} MYSQL_BIND;

typedef struct st_mysql_charset_info
{
        unsigned int number;
        const char * name;
        const char * csname;
        const char * collation;
        unsigned int state;
} MY_CHARSET_INFO;

enum enum_field_types
{
        MYSQL_TYPE_DECIMAL,
        MYSQL_TYPE_TINY,
        MYSQL_TYPE_SHORT,
        MYSQL_TYPE_LONG,
        MYSQL_TYPE_FLOAT,
        MYSQL_TYPE_DOUBLE,
        MYSQL_TYPE_NULL,
        MYSQL_TYPE_TIMESTAMP,
        MYSQL_TYPE_LONGLONG,
        MYSQL_TYPE_INT24,
        MYSQL_TYPE_DATE,
        MYSQL_TYPE_TIME,
        MYSQL_TYPE_DATETIME,
        MYSQL_TYPE_YEAR,
        MYSQL_TYPE_NEWDATE,
        MYSQL_TYPE_VARCHAR,
        MYSQL_TYPE_BIT,
        MYSQL_TYPE_TIMESTAMP2,
        MYSQL_TYPE_DATETIME2,
        MYSQL_TYPE_TIME2,
        MYSQL_TYPE_JSON,
        MYSQL_TYPE_NEWDECIMAL,
        MYSQL_TYPE_ENUM,
        MYSQL_TYPE_SET,
        MYSQL_TYPE_TINY_BLOB,
        MYSQL_TYPE_MEDIUM_BLOB,
        MYSQL_TYPE_LONG_BLOB,
        MYSQL_TYPE_BLOB,
        MYSQL_TYPE_VAR_STRING,
        MYSQL_TYPE_STRING,
        MYSQL_TYPE_GEOMETRY
};

enum mysql_option
{
        MYSQL_OPT_CONNECT_TIMEOUT,
        MYSQL_OPT_RECONNECT,
        MYSQL_SET_CHARSET_NAME,
        MYSQL_INIT_COMMAND
};

unsigned int mysql_num_fields( MYSQL_RES * result );
MYSQL_ROW mysql_fetch_row( MYSQL_RES * result );
void mysql_free_result( MYSQL_RES * result );
MYSQL * mysql_init( MYSQL * mysql );
int mysql_options( MYSQL * mysql, enum mysql_option option, const void * arg );
unsigned int mysql_errno( MYSQL * mysql );
MYSQL * mysql_real_connect( MYSQL * mysql, const char * host, const char * user, const char * passwd, const char * db, unsigned int port, const char * unix_socket, unsigned long clientflag );
int mysql_set_character_set( MYSQL * mysql, const char * csname );
unsigned long mysql_real_escape_string( MYSQL * mysql, char * to, const char * from, unsigned long length );
const char * mysql_character_set_name( MYSQL * mysql );
void mysql_get_character_set_info( MYSQL * mysql, MY_CHARSET_INFO * charset );
const char * mysql_error( MYSQL * mysql );
int mysql_query( MYSQL * mysql, const char * query );
unsigned int mysql_field_count( MYSQL * mysql );
MYSQL_RES * mysql_store_result( MYSQL * mysql );
void mysql_close( MYSQL * mysql );

MYSQL_STMT * mysql_stmt_init( MYSQL * mysql );
int mysql_stmt_prepare( MYSQL_STMT * stmt, const char * query, unsigned long length );
unsigned long mysql_stmt_param_count( MYSQL_STMT * stmt );
int mysql_stmt_bind_param( MYSQL_STMT * stmt, MYSQL_BIND * bnd );
int mysql_stmt_execute( MYSQL_STMT * stmt );
int mysql_stmt_reset( MYSQL_STMT * stmt );
int mysql_stmt_close( MYSQL_STMT * stmt );
unsigned int mysql_stmt_errno( MYSQL_STMT * stmt );
const char * mysql_stmt_error( MYSQL_STMT * stmt );

#ifdef __cplusplus
}
#endif

