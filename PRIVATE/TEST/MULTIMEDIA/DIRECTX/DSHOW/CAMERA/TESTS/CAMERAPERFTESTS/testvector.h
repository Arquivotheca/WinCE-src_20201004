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
#include "structs.h"
#include "parser.h"

#pragma once

typedef class cTestVector : private CParser
{
    public:
        cTestVector();
        ~cTestVector();

        HRESULT Init(WCHAR *wzFileName);
        cTestVector &operator++( );
        int IterationCount();
        HRESULT GetEntry(DWORD Index, WCHAR **Name, WCHAR **Recipient, double *Value);
        int FindEntryIndex(DWORD StartIndex, WCHAR *Name, WCHAR *Recipient, DWORD *FoundIndex, double *Value);
        HRESULT Cleanup();

    private:
        DWORD *m_dwCurrentVector;
}TESTVECTOR, *PTESTVECTOR;

