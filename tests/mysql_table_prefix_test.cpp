#include "../GraySvr/CWorldStorageMySQLUtils.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
        void ExpectTrue( bool condition, const std::string & message )
        {
                if ( !condition )
                {
                        std::cerr << message << std::endl;
                        std::exit( 1 );
                }
        }

        void ExpectFalse( bool condition, const std::string & message )
        {
                ExpectTrue( !condition, message );
        }
}

int main()
{
        std::string normalized;
        std::string errorMessage;

        bool valid = NormalizeMySQLTablePrefix( "  valid_prefix  ", normalized, &errorMessage );
        ExpectTrue( valid, "Expected ASCII prefix to validate successfully." );
        ExpectTrue( errorMessage.empty(), "Valid prefix should not produce an error message." );
        ExpectTrue( normalized == "valid_prefix", "Valid prefix should be trimmed before use." );

        normalized.clear();
        errorMessage.clear();
        bool invalid = NormalizeMySQLTablePrefix( "préfixe", normalized, &errorMessage );
        ExpectFalse( invalid, "Non-ASCII prefix must fail validation." );
        ExpectTrue( normalized.empty(), "Normalized prefix should be cleared when validation fails." );
        ExpectTrue( !errorMessage.empty(), "Validation failure must produce an error message." );
        ExpectTrue( errorMessage.find( "invalid characters" ) != std::string::npos, "Error message should describe the invalid prefix." );
        ExpectTrue( errorMessage.find( "ASCII letters, digits, and '_'" ) != std::string::npos, "Validation error should explain the allowed characters." );
        ExpectTrue( errorMessage.find( "\\x" ) != std::string::npos, "Non-ASCII bytes should be escaped in the error message." );

        normalized.clear();
        errorMessage.clear();
        bool sanitized = NormalizeMySQLTablePrefix( "préfixe", normalized, &errorMessage, true );
        ExpectFalse( sanitized, "Invalid bytes should be stripped even when normalization succeeds." );
        ExpectTrue( normalized == "prfixe", "Invalid UTF-8 bytes must be removed when sanitizing the prefix." );
        ExpectTrue( !errorMessage.empty(), "Sanitizing an invalid prefix must provide a warning message." );
        ExpectTrue( errorMessage.find( "Using normalized prefix 'prfixe'" ) != std::string::npos, "Sanitization message should describe the replacement prefix." );

        normalized.clear();
        errorMessage.clear();
        bool highBitInvalid = NormalizeMySQLTablePrefix( "\xDD\xDD", normalized, &errorMessage );
        ExpectFalse( highBitInvalid, "High-bit bytes must fail validation regardless of locale." );
        ExpectTrue( normalized.empty(), "Normalized prefix should be cleared for high-bit bytes." );
        ExpectTrue( !errorMessage.empty(), "High-bit validation failures must produce an error message." );
        ExpectTrue( errorMessage.find( "\\xDD\\xDD" ) != std::string::npos, "Each invalid byte should be escaped in the error message." );

        normalized.clear();
        errorMessage.clear();
        bool stripped = NormalizeMySQLTablePrefix( "\xDD\xDD", normalized, &errorMessage, true );
        ExpectFalse( stripped, "Sanitizing an all-invalid prefix should still flag the configuration." );
        ExpectTrue( normalized.empty(), "All invalid bytes must result in an empty sanitized prefix." );
        ExpectTrue( !errorMessage.empty(), "Sanitizing invalid bytes should produce a diagnostic message." );
        ExpectTrue( errorMessage.find( "Ignoring configured prefix" ) != std::string::npos, "Sanitization warning should mention the prefix being ignored." );

        std::string prefixedName;
        errorMessage.clear();
        bool prefixedValid = BuildMySQLPrefixedTableName( "  pre_  ", "schema_version", prefixedName, &errorMessage );
        ExpectTrue( prefixedValid, "Building a prefixed table name with a valid ASCII prefix should succeed." );
        ExpectTrue( errorMessage.empty(), "Valid prefix should not report an error when building the table name." );
        ExpectTrue( prefixedName == "pre_schema_version", "The normalized prefix must be applied to the table name." );

        prefixedName.clear();
        errorMessage.clear();
        bool prefixedInvalid = BuildMySQLPrefixedTableName( "\xDD\xDD", "schema_version", prefixedName, &errorMessage );
        ExpectFalse( prefixedInvalid, "Invalid prefixes must be rejected when building table names." );
        ExpectTrue( prefixedName.empty(), "Prefixed table name should be cleared when validation fails." );
        ExpectTrue( errorMessage.find( "\\xDD\\xDD" ) != std::string::npos, "Sanitized invalid bytes must be reported for invalid prefixes." );

        return 0;
}

