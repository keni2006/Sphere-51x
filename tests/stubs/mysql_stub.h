#pragma once

#include <string>
#include <vector>

bool WasMysqlQueryCalled();
void ResetMysqlQueryFlag();
const std::vector<std::string> & GetExecutedMysqlQueries();

