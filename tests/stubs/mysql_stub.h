#pragma once

#include <string>
#include <vector>

bool WasMysqlQueryCalled();
void ResetMysqlQueryFlag();
const std::vector<std::string> & GetExecutedMysqlQueries();

struct ExecutedPreparedStatement
{
        std::string query;
        std::vector<std::string> parameters;
};

const std::vector<ExecutedPreparedStatement> & GetExecutedPreparedStatements();
void PushMysqlResultSet( const std::vector<std::vector<std::string>> & rows );
void ClearMysqlResults();

