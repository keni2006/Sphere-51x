#include "test_harness.h"

#include "graycom.h"
#include "cmemoryscriptstream.h"

#include <stdexcept>
#include <string>

namespace
{
        std::string ToString( const char * text )
        {
                return ( text != nullptr ) ? std::string( text ) : std::string();
        }
}

TEST_CASE( TestMemoryStreamFindNextSection )
{
        const char scriptText[] = "\xEF\xBB\xBF[First]\nKey=Value\n\n[Second]\nOther=42\n";
        CMemoryScriptStream stream{ std::string( scriptText ) };
        CScript script( &stream );

        if ( !script.FindNextSection())
        {
                throw std::runtime_error( "Expected to find first section" );
        }
        if ( ToString( script.GetKey()) != "First" )
        {
                throw std::runtime_error( "Incorrect name for first section" );
        }

        if ( !script.FindNextSection())
        {
                throw std::runtime_error( "Expected to find second section" );
        }
        if ( ToString( script.GetKey()) != "Second" )
        {
                throw std::runtime_error( "Incorrect name for second section" );
        }

        if ( script.FindNextSection())
        {
                throw std::runtime_error( "Unexpected additional section" );
        }
}

TEST_CASE( TestMemoryStreamWriteKey )
{
        CMemoryScriptStream stream;
        CScript script( &stream );

        const std::string unicodeValue = reinterpret_cast<const char *>( u8"значение" );
        if ( !script.WriteKey( "Unicode", unicodeValue.c_str()))
        {
                throw std::runtime_error( "WriteKey returned false" );
        }
        if ( !script.WriteKey( "Empty", "" ))
        {
                throw std::runtime_error( "WriteKey should support empty values" );
        }

        const std::string expected = std::string( "Unicode=" ) + unicodeValue + "\nEmpty\n";
        if ( stream.GetBuffer() != expected )
        {
                throw std::runtime_error( "Unexpected serialized script output" );
        }
}

TEST_CASE( TestMemoryStreamReadKeyParse )
{
        const char scriptText[] = "[Section]\nAlpha=1\nBeta = second\n\n";
        CMemoryScriptStream stream{ std::string( scriptText ) };
        CScript script( &stream );

        if ( !script.FindNextSection())
        {
                throw std::runtime_error( "Expected to find section header" );
        }

        if ( !script.ReadKeyParse())
        {
                throw std::runtime_error( "Expected to parse first key" );
        }
        if ( ToString( script.GetKey()) != "Alpha" )
        {
                throw std::runtime_error( "Unexpected first key name" );
        }
        if ( ToString( script.GetArgStr()) != "1" )
        {
                throw std::runtime_error( "Unexpected first key value" );
        }

        if ( !script.ReadKeyParse())
        {
                throw std::runtime_error( "Expected to parse second key" );
        }
        if ( ToString( script.GetKey()) != "Beta" )
        {
                throw std::runtime_error( "Unexpected second key name" );
        }
        const std::string secondValue = ToString( script.GetArgStr());
        if ( secondValue != "second" )
        {
                throw std::runtime_error( std::string( "Unexpected second key value: '" ) + secondValue + "'" );
        }

        if ( script.ReadKeyParse())
        {
                throw std::runtime_error( "ReadKeyParse should stop at end of section" );
        }
}
