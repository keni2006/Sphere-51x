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

typedef struct st_mysql_charset_info
{
        unsigned int number;
        const char * name;
        const char * csname;
        const char * collation;
        unsigned int state;
} MY_CHARSET_INFO;

enum mysql_option
{
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

#ifdef __cplusplus
}
#endif

