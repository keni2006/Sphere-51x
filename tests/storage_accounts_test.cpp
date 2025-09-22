#include "storage_test_facade.h"
#include "mysql_stub.h"
#include "stubs/graysvr.h"
#include "test_harness.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
        CAccount BuildSampleAccount()
        {
                CAccount account;
                account.SetName( "alpha" );
                account.SetPassword( "secret" );
                account.SetPrivLevel( 3 );
                account.m_PrivFlags = PRIV_BLOCKED | PRIV_JAILED;
                account.m_sComment = "test account";
                account.m_sEMail = "alpha@example.com";
                account.m_sChatName = "Alpha";
                account.m_lang[0] = 'e';
                account.m_lang[1] = 'n';
                account.m_lang[2] = 'g';
                account.m_Total_Connect_Time = 120;
                account.m_Last_Connect_Time = 15;
                account.m_Last_IP = "127.0.0.1";
                account.m_First_IP = "10.0.0.1";
                account.m_Last_Connect_Date = { 124, 0, 1, 10, 30, 0 };
                account.m_First_Connect_Date = { 124, 0, 1, 8, 0, 0 };
                account.m_uidLastChar.SetValue( 0x01020304u );
                account.m_iEmailFailures = 1;
                account.m_EMailSchedule.push_back( 101 );
                account.m_EMailSchedule.push_back( 202 );
                return account;
        }
}

TEST_CASE( TestUpsertAccountCreatesAndUpdatesRows )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();

        CAccount account = BuildSampleAccount();

        PushMysqlResultSet({ { "42" } });

        if ( !storage.Service().UpsertAccount( account ))
        {
                throw std::runtime_error( "UpsertAccount returned false" );
        }

        const auto & queries = storage.ExecutedQueries();
        const std::string accountTable = "`test_accounts`";
        const bool hasInsert = std::any_of( queries.begin(), queries.end(), [&]( const std::string & query )
        {
                return query.find( "INSERT INTO" ) != std::string::npos && query.find( accountTable ) != std::string::npos;
        });

        if ( !hasInsert )
        {
                throw std::runtime_error( "Account insert query was not executed" );
        }

        auto insertIt = std::find_if( queries.begin(), queries.end(), [&]( const std::string & query )
        {
                return query.find( "INSERT INTO" ) != std::string::npos && query.find( accountTable ) != std::string::npos;
        });

        if ( insertIt == queries.end())
        {
                throw std::runtime_error( "Account insert query contents were not captured" );
        }

        const std::string & insertQuery = *insertIt;

        if ( insertQuery.find( "'eng'" ) == std::string::npos )
        {
                throw std::runtime_error( "Language code was not written to the account insert" );
        }

        if ( insertQuery.find( "'127.0.0.1'" ) == std::string::npos )
        {
                throw std::runtime_error( "Last IP address missing from account insert" );
        }

        if ( insertQuery.find( "'10.0.0.1'" ) == std::string::npos )
        {
                throw std::runtime_error( "First IP address missing from account insert" );
        }

        const std::string emailTable = "`test_account_emails`";
        const bool clearedEmails = std::any_of( queries.begin(), queries.end(), [&]( const std::string & query )
        {
                return query.find( "DELETE FROM" ) != std::string::npos && query.find( emailTable ) != std::string::npos;
        });

        if ( !clearedEmails )
        {
                throw std::runtime_error( "Existing email schedule was not cleared" );
        }

        const size_t insertedEmails = std::count_if( queries.begin(), queries.end(), [&]( const std::string & query )
        {
                return query.find( "INSERT INTO" ) != std::string::npos && query.find( emailTable ) != std::string::npos;
        });

        if ( insertedEmails != account.m_EMailSchedule.GetCount())
        {
                throw std::runtime_error( "Email schedule was not reinserted correctly" );
        }
}

TEST_CASE( TestDeleteAccountRemovesRows )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();

        if ( !storage.Service().DeleteAccount( "alpha" ))
        {
                throw std::runtime_error( "DeleteAccount returned false" );
        }

        const auto & queries = storage.ExecutedQueries();
        const bool deletedFromAccounts = std::any_of( queries.begin(), queries.end(), []( const std::string & query )
        {
                return query.find( "DELETE FROM" ) != std::string::npos && query.find( "`test_accounts`" ) != std::string::npos;
        });

        if ( !deletedFromAccounts )
        {
                throw std::runtime_error( "Account rows were not deleted" );
        }

        const bool deletedFromEmails = std::any_of( queries.begin(), queries.end(), []( const std::string & query )
        {
                return query.find( "DELETE FROM" ) != std::string::npos && query.find( "`test_account_emails`" ) != std::string::npos;
        });

        if ( !deletedFromEmails )
        {
                throw std::runtime_error( "Email schedule rows were not deleted" );
        }
}
