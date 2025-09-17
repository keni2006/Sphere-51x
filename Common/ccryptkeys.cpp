#include "graycom.h"
#include "grayproto.h"
#include "ccryptkeys.h"

#include <ctype.h>
#include <stdlib.h>

namespace
{
        struct DefaultKeyEntry
        {
                DWORD m_uiClientVersion;
                DWORD m_MasterHi;
                DWORD m_MasterLo;
        };

        const DefaultKeyEntry k_DefaultKeys[] =
        {
                { 20000, CLIKEY_20000_HI, CLIKEY_20000_LO },
                { 12604, CLIKEY_12604_HI, CLIKEY_12604_LO },
                { 12603, CLIKEY_12603_HI, CLIKEY_12603_LO },
                { 12602, CLIKEY_12602_HI, CLIKEY_12602_LO },
                { 12601, CLIKEY_12601_HI, CLIKEY_12601_LO },
                { 12600, CLIKEY_12600_HI, CLIKEY_12600_LO },
                { 12537, CLIKEY_12537_HI, CLIKEY_12537_LO },
                { 12536, CLIKEY_12536_HI1, CLIKEY_12536_LO1 },
                { 12535, CLIKEY_12535_HI, CLIKEY_12535_LO },
                { 12534, CLIKEY_12534_HI, CLIKEY_12534_LO },
                { 12533, CLIKEY_12533_HI, CLIKEY_12533_LO },
                { 12532, CLIKEY_12532_HI, CLIKEY_12532_LO },
                { 12531, CLIKEY_12531_HI, CLIKEY_12531_LO },
        };
}

CCryptKeysManager & CCryptKeysManager::GetInstance()
{
        static CCryptKeysManager s_Instance;
        return s_Instance;
}

CCryptKeysManager::CCryptKeysManager()
{
        LoadDefaults();
}

void CCryptKeysManager::LoadDefaults()
{
        m_clientKeys.RemoveAll();
        AddNoCryptKey();
        for ( size_t i = 0; i < COUNTOF(k_DefaultKeys); ++i )
        {
                CCryptClientKey key;
                key.m_uiClientVersion = k_DefaultKeys[i].m_uiClientVersion;
                key.m_MasterHi = k_DefaultKeys[i].m_MasterHi;
                key.m_MasterLo = k_DefaultKeys[i].m_MasterLo;
                key.m_EncType = CRYPT_LOGIN;
                AddKey(key);
        }
}

void CCryptKeysManager::ResetToDefaults()
{
        LoadDefaults();
}

void CCryptKeysManager::AddNoCryptKey()
{
        CCryptClientKey key;
        key.m_uiClientVersion = 0;
        key.m_MasterHi = 0;
        key.m_MasterLo = 0;
        key.m_EncType = CRYPT_NONE;
        m_clientKeys.Add(key);
}

void CCryptKeysManager::AddKey(CCryptClientKey key)
{
        for ( int i = 0; i < m_clientKeys.GetCount(); ++i )
        {
                if ( m_clientKeys[i].m_uiClientVersion == key.m_uiClientVersion )
                {
                        m_clientKeys.ElementAt(i) = key;
                        return;
                }
        }
        m_clientKeys.Add(key);
}

bool CCryptKeysManager::FindKeyForVersion(DWORD uiVersion, CCryptClientKey & outKey) const
{
        for ( int i = 0; i < m_clientKeys.GetCount(); ++i )
        {
                const CCryptClientKey & key = m_clientKeys.ElementAt(i);
                if ( key.m_uiClientVersion == uiVersion )
                {
                        outKey = key;
                        return true;
                }
        }
        return false;
}

const CCryptClientKey * CCryptKeysManager::GetKey(size_t index) const
{
        if ( index >= (size_t)m_clientKeys.GetCount() )
                return NULL;
        return &m_clientKeys.ElementAt(static_cast<int>(index));
}

size_t CCryptKeysManager::GetKeyCount() const
{
        return static_cast<size_t>(m_clientKeys.GetCount());
}

CRYPT_TYPE CCryptKeysManager::ParseEncryptionType(const TCHAR * pszName) const
{
        if ( pszName == NULL || pszName[0] == '\0' )
                return CRYPT_NONE;

        if ( isdigit(static_cast<unsigned char>(pszName[0])) )
        {
                long iVal = strtol(pszName, NULL, 0);
                if ( iVal < 0 )
                        return CRYPT_NONE;
                if ( iVal >= CRYPT_QTY )
                        return CRYPT_NONE;
                return static_cast<CRYPT_TYPE>(iVal);
        }

        if ( !strcmpi(pszName, "ENC_NONE") )
                return CRYPT_NONE;
        if ( !strcmpi(pszName, "ENC_BFISH") )
                return CRYPT_BFISH;
        if ( !strcmpi(pszName, "ENC_BTFISH") )
                return CRYPT_BTFISH;
        if ( !strcmpi(pszName, "ENC_TFISH") )
                return CRYPT_TFISH;
        if ( !strcmpi(pszName, "ENC_LOGIN") )
                return CRYPT_LOGIN;

        return CRYPT_NONE;
}

bool CCryptKeysManager::LoadKeyTable(CScript & script)
{
        bool fLoadedAny = false;

        m_clientKeys.RemoveAll();
        AddNoCryptKey();

        while ( script.FindNextSection())
        {
                if ( script.IsSectionType("SPHERECRYPT") )
                {
                        while ( script.ReadKeyParse())
                        {
                                if ( ! script.HasArgs())
                                        continue;

                                const TCHAR * pszVersion = script.GetKey();
                                if ( pszVersion == NULL || pszVersion[0] == '\0' )
                                        continue;

                                long iVersion = strtol(pszVersion, NULL, 10);
                                if ( iVersion <= 0 )
                                        continue;

                                TCHAR * pszArgs = script.GetArgStr();
                                if ( pszArgs == NULL || pszArgs[0] == '\0' )
                                        continue;

                                TCHAR * ppArgs[4];
                                int iArgCount = ParseCmds( pszArgs, ppArgs, COUNTOF(ppArgs) );
                                if ( iArgCount < 2 )
                                        continue;

                                CCryptClientKey key;
                                key.m_uiClientVersion = static_cast<DWORD>(iVersion);
                                key.m_MasterHi = strtoul( ppArgs[0], NULL, 16 );
                                key.m_MasterLo = strtoul( ppArgs[1], NULL, 16 );
                                key.m_EncType = ( iArgCount >= 3 ) ? ParseEncryptionType( ppArgs[2] ) : CRYPT_LOGIN;

                                AddKey( key );
                                fLoadedAny = true;
                        }
                }
        }

        if ( !fLoadedAny )
        {
                LoadDefaults();
                return false;
        }

        return true;
}
