#pragma once

#include <string>

// Returns true when the provided token is a safe MariaDB identifier consisting of
// ASCII alphanumeric characters or '_'.
bool IsSafeMariaDbIdentifierToken( const std::string & token );

// Normalizes the configured table prefix by trimming ASCII whitespace and validating
// the remaining bytes. The normalized prefix is returned in \a normalizedPrefix when
// validation succeeds.
//
// When validation fails, the normalized prefix is cleared and an optional error
// message describing the failure is written to \a outErrorMessage.
bool NormalizeMySQLTablePrefix( const char * rawPrefix, std::string & normalizedPrefix, std::string * outErrorMessage = nullptr, bool stripInvalidCharacters = false );

// Builds a fully qualified table name by validating and applying the configured
// table prefix to the provided base table name. When validation fails the
// function returns false and, when supplied, \a outErrorMessage will contain the
// sanitized validation failure.
bool BuildMySQLPrefixedTableName( const char * rawPrefix, const char * tableName, std::string & outPrefixedTableName, std::string * outErrorMessage = nullptr );

