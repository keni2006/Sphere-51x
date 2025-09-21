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
bool NormalizeMySQLTablePrefix( const char * rawPrefix, std::string & normalizedPrefix, std::string * outErrorMessage = nullptr );

