#ifndef _INC_CCRYPTKEYS_H
#define _INC_CCRYPTKEYS_H
#if _MSC_VER >= 1000
#pragma once
#endif

#include "common.h"
#include "carray.h"
#include "cscript.h"

// Encryption types supported by SphereCrypt.ini.
enum CRYPT_TYPE
{
        CRYPT_NONE = 0,
        CRYPT_BFISH,
        CRYPT_BTFISH,
        CRYPT_TFISH,
        CRYPT_LOGIN,
        CRYPT_QTY
};

struct CCryptClientKey
{
        DWORD m_uiClientVersion;
        DWORD m_MasterHi;
        DWORD m_MasterLo;
        CRYPT_TYPE m_EncType;
};

class CCryptKeysManager
{
public:
        static CCryptKeysManager & GetInstance();

        void ResetToDefaults();
        void AddKey(const CCryptClientKey & key);
        bool FindKeyForVersion(DWORD uiVersion, CCryptClientKey & outKey) const;
        const CCryptClientKey * GetKey(size_t index) const;
        size_t GetKeyCount() const;
        bool LoadKeyTable(CScript & script);

private:
        CCryptKeysManager();

        void LoadDefaults();
        void AddNoCryptKey();
        CRYPT_TYPE ParseEncryptionType(const TCHAR * pszName) const;

        CGTypedArray<CCryptClientKey, CCryptClientKey&> m_clientKeys;
};

#endif // _INC_CCRYPTKEYS_H
