//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

