#pragma once

#ifndef _INC_CSTRING_H
#define _INC_CSTRING_H

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <string>

#ifndef TCHAR
#define TCHAR char
#endif
#ifndef LPCTSTR
#define LPCTSTR const char *
#endif
#ifndef WORD
#define WORD unsigned short
#endif
#ifndef UINT
#define UINT unsigned int
#endif
#ifndef DWORD
#define DWORD unsigned long
#endif

class CGString
{
public:
        CGString() = default;
        CGString( const CGString & ) = default;
        CGString( const TCHAR * value )
        {
                Copy( value );
        }

        CGString & operator=( const CGString & ) = default;

        const CGString & operator=( const TCHAR * value )
        {
                Copy( value );
                return *this;
        }

        void Copy( const TCHAR * value )
        {
                if ( value != nullptr )
                {
                        m_Value = value;
                }
                else
                {
                        m_Value.clear();
                }
        }

        void Empty()
        {
                m_Value.clear();
        }

        bool IsEmpty() const
        {
                return m_Value.empty();
        }

        bool IsValid() const
        {
                return true;
        }

        int GetLength() const
        {
                return static_cast<int>( m_Value.size());
        }

        const TCHAR * GetPtr() const
        {
                return m_Value.c_str();
        }

        operator const TCHAR *() const
        {
                return m_Value.c_str();
        }

        TCHAR * GetBuffer()
        {
                return m_Value.empty() ? const_cast<TCHAR*>( "" ) : m_Value.data();
        }

        TCHAR * GetBuffer( int minLength )
        {
                if ( minLength > GetLength())
                {
                        m_Value.resize( minLength );
                }
                return GetBuffer();
        }

        int SetLength( int length )
        {
                if ( length < 0 )
                {
                        length = 0;
                }
                m_Value.resize( static_cast<size_t>( length ));
                return length;
        }

        TCHAR GetAt( int index ) const
        {
                if ( index < 0 || index >= GetLength())
                {
                        return '\0';
                }
                return m_Value[static_cast<size_t>( index )];
        }

        void SetAt( int index, TCHAR ch )
        {
                if ( index < 0 || index >= GetLength())
                {
                        return;
                }
                m_Value[static_cast<size_t>( index )] = ch;
        }

        TCHAR & operator[]( int index )
        {
                if ( index < 0 )
                {
                        index = 0;
                }
                if ( static_cast<size_t>( index ) >= m_Value.size())
                {
                        m_Value.resize( static_cast<size_t>( index ) + 1 );
                }
                return m_Value[static_cast<size_t>( index )];
        }

        const CGString & operator+=( const TCHAR * value )
        {
                if ( value != nullptr )
                {
                        m_Value += value;
                }
                return *this;
        }

        const CGString & operator+=( TCHAR ch )
        {
                m_Value.push_back( ch );
                return *this;
        }

        int VFormat( const TCHAR * format, va_list args )
        {
                if ( format == nullptr )
                {
                        m_Value.clear();
                        return 0;
                }

                char buffer[4096];
                int written = std::vsnprintf( buffer, sizeof( buffer ), format, args );
                if ( written < 0 )
                {
                        m_Value.clear();
                        return 0;
                }

                m_Value.assign( buffer, static_cast<size_t>( written ));
                return written;
        }

        int Format( const TCHAR * format, ... )
        {
                va_list args;
                va_start( args, format );
                int written = VFormat( format, args );
                va_end( args );
                return written;
        }

        int FormatVal( long value )
        {
                return Format( "%ld", value );
        }

        int FormatHex( unsigned int value )
        {
                return Format( "0%x", value );
        }

        int Compare( const TCHAR * other ) const
        {
                if ( other == nullptr )
                {
                        other = "";
                }
                return std::strcmp( m_Value.c_str(), other );
        }

        int CompareNoCase( const TCHAR * other ) const
        {
                if ( other == nullptr )
                {
                        other = "";
                }
#ifdef _WIN32
                return _stricmp( m_Value.c_str(), other );
#else
                return ::strcasecmp( m_Value.c_str(), other );
#endif
        }

        void MakeUpper()
        {
                for ( char & ch : m_Value )
                {
                        ch = static_cast<char>( std::toupper( static_cast<unsigned char>( ch )));
                }
        }

private:
        std::string m_Value;
};


#endif // _INC_CSTRING_H
