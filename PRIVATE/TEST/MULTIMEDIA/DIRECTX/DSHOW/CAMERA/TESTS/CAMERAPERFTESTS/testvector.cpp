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
#include "stdafx.h"
#include "testvector.h"

cTestVector::cTestVector() :CParser()
{
    m_dwCurrentVector = NULL;
}

cTestVector::~cTestVector()
{
    if(m_dwCurrentVector)
    {
        delete[] m_dwCurrentVector;
        m_dwCurrentVector = NULL;
    }
}

HRESULT
cTestVector::Init(WCHAR *wzFileName)
{
    HRESULT hr = S_OK;

    if(FAILED(hr = LoadIniFile(wzFileName)))
        return hr;

    // verify we have a parameter count (non-empty list)
    if(m_list.dwParameterCount <= 0)
        return E_FAIL;

    // verify it's a valid set of params
    for(DWORD i = 0; i < m_list.dwParameterCount; i++)
    {
        if(m_list.pParameters[i].dwCount < 1)
        {
            Cleanup();
            return E_FAIL;
        }
    }

    // create our vector list for tracking where we are in each parameter.
    m_dwCurrentVector = new DWORD[m_list.dwParameterCount];
    if(m_dwCurrentVector)
    {
        // start out with a vector of 0
        memset(m_dwCurrentVector, 0, sizeof(DWORD) * m_list.dwParameterCount);
    }
    else
    {
        Cleanup();
        return E_FAIL;
    }

    return S_OK;
}

cTestVector
&cTestVector::operator++( )
{
    // if the list isn't there, or the vector isn't set up, then we can't do anything.
    if(m_list.dwParameterCount <= 0 || !m_dwCurrentVector)
        return *this;

    for(DWORD i = 0; i < m_list.dwParameterCount; i++)
    {
        // try incrementing entry i
        // if entry i >= max entry index count
        if(++m_dwCurrentVector[i] >= m_list.pParameters[i].dwCount)
            // set entry i to 0 and try entry i+1
            m_dwCurrentVector[i] = 0;
        else
            // else return
            break;
    }

    return *this;
}

int
cTestVector::IterationCount()
{
    // verify we have a parameter count (non-empty list)
    if(m_list.dwParameterCount <= 0)
        return 0;

    int nAccumlate = m_list.pParameters[0].dwCount;

    for(DWORD i = 1; i < m_list.dwParameterCount; i++)
    {
        nAccumlate *= m_list.pParameters[i].dwCount;
    }

    return nAccumlate;
}

HRESULT
cTestVector::GetEntry(DWORD Index, WCHAR **Name, WCHAR **Recipient, double *Value)
{
    if(!m_dwCurrentVector || m_list.dwParameterCount <= 0)
        return E_FAIL;

    // if the requested index is invalid, then fail the call.
    if(Index >= m_list.dwParameterCount)
        return E_FAIL;

    *Name = m_list.pParameters[Index].wzName;
    *Recipient = m_list.pParameters[Index].wzRecipient;
    *Value = m_list.pParameters[Index].pValues[m_dwCurrentVector[Index]];

    return S_OK;
}

int
cTestVector::FindEntryIndex(DWORD StartIndex, WCHAR *Name, WCHAR *Recipient, DWORD *FoundIndex, double *Value)
{
    int nFound = 0;

    if(!FoundIndex)
        return -1;

    *FoundIndex = -1;

    // if the list is empty, or we're past the end, then it's there's nothing in the list.
    if(m_list.dwParameterCount <= 0 || StartIndex >= m_list.dwParameterCount)
        return 0;

    for(DWORD i = StartIndex; i < m_list.dwParameterCount; i++)
    {
        BOOL bNameMatch = FALSE;
        BOOL bRecipientMatch = FALSE;

        if(0 == wcscmp(L"*", Name) ||
            0 == wcscmp(m_list.pParameters[i].wzName, Name) )
        {
            bNameMatch = TRUE;
        }

        if(0 == wcscmp(L"*", Recipient) ||
            0 == wcscmp(m_list.pParameters[i].wzRecipient, Recipient) )
        {
            bRecipientMatch = TRUE;
        }

        if(bNameMatch && bRecipientMatch)
        {
            if(nFound == 0)
            {
                *FoundIndex = i;

                if(Value)
                    *Value = m_list.pParameters[i].pValues[m_dwCurrentVector[i]];
            }
            nFound++;
        }
    }

    return nFound;
}

HRESULT
cTestVector::Cleanup()
{
    if(m_dwCurrentVector)
    {
        delete[] m_dwCurrentVector;
        m_dwCurrentVector = NULL;
    }

    return S_OK;
}

