#pragma once

#include "../Database.h"
#include "MySqlConnection.h"

#include <memory>
#include <string>

struct CServerMySQLConfig;

namespace Storage
{
namespace MySql
{
        class ConnectionManager
        {
        public:
                struct ConnectionDetails
                {
                        std::string m_TableCharset;
                        std::string m_TableCollation;
                        Storage::DatabaseConfig m_Config;
                };

                ConnectionManager();
                ~ConnectionManager();

                bool Connect( const CServerMySQLConfig & config, ConnectionDetails & outDetails );
                void Disconnect();

                bool IsConnected() const;

                MySqlConnection * GetActiveConnection( MySqlConnectionPool::ScopedConnection & scoped ) const;

                bool BeginTransaction();
                bool CommitTransaction();
                bool RollbackTransaction();

                Storage::DatabaseConfig const & GetConfig() const
                {
                        return m_DatabaseConfig;
                }

        private:
                bool AttemptConnection( const Storage::DatabaseConfig & config, ConnectionDetails & outDetails );

                mutable std::unique_ptr<MySqlConnectionPool> m_ConnectionPool;
                mutable MySqlConnectionPool::ScopedConnection m_TransactionConnection;
                std::unique_ptr<IDatabaseTransaction> m_TransactionGuard;
                Storage::DatabaseConfig m_DatabaseConfig;
                bool m_fConnected;
                bool m_fAutoReconnect;
                int m_iReconnectTries;
                int m_iReconnectDelay;
                int m_iTransactionDepth;
        };
}
}

