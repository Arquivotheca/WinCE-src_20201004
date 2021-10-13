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
