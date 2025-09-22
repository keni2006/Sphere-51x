#pragma once

#include "storage_test_fixture.h"
#include "mysql_stub.h"

#include <functional>

class StorageServiceFacade
{
public:
        bool Connect( const std::function<void(CServerMySQLConfig &)> & mutator = {} )
        {
                return m_Fixture.Connect( mutator );
        }

        void Disconnect()
        {
                m_Fixture.Disconnect();
        }

        MySqlStorageService & Service()
        {
                return m_Fixture.Storage();
        }

        const MySqlStorageService & Service() const
        {
                return m_Fixture.Storage();
        }

        void ResetQueryLog() const
        {
                m_Fixture.ResetQueries();
        }

        const std::vector<std::string> & ExecutedQueries() const
        {
                return GetExecutedMysqlQueries();
        }

        const std::vector<ExecutedPreparedStatement> & ExecutedStatements() const
        {
                return GetExecutedPreparedStatements();
        }

private:
        StorageTestFixture m_Fixture;
};
