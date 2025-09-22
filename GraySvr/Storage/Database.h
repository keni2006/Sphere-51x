#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Storage
{
        struct DatabaseConfig
        {
                bool m_Enable = false;
                std::string m_Host;
                unsigned int m_Port = 0;
                std::string m_Database;
                std::string m_Username;
                std::string m_Password;
                std::string m_Charset;
                std::string m_Collation;
                bool m_AutoReconnect = true;
                unsigned int m_ReconnectTries = 1;
                unsigned int m_ReconnectDelaySeconds = 1;
                unsigned int m_ConnectTimeoutSeconds = 0;

                DatabaseConfig() = default;
        };

        class DatabaseError : public std::runtime_error
        {
        public:
                DatabaseError( std::string context, unsigned int code, std::string message );

                const std::string & GetContext() const noexcept
                {
                        return m_Context;
                }

                unsigned int GetCode() const noexcept
                {
                        return m_Code;
                }

        private:
                std::string m_Context;
                unsigned int m_Code;
        };

        struct DatabaseCharacterSetInfo
        {
                std::string m_Charset;
                std::string m_Collation;
        };

        class IDatabaseResult
        {
        public:
                using Row = const char * const *;

                virtual ~IDatabaseResult() = default;

                virtual bool IsValid() const noexcept = 0;
                virtual unsigned int GetFieldCount() const noexcept = 0;
                virtual Row FetchRow() = 0;
        };

        class IDatabaseStatement
        {
        public:
                virtual ~IDatabaseStatement() = default;

                virtual void Reset() = 0;
                virtual void BindNull( size_t index ) = 0;
                virtual void BindBool( size_t index, bool value ) = 0;
                virtual void BindInt64( size_t index, long long value ) = 0;
                virtual void BindUInt64( size_t index, unsigned long long value ) = 0;
                virtual void BindString( size_t index, const std::string & value ) = 0;
                virtual void BindBinary( size_t index, const void * data, size_t length ) = 0;
                virtual void Execute() = 0;
        };

        class IDatabaseTransaction
        {
        public:
                virtual ~IDatabaseTransaction() = default;

                virtual void Commit() = 0;
                virtual void Rollback() = 0;
                virtual bool IsActive() const noexcept = 0;
        };

        class IDatabaseConnection
        {
        public:
                virtual ~IDatabaseConnection() = default;

                virtual void Open( const DatabaseConfig & config ) = 0;
                virtual void Close() noexcept = 0;
                virtual bool IsOpen() const noexcept = 0;

                virtual void Execute( const std::string & query ) = 0;
                virtual std::unique_ptr<IDatabaseResult> Query( const std::string & query ) = 0;
                virtual std::unique_ptr<IDatabaseStatement> Prepare( const std::string & query ) = 0;
                virtual std::unique_ptr<IDatabaseTransaction> BeginTransaction() = 0;
                virtual std::string EscapeString( const char * input, size_t length ) = 0;
                virtual DatabaseCharacterSetInfo GetCharacterSetInfo() const = 0;
        };
}

