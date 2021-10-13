//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
