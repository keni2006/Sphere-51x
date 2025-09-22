#pragma once

#ifndef COMMON_STUB_H
#define COMMON_STUB_H

#include "cstring_stub.h"

#include <cstdint>

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#ifndef UINT
#define UINT unsigned int
#endif

#ifndef ASSERT
#define ASSERT(expr) (void)(expr)
#endif

#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

enum LOGL_TYPE : unsigned short
{
        LOGL_FATAL = 1,
        LOGL_CRIT = 2,
        LOGL_ERROR = 3,
        LOGL_WARN = 4,
        LOGL_EVENT = 5,
        LOGL_TRACE = 6,
};

#endif // COMMON_STUB_H
