#pragma once

#include "MySqlStorageService.h"

#include <functional>

class StorageTestFixture
{
public:
        StorageTestFixture();
        ~StorageTestFixture();

        bool Connect( const std::function<void(CServerMySQLConfig &)> & mutator = {} );
        void Disconnect();

        MySqlStorageService & Storage();
        const MySqlStorageService & Storage() const;

        void ResetQueries() const;

private:
        MySqlStorageService m_Storage;
};
