#pragma once

#include "cfile.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

class CMemoryScriptStream : public IScriptTextStream
{
public:
        CMemoryScriptStream() : m_Buffer(), m_Position( 0 ) {}
        explicit CMemoryScriptStream( const std::string & text ) : m_Buffer( text ), m_Position( 0 ) {}
        explicit CMemoryScriptStream( const CGString & text ) : m_Buffer( (const char *) text ), m_Position( 0 ) {}

        void SetBuffer( const std::string & text )
        {
                m_Buffer = text;
                m_Position = 0;
        }

        void SetBuffer( const CGString & text )
        {
                SetBuffer( std::string( (const char *) text ));
        }

        void Clear()
        {
                m_Buffer.clear();
                m_Position = 0;
        }

        const std::string & GetBuffer() const
        {
                return m_Buffer;
        }

        std::string & GetBuffer()
        {
                return m_Buffer;
        }

        size_t GetPosition() const
        {
                return m_Position;
        }

        virtual TCHAR * ReadLine( TCHAR FAR * pBuffer, size_t sizemax ) override
        {
                if ( pBuffer == NULL || sizemax == 0 )
                        return NULL;
                if ( m_Position >= m_Buffer.size())
                        return NULL;

                size_t maxCopy = ( sizemax > 0 ) ? ( sizemax - 1 ) : 0;
                size_t copied = 0;
                while (( copied < maxCopy ) && ( m_Position < m_Buffer.size()))
                {
                        char ch = m_Buffer[m_Position++];
                        pBuffer[copied++] = ch;
                        if ( ch == '\n' )
                                break;
                }
                pBuffer[copied] = '\0';
                return pBuffer;
        }

        virtual bool Write( const void FAR * pData, size_t iLen ) override
        {
                if ( pData == NULL )
                        return false;
                const char * source = static_cast<const char *>( pData );
                if ( iLen == 0 )
                        return true;

                if ( m_Position > m_Buffer.size())
                {
                        m_Buffer.resize( m_Position, '\0' );
                }

                size_t required = m_Position + iLen;
                if ( required > m_Buffer.size())
                {
                        m_Buffer.resize( required );
                }
                std::memcpy( &m_Buffer[m_Position], source, iLen );
                m_Position += iLen;
                return true;
        }

        virtual bool Seek( long offset = 0, int origin = SEEK_SET ) override
        {
                long long base = 0;
                switch ( origin )
                {
                case SEEK_SET:
                        base = 0;
                        break;
                case SEEK_CUR:
                        base = static_cast<long long>( m_Position );
                        break;
                case SEEK_END:
                        base = static_cast<long long>( m_Buffer.size());
                        break;
                default:
                        return false;
                }

                long long desired = base + offset;
                if ( desired < 0 )
                        return false;

                size_t newPos = static_cast<size_t>( desired );
                if ( newPos > m_Buffer.size())
                        newPos = m_Buffer.size();
                m_Position = newPos;
                return true;
        }

private:
        std::string m_Buffer;
        size_t m_Position;
};

