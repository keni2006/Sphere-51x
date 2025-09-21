#include "CWorldStorageMySQLUtils.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace
{
        void TrimAsciiWhitespace( std::string & value )
        {
                const char * whitespace = " \t\n\r\f\v";
                size_t begin = value.find_first_not_of( whitespace );
                if ( begin == std::string::npos )
                {
                        value.clear();
                        return;
                }

                size_t end = value.find_last_not_of( whitespace );
                value = value.substr( begin, end - begin + 1 );
        }

        std::string SanitizeIdentifierForLogging( const std::string & token )
        {
                std::string sanitized;
                sanitized.reserve( token.size());

                for ( unsigned char ch : token )
                {
                        if ( ch >= 0x20 && ch <= 0x7e )
                        {
                                sanitized.push_back( static_cast<char>( ch ));
                        }
                        else
                        {
                                char buffer[5];
                                std::snprintf( buffer, sizeof( buffer ), "\\x%02X", ch );
                                sanitized.append( buffer );
                        }
                }

                return sanitized;
        }
}

bool IsSafeMariaDbIdentifierToken( const std::string & token )
{
        if ( token.empty())
        {
                return false;
        }

        for ( char ch : token )
        {
                const unsigned char uch = static_cast<unsigned char>( ch );
                const bool isAsciiLetter = ( uch >= 'A' && uch <= 'Z' ) || ( uch >= 'a' && uch <= 'z' );
                const bool isAsciiDigit = ( uch >= '0' && uch <= '9' );
                if ( !isAsciiLetter && !isAsciiDigit && uch != '_' )
                {
                        return false;
                }
        }

        return true;
}

bool NormalizeMySQLTablePrefix( const char * rawPrefix, std::string & normalizedPrefix, std::string * outErrorMessage )
{
        normalizedPrefix.clear();

        std::string working = rawPrefix != nullptr ? std::string( rawPrefix ) : std::string();
        TrimAsciiWhitespace( working );

        normalizedPrefix = working;
        if ( normalizedPrefix.empty())
        {
                if ( outErrorMessage != nullptr )
                {
                        outErrorMessage->clear();
                }
                return true;
        }

        if ( !IsSafeMariaDbIdentifierToken( normalizedPrefix ))
        {
                if ( outErrorMessage != nullptr )
                {
                        const std::string sanitized = SanitizeIdentifierForLogging( normalizedPrefix );
                        *outErrorMessage = "MySQL table prefix '" + sanitized + "' contains invalid characters; only ASCII letters, digits, and '_' are allowed.";
                }

                normalizedPrefix.clear();
                return false;
        }

        if ( outErrorMessage != nullptr )
        {
                outErrorMessage->clear();
        }
        return true;
}

