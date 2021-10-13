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
#include "prettyprint.hxx"
#include "log.h"

PrintTuple::PrintTuple(UINT cbSize, float fMaxPerfInMbps, UINT uiNumPackets)
{
    m_cbSize = cbSize;
    m_fMaxPerfInMbps = fMaxPerfInMbps;
    m_uiNumPackets = uiNumPackets;
}


void PrintTuple::PrintHeaders()
{
    TCHAR szHeaders[256];
    StringCchPrintf(szHeaders, _countof(szHeaders), _T("||%19s||%18s||%13s||"), _T("Size in Bytes"), _T("Perf in Mbps"), _T("Packets"));
    LogMsg(szHeaders);
}

void PrintTuple::Print()
{
    TCHAR szTuple[256];
    StringCchPrintf(szTuple, _countof(szTuple),_T("||%19d||%18.2f||%13d||"), m_cbSize, m_fMaxPerfInMbps, m_uiNumPackets);
    LogMsg(szTuple);
}

void PrettyPrint::AddTuple(const PrintTuple & printTuple)
{
    printTuples.push_back(printTuple);
}

void PrettyPrint::Print()
{
    PrintTuple::PrintHeaders();
    for(ce::vector<PrintTuple>::iterator it = printTuples.begin(), itEnd = printTuples.end(); it != itEnd; ++it)
    {
        it->Print();
    }
}