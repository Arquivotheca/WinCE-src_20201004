//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  XmlDocument.h
//
//  Sample Xml
//            <Root>
//                <TestCase Action="Run" />
//                <TestCase Action="Verify" />
//            </Root>
//
//
//  Sample usage:
//
//            HRESULT hr = S_OK;
//            XmlDocument doc;
//            XmlElementList* pTestCases = NULL;
//           
//            hr= doc.Load(_T("TestCases.xml"));
//            if (SUCCEEDED(hr))
//            {
//                //
//                // Get all the xml elements with the tag "TestCase"
//                //
//                
//                hr = doc.GetElementsByTagName(
//                    _T("TestCase"), 
//                    &pTestCases);
//
//                if (SUCCEEDED(hr))
//                {
//                    //
//                    // Iterate through the list of "TestCase" elements
//                    //
//                    
//                    XmlElement* pTest = NULL;
//                    while (SUCCEEDED(hr = pTestCases->GetNext(&pTest))
//                      && pTest !=NULL)
//                    {
//                        //
//                        // Foreach "TestCase" element get the "Action" attribute
//                        //
//                        TCHAR actionValue[MAX_PATH] = {0};
//                        hr = pTest->GetAttributeValue(_T("Action"),&actionValue[0], MAX_PATH);
//
//                        delete pTest;
//                        pTest = NULL;
//                        if (FAILED(hr))
//                        {
//                            break;
//                        }
//                    }
//                    delete pTestCases;
//                    pTestCases = NULL;
//                }
//            }
//            
//
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "precomm.h"
#include "XmlElement.h"

class XmlDocument
{
public:
    XmlDocument();
    ~XmlDocument();

    HRESULT GetDocumentElement(
        XmlElement** ppDocumentElement);

    HRESULT GetElementsByTagName(
        __in PTSTR name,
        XmlElementList** ppElementList);

    HRESULT Load(
        __in PTSTR fileName);
        
    HRESULT LoadXml(
        __in PTSTR xmlString);

    HRESULT Save(
        __in PTSTR fileName);
    
private:
    HRESULT Initialize();
    HRESULT ParseLoadError();

    XmlDocument(
        const XmlDocument& document);

    XmlDocument& operator=(  
        const XmlDocument& document);

private:
    IXMLDOMDocument* m_pDocument;
    BOOL m_coInitialized;
};

