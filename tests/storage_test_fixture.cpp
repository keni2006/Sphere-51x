#include "storage_test_fixture.h"

#include "mysql_stub.h"
#include "stubs/graysvr.h"

StorageTestFixture::StorageTestFixture() = default;

StorageTestFixture::~StorageTestFixture()
{
        m_Storage.Stop();
}

bool StorageTestFixture::Connect( const std::function<void(CServerMySQLConfig &)> & mutator )
{
        ResetMysqlQueryFlag();

        CServerMySQLConfig config;
        config.m_fEnable = true;
        config.m_sTablePrefix = "test_";
        config.m_sHost = "localhost";
        config.m_sDatabase = "spheretest";
        config.m_sUser = "root";
        config.m_sPassword = "";

        if ( mutator )
        {
                mutator( config );
        }

        return m_Storage.Start( config );
}

void StorageTestFixture::Disconnect()
{
        m_Storage.Stop();
}

MySqlStorageService & StorageTestFixture::Storage()
{
        return m_Storage;
}

const MySqlStorageService & StorageTestFixture::Storage() const
{
        return m_Storage;
}

void StorageTestFixture::ResetQueries() const
{
        ResetMysqlQueryFlag();
}
