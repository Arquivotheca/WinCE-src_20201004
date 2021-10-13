//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:    soapstream.h
// 
// Contents:
//
//  Header File for encoding stream
//
//-----------------------------------------------------------------------------
#ifndef __SOAPENCSTREAM_H__
#define __SOAPENCSTREAM_H__

class CStreamShell : public CSoapShell
{
public:
    CStreamShell();
    ~CStreamShell();

    HRESULT IsInitialized();
    HRESULT write(BYTE * pBuffer, ULONG ulLen); 
    HRESULT setExtension(WCHAR *pcExtension, VARIANT *pvar, BOOL * pbSuccess);
    HRESULT setCharset(BSTR bstrCharset);
    HRESULT flush(void);
    void clear();
    
    HRESULT InitStream(IStream * pIStream);
    HRESULT InitResponse (IResponse * pIResponse);
    HRESULT setFaultCode(void);        

private:
    CAutoRefc<IStream>      m_pIStream;
    CAutoRefc<IResponse>    m_pIResponse;
    SAFEARRAY              *m_psa;
    DWORD                   m_dwBuffered;

    HRESULT WriteResponse(BYTE * pBuffer, ULONG bytesTotal);
};



#endif
