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

#ifndef __FSDMAPINCLUDES_H__
#define __FSDMAPINCLUDES_H__

struct lt_compare
{
    bool operator () (const wchar_t *s1, const wchar_t *s2) const 
    {
        bool bRet = false;
        bRet = _wcsicmp(s1, s2) < 0;
        return bRet;
    }
};

#endif // __FSDMAPINCLUDES_H__