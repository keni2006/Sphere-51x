#include "storage_test_facade.h"
#include "mysql_stub.h"
#include "stubs/graysvr.h"
#include "test_harness.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <cctype>
#include <vector>
#include <iterator>

namespace
{
        std::string TrimWhitespace( const std::string & value )
        {
                const std::string::size_type first = value.find_first_not_of( " \t\r\n" );
                if ( first == std::string::npos )
                {
                        return std::string();
                }
                const std::string::size_type last = value.find_last_not_of( " \t\r\n" );
                return value.substr( first, last - first + 1 );
        }

        std::string StripMatchingQuotes( const std::string & value )
        {
                if ( value.size() >= 2 && value.front() == value.back() && ( value.front() == '`' || value.front() == '\'' ))
                {
                        return value.substr( 1, value.size() - 2 );
                }
                return value;
        }

        std::vector<std::string> SplitCommaSeparated( const std::string & text )
        {
                std::vector<std::string> parts;
                std::string current;
                bool inSingleQuotes = false;
                for ( char ch : text )
                {
                        if ( ch == '\'' )
                        {
                                inSingleQuotes = !inSingleQuotes;
                                current.push_back( ch );
                                continue;
                        }
                        if ( ch == ',' && !inSingleQuotes )
                        {
                                parts.push_back( current );
                                current.clear();
                                continue;
                        }
                        current.push_back( ch );
                }
                parts.push_back( current );
                return parts;
        }

        std::vector<std::string> ExtractColumnsFromInsert( const std::string & query )
        {
                const std::string::size_type open = query.find( '(' );
                if ( open == std::string::npos )
                {
                        return {};
                }
                const std::string::size_type close = query.find( ')', open );
                if ( close == std::string::npos )
                {
                        return {};
                }
                const std::string segment = query.substr( open + 1, close - open - 1 );
                std::vector<std::string> columns;
                for ( const std::string & token : SplitCommaSeparated( segment ))
                {
                        columns.push_back( StripMatchingQuotes( TrimWhitespace( token )));
                }
                return columns;
        }

        std::vector<std::string> ExtractValuesFromInsert( const std::string & query )
        {
                const std::string marker = "VALUES (";
                const std::string::size_type start = query.find( marker );
                if ( start == std::string::npos )
                {
                        return {};
                }
                const std::string::size_type open = start + marker.size();
                const std::string::size_type close = query.find( ')', open );
                if ( close == std::string::npos )
                {
                        return {};
                }
                const std::string segment = query.substr( open, close - open );
                std::vector<std::string> values;
                for ( const std::string & token : SplitCommaSeparated( segment ))
                {
                        values.push_back( StripMatchingQuotes( TrimWhitespace( token )));
                }
                return values;
        }

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

TEST_CASE( TestNewAccountInsertUsesZeroCounters )
{
        StorageServiceFacade storage;
        if ( !storage.Connect())
        {
                throw std::runtime_error( "Unable to initialize storage" );
        }

        storage.ResetQueryLog();

        CAccount account;
        account.SetName( "beta" );
        account.SetPassword( "" );
        account.SetPrivLevel( 0 );

        PushMysqlResultSet({ { "7" } });

        if ( !storage.Service().UpsertAccount( account ))
        {
                throw std::runtime_error( "UpsertAccount returned false" );
        }

        const auto & queries = storage.ExecutedQueries();
        const std::string accountTable = "`test_accounts`";
        auto insertIt = std::find_if( queries.begin(), queries.end(), [&]( const std::string & query )
        {
                return query.find( "INSERT INTO" ) != std::string::npos && query.find( accountTable ) != std::string::npos;
        });

        if ( insertIt == queries.end())
        {
                throw std::runtime_error( "Account insert query contents were not captured" );
        }

        const std::vector<std::string> columns = ExtractColumnsFromInsert( *insertIt );
        const std::vector<std::string> values = ExtractValuesFromInsert( *insertIt );

        if ( columns.empty() || values.empty() )
        {
                throw std::runtime_error( "Failed to parse account insert query" );
        }

        if ( columns.size() != values.size())
        {
                throw std::runtime_error( "Account insert column/value mismatch" );
        }

        auto assertColumnZero = [&]( const std::string & column )
        {
                const auto it = std::find( columns.begin(), columns.end(), column );
                if ( it == columns.end())
                {
                        throw std::runtime_error( "Column '" + column + "' missing from account insert" );
                }

                const std::size_t index = static_cast<std::size_t>( std::distance( columns.begin(), it ));
                if ( values[index] != "0" )
                {
                        throw std::runtime_error( "Column '" + column + "' expected to be zero but was '" + values[index] + "'" );
                }
        };

        assertColumnZero( "total_connect_time" );
        assertColumnZero( "last_connect_time" );
        assertColumnZero( "email_failures" );
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
