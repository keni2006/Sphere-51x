#include "Database.h"

#include <utility>

namespace Storage
{
        DatabaseError::DatabaseError( std::string context, unsigned int code, std::string message ) :
                std::runtime_error( message.empty() ? std::string( "Unknown database error" ) : std::move( message ) ),
                m_Context( std::move( context ) ),
                m_Code( code )
        {
        }
}

