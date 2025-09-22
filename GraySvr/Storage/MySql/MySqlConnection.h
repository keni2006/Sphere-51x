#pragma once

#include "Storage/Database.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <errmsg.h>
#include <mysqld_error.h>
#include <mysql.h>
#else
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>
#include <mysql/mysql.h>
#endif

#ifndef MYSQL_OPT_SET_CHARSET_NAME
#define MYSQL_OPT_SET_CHARSET_NAME MYSQL_SET_CHARSET_NAME
#endif

namespace Storage
{
namespace MySql
{
        class MySqlResult : public IDatabaseResult
        {
        public:
                MySqlResult() noexcept = default;
                explicit MySqlResult( MYSQL_RES * result ) noexcept;

                bool IsValid() const noexcept override;
                unsigned int GetFieldCount() const noexcept override;
                Row FetchRow() override;

        private:
                struct ResultDeleter
                {
                        void operator()( MYSQL_RES * result ) const noexcept;
                };

                std::unique_ptr<MYSQL_RES, ResultDeleter> m_Handle;
        };

        class MySqlStatement : public IDatabaseStatement
        {
        public:
                MySqlStatement( MYSQL * connection, const std::string & query );
                ~MySqlStatement() override;

                void Reset() override;
                void BindNull( size_t index ) override;
                void BindBool( size_t index, bool value ) override;
                void BindInt64( size_t index, long long value ) override;
                void BindUInt64( size_t index, unsigned long long value ) override;
                void BindString( size_t index, const std::string & value ) override;
                void BindBinary( size_t index, const void * data, size_t length ) override;
                void Execute() override;

                MYSQL_STMT * GetHandle() const noexcept
                {
                        return m_Statement.get();
                }

        private:
                void EnsureIndex( size_t index ) const;
                void EnsureStorage( size_t index );
                void BindRawData( size_t index, enum enum_field_types type, const void * data, size_t length, bool isUnsigned );
                void BindIntegral( size_t index, long long value, bool isUnsigned );

                struct StatementDeleter
                {
                        void operator()( MYSQL_STMT * stmt ) const noexcept;
                };

                std::unique_ptr<MYSQL_STMT, StatementDeleter> m_Statement;
                std::vector<MYSQL_BIND> m_Binds;
                std::vector<std::vector<unsigned char>> m_Buffers;
                std::vector<unsigned long> m_Lengths;
                std::vector<my_bool> m_IsNull;
        };

        class MySqlConnection;

        class MySqlTransaction : public IDatabaseTransaction
        {
        public:
                explicit MySqlTransaction( MySqlConnection & connection );
                ~MySqlTransaction() override;

                void Commit() override;
                void Rollback() override;
                bool IsActive() const noexcept override
                {
                        return m_Active;
                }

        private:
                MySqlConnection & m_Connection;
                bool m_Active;
        };

        class MySqlConnection : public IDatabaseConnection
        {
        public:
                MySqlConnection();
                ~MySqlConnection() override;

                void Open( const DatabaseConfig & config ) override;
                void Close() noexcept override;
                bool IsOpen() const noexcept override;

                void Execute( const std::string & query ) override;
                std::unique_ptr<IDatabaseResult> Query( const std::string & query ) override;
                std::unique_ptr<IDatabaseStatement> Prepare( const std::string & query ) override;
                std::unique_ptr<IDatabaseTransaction> BeginTransaction() override;
                std::string EscapeString( const char * input, size_t length ) override;
                DatabaseCharacterSetInfo GetCharacterSetInfo() const override;

                MYSQL * GetHandle() const noexcept
                {
                        return m_Handle.get();
                }

                void SetOption( enum mysql_option option, const void * value );
                void SetStringOption( enum mysql_option option, const std::string & value );
                void ConfigureCharacterSet( const std::string & charset, const std::string & collation );

        private:
                MYSQL * RequireHandle( const char * context ) const;

                struct HandleDeleter
                {
                        void operator()( MYSQL * handle ) const noexcept;
                };

                std::unique_ptr<MYSQL, HandleDeleter> m_Handle;
                std::vector<std::string> m_StringOptions;
                DatabaseConfig m_Config;
        };

        class MySqlConnectionPool
        {
        public:
                class ScopedConnection
                {
                public:
                        ScopedConnection() noexcept;
                        ScopedConnection( MySqlConnectionPool & pool, std::unique_ptr<MySqlConnection> connection ) noexcept;
                        ~ScopedConnection();

                        ScopedConnection( const ScopedConnection & ) = delete;
                        ScopedConnection & operator=( const ScopedConnection & ) = delete;

                        ScopedConnection( ScopedConnection && other ) noexcept;
                        ScopedConnection & operator=( ScopedConnection && other ) noexcept;

                        MySqlConnection * operator->() const noexcept
                        {
                                return m_Connection.get();
                        }

                        MySqlConnection & Get() const;
                        bool IsValid() const noexcept;
                        void Reset();

                private:
                        MySqlConnectionPool * m_Pool;
                        std::unique_ptr<MySqlConnection> m_Connection;
                };

                MySqlConnectionPool();
                MySqlConnectionPool( const DatabaseConfig & config, size_t maxConnections );
                ~MySqlConnectionPool();

                void Configure( const DatabaseConfig & config, size_t maxConnections );
                void AddConnection( std::unique_ptr<MySqlConnection> connection );
                ScopedConnection Acquire();
                void Shutdown();

        private:
                friend class ScopedConnection;
                std::unique_ptr<MySqlConnection> CreateConnection();
                void Release( std::unique_ptr<MySqlConnection> connection );

                DatabaseConfig m_Config;
                size_t m_MaxConnections;
                std::mutex m_Mutex;
                std::condition_variable m_Condition;
                std::vector<std::unique_ptr<MySqlConnection>> m_IdleConnections;
                size_t m_ActiveConnections;
                bool m_ShuttingDown;
        };
}
}

