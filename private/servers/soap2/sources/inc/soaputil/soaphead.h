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
//+---------------------------------------------------------------------------------
//
//
// File:
//      soaphead.h
//
// Contents:
//
//      utility routines for soap header processing
//
//----------------------------------------------------------------------------------

#ifndef __SOAPHEAD_H_INCLUDED__
#define __SOAPHEAD_H_INCLUDED__

HRESULT _WSDLUtilFindAttribute(IXMLDOMNode *pNode, const TCHAR *pchAttributeToFind, BSTR *pbstrValue);

class CSoapHeaders
{
    public:
        CSoapHeaders();
        ~CSoapHeaders();

        HRESULT Init(void);
        HRESULT AddHeaderHandler(IHeaderHandler *pHeaderHandler);
        HRESULT ReadHeaders(ISoapReader *pReader, IDispatch *pObject);
        HRESULT WriteHeaders(ISoapSerializer *pSerializer, IDispatch *pObject);

    protected:
        CAutoRefc<IHeaderHandler> m_pHeaderHandler;
        
};

#endif  // __SOAPHEAD_H_INCLUDED__

