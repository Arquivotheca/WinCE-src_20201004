//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once
#include <eh.h>

extern void SehTranslatorFunction(unsigned int, struct _EXCEPTION_POINTERS*);

class SehGuard
{
public:
    SehGuard()
    {
        m_prev = _set_se_translator(SehTranslatorFunction);
    }

    ~SehGuard()
    {
        _set_se_translator(m_prev);
    }

private:
    _se_translator_function m_prev;
};

class SehException
{
public:
    SehException(int code) : m_code(code) { }
    inline unsigned int GetCode() { return m_code; }

private:
    unsigned int m_code;
};

void SehTranslatorFunction(unsigned int code, struct _EXCEPTION_POINTERS*)
{
    ASSERT(false);
    throw SehException(code);
}
