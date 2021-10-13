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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include "av_upnp.h"

using namespace av_upnp;

namespace av_upnp
{
    LPCWSTR AVDCPListDelimiter = L",";
    LPCWSTR AVDCPListDelimiterEscape  = L"\\";

    DWORD EscapeAVDCPListDelimiters(wstring* pstr)
    {
        if(!pstr)
            return ERROR_AV_POINTER;

        for(wstring::size_type n = 0; wstring::npos != (n = pstr->find(AVDCPListDelimiter, n)); n += 2)
        {
            if(!pstr->insert(n, AVDCPListDelimiterEscape))
                return ERROR_AV_OOM;
        }

        return SUCCESS_AV;
    }
}
