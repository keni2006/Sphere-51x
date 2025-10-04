#include "graycom.h"

class CLog : public CEventLog
{
public:
        CLog() : m_pContext( nullptr ) {}

        CScript * SetScriptContext( CScript * context )
        {
                CScript * previous = m_pContext;
                m_pContext = context;
                return previous;
        }

private:
        CScript * m_pContext;
};

CLog g_Log;
CEventLog * g_pLog = &g_Log;

class CServer
{
public:
        CGString m_sWorldBaseDir;
        CGString m_sSCPBaseDir;
};

CServer g_Serv;

void Assert_CheckFail( const char *, const char *, unsigned int )
{
        // Assertions are ignored in unit tests.
}
