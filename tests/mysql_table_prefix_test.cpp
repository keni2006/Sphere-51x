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
        bool highBitInvalid = NormalizeMySQLTablePrefix( "\xDD\xDD", normalized, &errorMessage );
        ExpectFalse( highBitInvalid, "High-bit bytes must fail validation regardless of locale." );
        ExpectTrue( normalized.empty(), "Normalized prefix should be cleared for high-bit bytes." );
        ExpectTrue( !errorMessage.empty(), "High-bit validation failures must produce an error message." );
        ExpectTrue( errorMessage.find( "\\xDD\\xDD" ) != std::string::npos, "Each invalid byte should be escaped in the error message." );

        return 0;
}

